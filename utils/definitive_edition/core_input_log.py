"""Audit saved Core-owned configured keyboard input without operating GTA."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

try:
    from . import core_startup_log as startup
except ImportError:
    import core_startup_log as startup

PROVENANCE = "mta_core_configured_input"
CONTEXT = dict(in_vehicle=False, has_detonator=False, is_dead=False,
               aiming_weapon=False, entering_vehicle=False)
ACTIONS = dict(forwards=4, backwards=5, left=6, right=7, sprint=13, walk=17)
MOVEMENT_MASK = sum(1 << action for action in ACTIONS.values())
NOTICE = (
    "This checks saved reports of real MTA XML bindings, keyboard polling, canonical synthesis and cleanup. "
    "The on-foot context is an explicit assumption. Engine writes, game-context detection, full MTA startup, "
    "rendering and remote ownership remain unverified by this read-only reference."
)


def signed(value):
    return value - 0x100000000 if value & 0x80000000 else value


def analyze_log(text, require_exit=False):
    if len(text.encode("utf-8", errors="replace")) > startup.MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    translated, issues, samples = [], [], []
    started = finished = stop = None
    observed = set()
    last_clock = None

    def invalid(line, reason):
        if len(issues) < 64:
            issues.append(dict(line=line, reason=reason))

    for line, raw in enumerate(text.splitlines(), 1):
        try:
            event = startup.logged_event(raw)
        except (RecursionError, OverflowError):
            invalid(line, "JSON exceeds the parser representation")
            continue
        if event is None:
            if "core_input_reference" in raw:
                invalid(line, "Malformed configured-input marker")
            translated.append(raw)
            continue
        kind = event["event"]
        if kind.startswith("core_startup_reference"):
            invalid(line, "Another Core reference cannot accompany configured input")
        if kind == "adapter_start":
            if (event.get("core_input_reference") is not True or event.get("provenance") != PROVENANCE or
                    type(event.get("core_api")) is not int or event["core_api"] != 3):
                invalid(line, "Adapter must identify the configured-input reference")
            event = {**event, "core_startup_reference": True, "provenance": startup.PROVENANCE, "core_api": 2}
            event.pop("core_input_reference", None)
        elif kind == "core_input_reference_started":
            if (set(event) != {"event", "at_ms", "core_api", "sample_interval_ms", "budget_ms", "context", "context_source"} or
                    started or finished or type(event.get("core_api")) is not int or event["core_api"] != 3 or
                    type(event.get("sample_interval_ms")) is not int or event["sample_interval_ms"] != 100 or
                    type(event.get("budget_ms")) is not int or event["budget_ms"] != 60000 or
                    event.get("context_source") != "assumed_on_foot" or event.get("context") != CONTEXT or
                    not isinstance(event.get("context"), dict) or any(value is not False for value in event["context"].values())):
                invalid(line, "One bounded reference with explicit assumed context is required")
            started = event
            event = dict(event="core_startup_reference_started", at_ms=event.get("at_ms"), core_api=2)
        elif kind == "core_input_reference_stop_requested":
            if stop:
                invalid(line, "Duplicate cancellation request")
            stop = event
            event = {**event, "event": "core_startup_reference_stop_requested"}
        elif kind == "core_input_reference_sample":
            if len(samples) >= 1202:
                invalid(line, "Bounded sample count exceeded")
                continue
            state = event.get("state")
            valid = (set(event) == {"event", "at_ms", "elapsed_ms", "state"} and started and not finished and
                     startup.integer(event.get("at_ms")) and startup.integer(event.get("elapsed_ms")) and
                     startup.integer(started.get("at_ms")) and event["at_ms"] - started["at_ms"] == event["elapsed_ms"] and
                     isinstance(state, list) and len(state) == 64 and all(startup.integer(value, 0xffffffff) for value in state))
            if not valid:
                invalid(line, "Sample requires a complete 64-field unsigned snapshot and valid clock/order")
                continue
            if last_clock is not None and event["at_ms"] < last_clock:
                invalid(line, "Sample clock moved backwards")
            last_clock = event["at_ms"]
            previous = samples[-1]["state"] if samples else None
            if (state[0] != 1 or state[1] not in (1, 2) or not 1 <= state[2] <= 1202 or state[3] > 62000 or
                    state[4] not in (4, 5, 7) or state[5] == 0 or state[5] > 10000 or state[6] > 10000 or
                    not 1 <= state[7] <= state[5] or state[8] > state[6] or state[9] > 1200 or state[10] > 1200 or
                    not 1 <= state[11] <= 1201 or state[12] & ~MOVEMENT_MASK or state[13] or state[14] > 9 or state[15] not in (0, 1)):
                invalid(line, "Snapshot version, bounds, keyboard coverage or movement scope is invalid")
            if previous and (state[2] <= previous[2] or state[3] < previous[3] or state[11] < previous[11] or
                             state[9] < previous[9] or state[10] < previous[10] or previous[1] == 2 or
                             state[5:9] != previous[5:9]):
                invalid(line, "Snapshot sequence/counters/configuration changed inconsistently")
            current = [signed(value) for value in state[16:40]]
            old = [signed(value) for value in state[40:64]]
            for controller in (current, old):
                if (any(value not in (-128, 0, 128) for value in controller[:2]) or
                        any(controller[index] for index in range(24) if index not in (0, 1, 16, 21)) or
                        controller[16] not in (0, 255) or controller[21] not in (0, 255)):
                    invalid(line, "Controller values exceed the configured movement-only contract")
            if previous and state[1] == 1 and state[2] == previous[2] + 1 and state[40:64] != previous[16:40]:
                invalid(line, "Consecutive controller previous/current frames disagree")
            held = state[12]
            if state[1] == 1:
                if state[14] or state[15]:
                    invalid(line, "A running sample cannot claim terminal cleanup")
            elif state[14] != 1 or state[15] != 1 or state[3] < 60000 or held or any(current) or any(old):
                invalid(line, "Successful terminal input requires time completion and neutral cleanup")
            if state[4] != 7 and (held or any(current)):
                invalid(line, "Unfocused or unrearmed input must be neutral")
            for positive, negative, index in (("right", "left", 0), ("backwards", "forwards", 1)):
                p, n = bool(held & (1 << ACTIONS[positive])), bool(held & (1 << ACTIONS[negative]))
                expected = 128 if p and not n else -128 if n and not p else 0 if not p and not n else None
                if expected is not None and current[index] != expected:
                    invalid(line, "Configured direction mask disagrees with canonical controller")
                if p and not n and current[index] == 128:
                    observed.add(positive)
                if n and not p and current[index] == -128:
                    observed.add(negative)
            for name, index in (("sprint", 16), ("walk", 21)):
                if current[index] != (255 if held & (1 << ACTIONS[name]) else 0):
                    invalid(line, "Configured modifier mask disagrees with canonical controller")
            if held & (1 << ACTIONS["sprint"]) and current[16] == 255 and current[1] == -128:
                observed.add("sprint")
            if held & (1 << ACTIONS["walk"]) and current[21] == 255 and current[1] == -128:
                observed.add("walk")
            if state[1] == 1 and state[4] == 7 and not held and not any(current):
                observed.add("neutral")
            samples.append({**event, "line": line})
            continue
        elif kind == "core_input_reference_finished":
            if (set(event) != {"event", "at_ms", "elapsed_ms", "state", "run_result", "passed", "cleanup_passed", "stop_requested", "samples",
                              "full_core_ready", "rendering_started", "network_active", "input_writes", "remote_peds_created"} or
                    finished or not samples or samples[-1]["state"][1] != 2 or
                    type(event.get("samples")) is not int or event["samples"] != len(samples)):
                invalid(line, "One terminal result must follow the final input sample and preserve its sample count")
            if samples and (not startup.integer(event.get("at_ms")) or event["at_ms"] < samples[-1]["at_ms"]):
                invalid(line, "Terminal clock precedes its input cleanup")
            finished = event
            event = {**event, "event": "core_startup_reference_finished"}
            event.pop("samples", None)
        elif kind.startswith("core_input_reference_"):
            invalid(line, "Unknown configured-input marker")
        translated.append(json.dumps(event, allow_nan=False))

    base = startup.analyze_log("\n".join(translated), require_exit)
    terminal = bool(samples and samples[-1]["state"][1] == 2 and samples[-1]["state"][14:16] == [1, 1])
    lifecycle = bool(base["core_startup_reference_complete"] and not issues and terminal and not stop)
    required = {"forwards", "backwards", "left", "right", "sprint", "walk", "neutral"}
    coverage = lifecycle and required <= observed
    return dict(schema_version=1, status="core_input_reference_observed" if coverage else
                "movement_coverage_missing" if lifecycle else "invalid_evidence",
                core_input_reference_complete=coverage, configured_input_session_complete=lifecycle,
                configured_keyboard_coverage_verified=coverage, observed_controls=sorted(observed),
                missing_controls=sorted(required - observed), sample_count=len(samples),
                terminal_input_state=samples[-1]["state"] if samples else None,
                issue_count=len(issues), issues=issues, lifecycle=base,
                normal_exit_verified=base["normal_exit_verified"], full_mta_boot_verified=False,
                de_renderer_verified=False, engine_input_write_verified=False,
                remote_player_context_verified=False, context_source="assumed_on_foot", notice=NOTICE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--require-exit", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError("Output must differ from the raw evidence log")
        snapshot = startup.read_snapshot(args.log)
        result = analyze_log(snapshot["data"].decode("utf-8-sig", errors="replace"), args.require_exit)
        result.update(log=str(args.log.resolve()), evidence_scope="entire_log",
                      log_fingerprint={key: snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")},
                      reader_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    except (OSError, ValueError) as error:
        result = dict(status="read_error", error=str(error), core_input_reference_complete=False)
        if "Output must differ" in str(error):
            print(json.dumps(result))
            return 2
    args.output.write_text(json.dumps(startup.safe_report(result), indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps(startup.safe_report(result), allow_nan=False))
    return 0 if result["core_input_reference_complete"] else 2


if __name__ == "__main__":
    sys.exit(main())
