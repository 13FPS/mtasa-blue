"""Audit saved Core-owned native renderer observations without operating GTA."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

try:
    from . import core_startup_log as startup
except ImportError:
    import core_startup_log as startup

PROVENANCE = "mta_core_native_render_observation"
INCOMPLETE_ERROR = "Error: MTA Core renderer observation incomplete; inspect its saved state"
PAIRS = dict(generation=15, native_present_callbacks=17, accepted_snapshots=19,
             rejected_snapshots=21, nested_callbacks=23, foreign_callbacks=33,
             publication_misses=35)
GPU_PAIRS = dict(attempts=44, accepted=46, rejected=48)
POINTER_KEYS = frozenset(("viewport", "swapchain", "device", "native_pointer", "address", "command_queue", "resource_pointer"))
UNVERIFIED_CLAIMS = frozenset(("com_present_verified", "visual_success_verified", "de_renderer_verified", "full_mta_boot_verified",
                              "full_core_ready", "gameplay_ready", "rendering_started", "engine_input_write_verified",
                              "remote_player_context_verified", "network_active", "input_writes", "remote_peds_created"))
NOTICE = (
    "This checks saved reports of an explicitly selected owned native viewport callback and cleanup. "
    "Callback entry and its native return value do not prove that COM Present executed or pixels were drawn. "
    "Visual quality, a DE GUI renderer, full MTA startup and multiplayer remain unverified."
)


def unsigned_array(values, count):
    return isinstance(values, list) and len(values) == count and all(startup.integer(v, 0xffffffff) for v in values)


def counters(state):
    return {name: state[index] | state[index + 1] << 32 for name, index in PAIRS.items()}


def event_contract(event):
    if type(event.get("core_api")) is not int:
        return None
    if event["core_api"] == 5 and "observation_api" not in event and "requested_route" not in event:
        return (1, 1)
    if (type(event.get("observation_api")) is int and type(event.get("requested_route")) is int and
            ((event["core_api"] == 6 and event["observation_api"] == 2 and event["requested_route"] in (1, 2)) or
             (event["core_api"] == 7 and event["observation_api"] == 3 and event["requested_route"] == 2))):
        return (event["observation_api"], event["requested_route"])
    return None


def route_consistent(state, version, requested):
    if version == 1:
        return True
    observed = state[41]
    known, presence = (15, 15) if requested == 1 else (19, 3)
    # API2 separates explicit admission from a returned natural callback. A
    # queue is not an immediate context, and unknown COM fields stay clear.
    return (state[40] == requested and observed in (0, requested) and
            state[42] == (known if observed else 0) and
            state[43] == int(bool(observed) and requested == 2) and
            state[37] == (presence if observed else 0) and
            (bool(state[19] or state[20]) == bool(observed)) and (not observed or state[31] == 1) and
            (bool(observed) or not any(state[25:30]) and state[31] == 0))


def gpu_eligible(state):
    # Passing this conservative metadata predicate does not prove queue ordering,
    # PRESENT resource state, resize ownership, or any submitted/drawn GUI frame.
    return bool(state[54] == 0 and state[57] <= 1 and state[58] == 1 and
                state[61] == 7 and 2 <= state[63] <= 16 and state[62] < state[63] and state[64] > 0 and
                0 < state[66] <= 32768 and 0 < state[67] <= 32768 and
                state[68] == state[66] and state[69] == 0 and state[70] == state[67] and state[71] == state[64] and
                state[72:75] == [1, 0, 0] and state[76:78] == [1, 1])


def gpu_supported(state):
    return state[50] == 19 and state[51:54] == [0, 1, 1] and state[75] == 1 and gpu_eligible(state)


def gpu_metrics(state):
    return {name: state[index] | state[index + 1] << 32 for name, index in GPU_PAIRS.items()}


def gpu_consistent(state, previous=None):
    metrics = gpu_metrics(state)
    if (metrics["attempts"] != metrics["accepted"] + metrics["rejected"] or
            metrics["attempts"] != counters(state)["accepted_snapshots"] or
            state[50] > 19 or state[52] > 1 or state[53] > 1 or state[61] > 7 or state[74] > 1 or
            state[75] > 6 or any(state[78:80])):
        return False
    if previous and any(metrics[key] < gpu_metrics(previous)[key] for key in GPU_PAIRS):
        return False
    if not metrics["attempts"]:
        return not any(state[46:80])
    if not state[50] or not state[75] or (state[53] and not gpu_supported(state)):
        return False
    if state[75] == 1:
        return gpu_supported(state) and metrics["accepted"] > 0
    if state[53] or not metrics["rejected"]:
        return False
    if state[75] == 2:
        return state[51] == 0 and (state[50] == 19 and not gpu_eligible(state) if state[52] else
                                  state[74] == 1 if state[50] == 2 else
                                  state[50] == 3 and not state[74] and (state[76] != 1 or state[77] != 1))
    if state[52]:
        return False
    if state[75] == 3:
        return (state[50] in (1, 3) or 4 <= state[50] <= 16) and state[51] >= 0x80000000
    if state[75] == 4:
        return 4 <= state[50] <= 16 and state[51] == 0x8000ffff
    if state[75] == 5:
        return state[50] == 17 and state[51] == 0
    return state[75] == 6 and state[50] == 18 and state[51] == 0x8000ffff


def unsafe_event_claims(event):
    # Inspect nested objects as well: moving a readiness flag or pointer below
    # `details` must not turn the same unsupported claim into accepted evidence.
    pending, visited = [event], 0
    while pending:
        value = pending.pop()
        visited += 1
        if visited > 16384:
            return True
        if isinstance(value, dict):
            if any(key in POINTER_KEYS or (key in UNVERIFIED_CLAIMS and item is not False) for key, item in value.items()):
                return True
            pending.extend(value.values())
        elif isinstance(value, list):
            pending.extend(value)
    return False


def analyze_log(text, require_exit=False):
    if len(text.encode("utf-8", errors="replace")) > startup.MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    translated, issues, samples = [], [], []
    started = finished = stop = deliberate_incomplete = None
    last_clock = None
    adapter_contract = None
    version, requested = 1, 1

    def invalid(line, reason):
        if len(issues) < 128:
            issues.append(dict(line=line, reason=reason))

    for line, raw in enumerate(text.splitlines(), 1):
        try:
            event = startup.logged_event(raw)
        except (RecursionError, OverflowError):
            invalid(line, "JSON exceeds the parser representation")
            continue
        if event is None:
            if "core_render_reference" in raw:
                invalid(line, "Malformed renderer observation marker")
            translated.append(raw)
            continue
        kind = event["event"]
        if unsafe_event_claims(event):
            invalid(line, "Native pointers or visual/COM readiness claims exceed this scalar diagnostic")
        if kind.startswith(("core_startup_reference", "core_input_reference", "core_movement_reference")):
            invalid(line, "Another Core reference cannot accompany renderer observations")
        if kind == "adapter_start":
            adapter_contract = event_contract(event)
            if (event.get("core_render_reference") is not True or event.get("provenance") != PROVENANCE or
                    adapter_contract is None or
                    any(event.get(flag) is True for flag in ("core_input_reference", "core_movement_reference", "core_startup_reference"))):
                invalid(line, "Adapter must identify the isolated Core renderer observation")
            event = {**event, "core_startup_reference": True, "provenance": startup.PROVENANCE, "core_api": 2}
            event.pop("core_render_reference", None)
            event.pop("observation_api", None)
            event.pop("requested_route", None)
        elif kind == "core_render_reference_started":
            contract = event_contract(event)
            keys = {"event", "at_ms", "core_api", "sample_interval_ms", "budget_ms"}
            if contract and contract[0] >= 2:
                keys |= {"observation_api", "requested_route"}
            if (set(event) != keys or contract is None or contract != adapter_contract or
                    started or finished or
                    type(event.get("sample_interval_ms")) is not int or event["sample_interval_ms"] != 100 or
                    type(event.get("budget_ms")) is not int or event["budget_ms"] != 10000):
                invalid(line, "One explicit ten-second scalar observation contract is required")
            if contract:
                version, requested = contract
            started = dict(line=line, **event)
            event = dict(event="core_startup_reference_started", at_ms=event.get("at_ms"), core_api=2)
        elif kind == "core_render_reference_stop_requested":
            if stop:
                invalid(line, "Duplicate cancellation request")
            stop = dict(line=line, **event)
            event = {**event, "event": "core_startup_reference_stop_requested"}
        elif kind == "core_render_reference_sample":
            state = event.get("state")
            if (len(samples) >= 4096 or set(event) != {"event", "at_ms", "elapsed_ms", "state"} or
                    not started or finished or not startup.integer(event.get("at_ms")) or
                    not startup.integer(event.get("elapsed_ms")) or not startup.integer(started.get("at_ms")) or
                    event["at_ms"] - started["at_ms"] != event["elapsed_ms"] or not unsigned_array(state, {1: 40, 2: 44, 3: 80}[version])):
                invalid(line, "Sample requires the versioned unsigned fields with bound clock/order")
                continue
            if last_clock is not None and event["at_ms"] < last_clock:
                invalid(line, "Script sample clock reversed")
            last_clock = event["at_ms"]
            if state[3] > event["elapsed_ms"] + 250:
                invalid(line, "Native elapsed time exceeds its containing script interval")
            previous = samples[-1]["state"] if samples else None
            boolean_fields = (6, 8, 9, 10, 11, 12, 13, 14, 29, 31, 32)
            if (state[0] != version or state[1] not in (1, 2, 3) or not state[2] or state[4] > 8 or
                    state[5] > 201 or any(state[index] > 1 for index in boolean_fields) or
                    state[26] > 32768 or state[27] > 32768 or state[37] & ~15 or any(state[38:40]) or
                    not route_consistent(state, version, requested)):
                invalid(line, "Snapshot version, flags, field bounds or reserved storage is invalid")
            if version == 3 and not gpu_consistent(state, previous):
                invalid(line, "GPU ownership metadata or coherent published counters are contradictory")
            native = counters(state)
            outcomes = ("accepted_snapshots", "rejected_snapshots", "foreign_callbacks", "publication_misses")
            if (any(native[name] > native["native_present_callbacks"] for name in (*outcomes, "nested_callbacks")) or
                    sum(native[name] for name in outcomes) > native["native_present_callbacks"]):
                invalid(line, "Observed snapshots exceed native callback entries")
            if previous:
                prior = counters(previous)
                if (previous[1] == 2 or state[2] <= previous[2] or state[3] < previous[3] or state[5] < previous[5] or
                        state[7] < previous[7] or state[8] < previous[8] or state[9] < previous[9] or
                        (previous[30] and state[30] != previous[30]) or state[12] < previous[12] or
                        (prior["generation"] and native["generation"] != prior["generation"]) or
                        (version >= 2 and previous[41] and state[41] != previous[41]) or
                        any(native[name] < prior[name] for name in PAIRS if name != "generation")):
                    invalid(line, "Snapshot sequence, generation or counters changed inconsistently")
            if state[1] == 1 and (state[4] or state[6] or state[10]):
                invalid(line, "A running sample cannot claim terminal cleanup")
            if state[1] == 3 and (state[6] or state[10] != 1):
                invalid(line, "Pending cleanup must retain ownership without claiming completion")
            if state[1] == 2 and (state[6] != 1 or state[10] or state[13] or state[32]):
                invalid(line, "Terminal cleanup requires inactive, detached native observation")
            samples.append(dict(line=line, **event))
            continue
        elif kind == "core_render_reference_finished":
            keys = {"event", "at_ms", "elapsed_ms", "state", "run_result", "passed", "cleanup_passed", "stop_requested", "samples",
                    "full_core_ready", "rendering_started", "network_active", "input_writes", "remote_peds_created"}
            if (set(event) != keys or finished or not samples or samples[-1]["state"][1] != 2 or
                    type(event.get("samples")) is not int or event["samples"] != len(samples) or
                    not startup.integer(event.get("at_ms")) or event["at_ms"] < samples[-1]["at_ms"]):
                invalid(line, "One result must follow the terminal observation and preserve its sample count")
            finished = dict(line=line, **event)
            state = event.get("state")
            terminal = samples[-1]["state"] if samples else None
            if unsigned_array(state, 19):
                cleanup = bool(terminal and terminal[6] == 1 and not terminal[10] and not terminal[13] and not terminal[32] and
                               state[7] == 1 and state[13] == 1 and state[14] == 0 and state[15] == terminal[9])
                if event.get("cleanup_passed") is not cleanup:
                    invalid(line, "Cleanup must match the actual backend PIN and observer detachment")
                # Reuse the read-only startup lifecycle audit after checking
                # the observer's actual PIN. Keep original diagnostics in the
                # returned evidence; this normalization is never a DLL unload claim.
                state = state.copy()
                state[15] = 0
            event = {**event, "event": "core_startup_reference_finished", "state": state}
            event.pop("samples", None)
        elif kind == "adapter_failed":
            # The producer deliberately throws after reporting a clean but
            # unobserved result. Preserve that failure in the startup audit;
            # only this exact adjacent payload can qualify diagnostic cleanup.
            if (finished and finished.get("passed") is False and line == finished["line"] + 1 and
                    set(event) == {"event", "phase", "error", "game_adapter_active"} and
                    event.get("phase") == "core_render_reference" and event.get("error") == INCOMPLETE_ERROR and
                    event.get("game_adapter_active") is False and deliberate_incomplete is None):
                deliberate_incomplete = dict(line=line, **event)
        elif kind.startswith("core_render_reference_"):
            invalid(line, "Unknown renderer observation marker")
        translated.append(json.dumps(event, allow_nan=False))

    base = startup.analyze_log("\n".join(translated), require_exit)
    terminal = samples[-1]["state"] if samples else None
    native = counters(terminal) if terminal else None
    diagnostic_native_clean = bool(terminal and terminal[1] == 2 and 10000 <= terminal[3] <= 12000 and terminal[4] == 1 and
                                   terminal[5] > 0 and terminal[6] == 1 and terminal[7] > 0 and not terminal[8] and terminal[9] == 1 and
                                   not terminal[10] and terminal[11:15] == [1, 1, 0, 1] and native["generation"] > 0 and
                                   not any(native[name] for name in ("rejected_snapshots", "nested_callbacks", "foreign_callbacks")) and
                                   terminal[30] == terminal[32] == 0 and not any(terminal[38:40]))
    clean = bool(terminal and terminal[1] == 2 and 10000 <= terminal[3] <= 12000 and terminal[4] == 1 and
                 terminal[5] > 0 and terminal[6] == 1 and terminal[7] > 0 and not terminal[8] and terminal[9] == 1 and
                 not terminal[10] and terminal[11:15] == [1, 1, 0, 1] and native["generation"] > 0 and
                 native["native_present_callbacks"] > 0 and native["accepted_snapshots"] > 0 and
                 not any(native[name] for name in ("rejected_snapshots", "nested_callbacks", "foreign_callbacks")) and
                 terminal[25] > 0 and 0 < terminal[26] <= 32768 and 0 < terminal[27] <= 32768 and
                 terminal[29] <= 1 and terminal[30:35] == [0, 1, 0, 0, 0] and
                 terminal[37] == (15 if requested == 1 else 3) and not any(terminal[38:40]) and
                 (version == 1 or terminal[41] == requested))
    native_clean = clean
    gpu_clean = bool(version == 3 and terminal and gpu_supported(terminal) and
                     gpu_metrics(terminal)["accepted"] > 0 and not gpu_metrics(terminal)["rejected"])
    clean = clean and (version != 3 or gpu_clean)
    lifecycle = bool(base["core_startup_reference_complete"] and not issues and not stop)
    complete = lifecycle and clean
    if finished and finished.get("passed") is True and not clean:
        invalid(finished["line"], "A passed flag cannot replace native observations or cleanup proof")
        complete = False
    state = finished.get("state") if finished else None
    worker_clean = bool(unsigned_array(state, 19) and state[:11] == startup.EXPECTED_PREFIX and
                        state[11] <= 3 and state[12] <= 3 and not state[12] & ~state[11] and state[13:] == [1, 0, 1, 0, 1, 0])
    expected_failure_only = bool(deliberate_incomplete and len(base["failures"]) == 1 and
                                 {key: value for key, value in base["failures"][0].items() if key != "line"} ==
                                 {key: value for key, value in deliberate_incomplete.items() if key != "line"})
    diagnostic_flags = all(value for key, value in base["flags"].items() if key not in ("expected_async_result", "no_failures"))
    # A value-derived clean diagnostic lifecycle is independent of callback
    # acceptance. Never rewrite passed=False or hide an unrelated failure to
    # make the reused startup reader report success.
    diagnostic_lifecycle = bool(diagnostic_flags and not issues and not stop and diagnostic_native_clean and worker_clean and finished and
                                finished.get("run_result") == 1 and finished.get("cleanup_passed") is True and
                                finished.get("stop_requested") is False and
                                ((finished.get("passed") is True and not base["failures"]) or
                                 (finished.get("passed") is False and expected_failure_only)))
    return dict(schema_version=1, status=("native_gpu_ownership_observed" if version == 3 else "native_renderer_callback_observed") if complete else
                "invalid_evidence" if issues or base["issue_count"] else
                ("native_gpu_ownership_incomplete" if version == 3 and native_clean else "native_observation_missing") if diagnostic_lifecycle else "incomplete_or_failed",
                core_render_reference_complete=complete, render_observation_session_complete=lifecycle,
                diagnostic_lifecycle_complete=diagnostic_lifecycle, deliberate_incomplete_event=deliberate_incomplete,
                native_renderer_callback_observed=bool(native_clean and (lifecycle or diagnostic_lifecycle)), sample_count=len(samples), native_metrics=native,
                gpu_ownership_verified=bool(version == 3 and complete),
                gpu_metrics=gpu_metrics(terminal) if version == 3 and terminal else None,
                observation_api=version, requested_route=requested,
                observed_route=(terminal[41] if version >= 2 else 1 if complete else 0) if terminal else 0,
                presence_known_mask=(terminal[42] if version >= 2 else 15 if complete else 0) if terminal else 0,
                queue_present=bool(version >= 2 and terminal and terminal[43]),
                terminal_render_state=terminal, worker_state=finished.get("state") if finished else None,
                cleanup_pending=bool(terminal and (terminal[1] == 3 or terminal[10] or terminal[32])),
                issue_count=len(issues) + base["issue_count"], issues=issues, lifecycle=base,
                normal_exit_verified=base["normal_exit_verified"], require_exit=require_exit,
                visual_success_verified=False, com_present_verified=False, de_renderer_verified=False,
                full_mta_boot_verified=False, engine_input_write_verified=False, remote_player_context_verified=False,
                notice=NOTICE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--require-exit", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError("Output must differ from the raw evidence log")
        raw = startup.read_snapshot(args.log)
        result = analyze_log(raw["data"].decode("utf-8-sig", errors="replace"), args.require_exit)
        result.update(log=str(args.log.resolve()), evidence_scope="entire_log",
                      log_fingerprint={key: raw[key] for key in ("size_bytes", "mtime_ns", "sha256")},
                      reader_path=str(Path(__file__).resolve()), reader_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    except (OSError, ValueError) as error:
        result = dict(status="read_error", error=str(error), core_render_reference_complete=False)
        if "Output must differ" in str(error):
            print(json.dumps(result))
            return 2
    args.output.write_text(json.dumps(startup.safe_report(result), indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps(startup.safe_report(result), allow_nan=False))
    return 0 if result["core_render_reference_complete"] else 2


if __name__ == "__main__":
    sys.exit(main())
