"""Audit saved read-only controller references without inspecting or controlling GTA."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re
import sys

try:
    from .session_log import MAX_LOG_BYTES, json_event, read_snapshot
except ImportError:
    from session_log import MAX_LOG_BYTES, json_event, read_snapshot


MAX_EVENTS = 512
MASK = 0xFFFFF
UNKNOWN_FIELDS = ["m_bChatIndicated", "m_bPedWalk", "m_bVehicleMouseLook", "m_bRadioTrackSkip"]
COMMON_FIELDS = {"event", "reference_id", "ped", "at_ms", "elapsed_ms"}
NORMAL_FINISHES = {"sample_budget", "time_budget", "Ctrl+Shift+End", "stop_key"}
READ_ONLY_FLAGS = {"network_active", "input_writes", "remote_peds_created", "game_input_sent_by_tools"}
DISALLOWED_EVENTS = {
    "connecting", "connected", "joined", "welcome", "online_transform", "remote_ped_created",
    "remote_animation_requested", "local_transform", "input_sent", "game_input_written",
}
NOTICE = (
    "This audit verifies only a bounded read-only controller-reference log and immediate prior-probe "
    "provenance. Nonzero stored input and raw velocity are observations; they do not establish native "
    "walk/jog states, velocity units, physical input mapping, multiplayer, or a complete MTA backend."
)


def clock(value):
    return type(value) is int and 0 <= value <= 9007199254740991


def number(value):
    # Bound integers before floating conversion so hostile JSON cannot overflow
    # the audit while preserving raw engine values without inventing units.
    return type(value) in (int, float) and abs(value) <= 100000 and math.isfinite(value)


def controls(value):
    # These are signed storage fields. A guessed gamepad range would discard
    # useful mapping evidence before the engine binding has been observed.
    return isinstance(value, list) and len(value) == 20 and all(
        type(item) is int and -32768 <= item <= 32767 for item in value
    )


def analyze_log(text, require_exit=False):
    """Keep the whole stream: earlier success must never hide a restart or failure."""
    if len(text.encode("utf-8", errors="replace")) > MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    events, issues, failures, accepted, rejected = [], [], [], [], []
    entries, shutdowns, exits = [], [], []
    counts = Counter()
    state, identity, start_at, last_at, last_sample = "initial", None, None, None, None
    started = finished = stopped = None
    sample_count = 0

    def invalid(event, message):
        issues.append({"line": event.get("line"), "event": event.get("event"), "reason": message})

    def exact_fields(event, extra, optional=()):
        keys = set(event) - {"line"}
        required = COMMON_FIELDS | set(extra)
        if not required <= keys or keys - required - set(optional):
            invalid(event, "Reference payload has missing or unexpected fields")

    def common(event, null_before_ready=False):
        nonlocal identity, start_at, last_at
        if null_before_ready and identity is None and event.get("reference_id") is None and event.get("ped") is None:
            if event.get("elapsed_ms") is not None or (event.get("at_ms") is not None and not clock(event["at_ms"])):
                invalid(event, "Unbound cleanup clock is malformed")
            return
        ped, at, elapsed, reference = (event.get(key) for key in ("ped", "at_ms", "elapsed_ms", "reference_id"))
        if (type(ped) is not int or not 1 <= ped <= 0xFFFFFFFF or not clock(at) or not clock(elapsed)
                or not isinstance(reference, str)):
            invalid(event, "Reference ownership or integer clock is malformed")
            return
        if identity is None:
            identity, start_at = (reference, ped), at - elapsed
            if not clock(start_at) or reference != "input-" + str(start_at) + "-" + str(ped):
                invalid(event, "Reference identifier does not bind its start clock and local ped")
        if (reference, ped) != identity or at - start_at != elapsed:
            invalid(event, "Reference ownership or elapsed clock changed")
        if last_at is not None and at < last_at:
            invalid(event, "Reference clocks reversed")
        last_at = at

    for line_number, raw in enumerate(text.splitlines(), 1):
        if re.search(r'\[INFO\]\s+New event received:\s*"host shut down"\s*$', raw):
            shutdowns.append(line_number)
        if re.search(r"\[INFO\]\s+Exit\s*$", raw):
            exits.append(line_number)
        payload = json_event(raw)
        if payload is None:
            if any(marker in raw for marker in ("native_input_reference_", "local_controller_sample", "cleanup_failed")):
                invalid({"line": line_number}, "Malformed reference JSON cannot be omitted")
            continue
        event = {**payload, "line": line_number}
        if len(events) >= MAX_EVENTS:
            # Continue scanning for exit/failure evidence, but never accept a
            # success obtained by silently trimming a larger reference stream.
            invalid(event, "Event count exceeds the bounded audit limit")
            if payload["event"] in {"adapter_failed", "cleanup_failed"}:
                failures.append(event)
            continue
        events.append(event)
        kind = event["event"]
        counts[kind] += 1
        if kind == "adapter_phase" and event.get("phase") == "entry":
            entries.append(event)
            if event.get("mode") != "smoke" or len(entries) != 1 or state != "initial":
                invalid(event, "Exactly one Smoke adapter entry must precede the reference")
        if kind == "adapter_start":
            # The reference controller's markers must not authenticate a log
            # whose enclosing adapter also claims a transport endpoint.
            if (counts[kind] != 1 or state != "initial" or event.get("mode") != "smoke"
                    or "endpoint" not in event or event["endpoint"] is not None
                    or event.get("native_input_reference") is not True
                    or any(event.get(flag) is not False for flag in ("network_active", "input_writes", "remote_peds_created"))):
                invalid(event, "Adapter start must identify one isolated input reference without an endpoint")
        if kind in DISALLOWED_EVENTS or kind.startswith(("online_", "remote_ped_", "remote_animation_", "native_gait_reference_")):
            invalid(event, "Network, NPC or unrelated reference activity is outside this read-only Smoke trial")
        if any(flag in event and event[flag] is not False for flag in READ_ONLY_FLAGS):
            invalid(event, "Activity claims must remain explicitly false when present")
        if event.get("mode") == "online":
            invalid(event, "Online activity is outside this Smoke trial")
        if kind in {"adapter_failed", "cleanup_failed"}:
            failures.append(event)
            continue
        if not (kind.startswith("native_input_reference_") or kind == "local_controller_sample"):
            continue
        if not entries or entries[0]["line"] >= line_number:
            invalid(event, "Reference publication preceded the Smoke entry")
        before = len(issues)
        common(event, kind in {"native_input_reference_reset", "native_input_reference_stopped"} and start_at is None)

        if kind == "native_input_reference_reset":
            exact_fields(event, {"reason", "current_mask", "previous_mask"})
            if any(type(event.get(field)) is not int or event[field] != 0 for field in ("current_mask", "previous_mask")):
                invalid(event, "Reset readback must contain two integer zero masks")
            if event.get("reason") == "start":
                if state != "initial" or event.get("elapsed_ms") != 0 or identity is None:
                    invalid(event, "Start reset must be the first bound reference publication")
                state = "start_reset"
            elif event.get("reason") == "cleanup":
                if state != "finished" and not (state == "initial" and identity is None):
                    invalid(event, "Cleanup reset must follow finish and precede stop")
                state = "cleanup"
            else:
                invalid(event, "Unknown reset reason")
        elif kind == "native_input_reference_started":
            exact_fields(event, {"sample_interval_ms", "budget_ms", "max_samples", "known_field_count",
                                 "unknown_fields", "velocity_units", "network_active", "input_writes", "remote_peds_created"})
            if state != "start_reset" or started is not None or event.get("elapsed_ms") != 0:
                invalid(event, "Started must immediately follow the one start reset")
            for field, expected in (("sample_interval_ms", 100), ("budget_ms", 30000), ("max_samples", 300), ("known_field_count", 20)):
                if type(event.get(field)) is not int or event[field] != expected:
                    invalid(event, "Started sampling policy differs from the certified bounded probe")
            if event.get("unknown_fields") != UNKNOWN_FIELDS or event.get("velocity_units") is not None:
                invalid(event, "Unknown controller fields or velocity units were invented")
            if any(event.get(flag) is not False for flag in ("network_active", "input_writes", "remote_peds_created")):
                invalid(event, "Started read-only activity flags are missing or active")
            started, state = event, "sampling"
        elif kind == "local_controller_sample":
            sample_count += 1
            exact_fields(event, {"current", "previous", "current_mask", "previous_mask", "previous_source", "previous_at_ms",
                                 "unknown_fields", "velocity_raw", "velocity_units", "position"})
            if state != "sampling" or started is None:
                invalid(event, "Sample appeared outside its started reference")
            if not controls(event.get("current")):
                invalid(event, "Current must contain exactly twenty signed int16 storage values")
            if type(event.get("current_mask")) is not int or event["current_mask"] != MASK:
                invalid(event, "Current mask must contain exactly the twenty known fields")
            if event.get("previous_source") != "previous_probe_sample":
                invalid(event, "Previous values must be identified as the previous published probe sample")
            if last_sample is None:
                if (event.get("previous") is not None or event.get("previous_at_ms") is not None
                        or type(event.get("previous_mask")) is not int or event["previous_mask"] != 0):
                    invalid(event, "First sample must have null previous values and clock with a zero mask")
                if started is None or event.get("at_ms") != started.get("at_ms"):
                    invalid(event, "First sample must share the started probe clock")
            else:
                if (not controls(event.get("previous")) or event.get("previous") != last_sample.get("current")
                        or type(event.get("previous_mask")) is not int or event["previous_mask"] != MASK
                        or not clock(event.get("previous_at_ms")) or event["previous_at_ms"] != last_sample.get("at_ms")):
                    invalid(event, "Previous values, mask and clock must match the immediately prior publication")
                if (not clock(event.get("at_ms")) or not clock(last_sample.get("at_ms"))
                        or not 100 <= event["at_ms"] - last_sample["at_ms"] <= 250):
                    invalid(event, "Sample clocks must advance from 100 through 250 milliseconds")
            if event.get("unknown_fields") != UNKNOWN_FIELDS or event.get("velocity_units") is not None:
                invalid(event, "Sample must preserve the four unknown logical fields and null velocity units")
            for field, keys in (("velocity_raw", {"x", "y", "z"}), ("position", {"x", "y", "z", "heading"})):
                value = event.get(field)
                if not isinstance(value, dict) or set(value) != keys or not all(number(component) for component in value.values()):
                    invalid(event, "Sample vector components must be finite bounded numbers with the exact schema")
            if sample_count > 300 or not clock(event.get("elapsed_ms")) or event["elapsed_ms"] >= 30000:
                invalid(event, "Sample exceeded the finite thirty-second or three-hundred-sample budget")
            (accepted if len(issues) == before else rejected).append(event)
            # Retain the raw publication as the predecessor even when invalid:
            # dropping it must never manufacture a different provenance chain.
            last_sample = event
        elif kind == "native_input_reference_finished":
            exact_fields(event, {"reason", "outcome", "samples"}, {"error"})
            if state != "sampling" or finished is not None:
                invalid(event, "Finish must follow sampling exactly once")
            if (event.get("outcome") != "observation_only" or type(event.get("samples")) is not int
                    or event["samples"] != sample_count or not isinstance(event.get("reason"), str) or not event["reason"]):
                invalid(event, "Finish outcome, reason or publication count mismatch")
            reason, elapsed = event.get("reason"), event.get("elapsed_ms")
            if reason in NORMAL_FINISHES:
                if (not clock(elapsed) or elapsed > 30250 or last_sample is None or not clock(last_sample.get("at_ms"))
                        or not clock(event.get("at_ms")) or not 0 <= event["at_ms"] - last_sample["at_ms"] <= 250):
                    invalid(event, "Normal completion exceeded the bounded probe deadline or final sample gap")
                if reason == "sample_budget" and (sample_count != 300 or not clock(elapsed) or not 29900 <= elapsed < 30000):
                    invalid(event, "Sample-budget finish does not match three hundred bounded samples")
                if reason == "time_budget" and (not clock(elapsed) or not 30000 <= elapsed <= 30250):
                    invalid(event, "Time-budget finish does not match the thirty-second deadline")
            if "error" in event:
                failures.append(event)
            finished, state = event, "finished"
        elif kind == "native_input_reference_stopped":
            exact_fields(event, {"reason", "samples", "network_active", "input_writes"})
            if state != "cleanup" or stopped is not None:
                invalid(event, "Stopped must follow the one cleanup reset")
            if (type(event.get("samples")) is not int or event["samples"] != sample_count
                    or not isinstance(event.get("reason"), str) or not event["reason"]):
                invalid(event, "Stopped reason or publication count mismatch")
            if finished is not None and (event.get("reason") != finished.get("reason") or event.get("at_ms") != finished.get("at_ms")):
                invalid(event, "Stop must share its finish reason and clock")
            if any(event.get(flag) is not False for flag in ("network_active", "input_writes")):
                invalid(event, "Stopped activity flags must remain explicitly false")
            stopped, state = event, "stopped"
        else:
            invalid(event, "Unknown native input reference event")

    one_entry = len(entries) == 1 and entries[0].get("mode") == "smoke"
    if not one_entry:
        invalid({"line": None}, "Exactly one Smoke adapter entry is required")
    normal_exit = (len(shutdowns) == len(exits) == 1 and stopped is not None
                   and stopped["line"] < shutdowns[0] < exits[0])
    if (shutdowns or exits) and not normal_exit:
        invalid({"line": (shutdowns + exits)[0]}, "Host shutdown and Exit must appear once in order after stop")
    flags = {
        "one_smoke_entry": one_entry,
        "one_reference_started": counts["native_input_reference_started"] == 1,
        "bounded_samples_present": 0 < sample_count <= 300,
        "one_reference_finished": counts["native_input_reference_finished"] == 1,
        "one_start_reset": sum(item.get("reason") == "start" for item in events if item["event"] == "native_input_reference_reset") == 1,
        "one_cleanup_reset": sum(item.get("reason") == "cleanup" for item in events if item["event"] == "native_input_reference_reset") == 1,
        "reference_stopped": counts["native_input_reference_stopped"] == 1,
        "normal_reference_completion": finished is not None and finished.get("reason") in NORMAL_FINISHES,
        "valid_marker_sequence": not issues,
        "no_adapter_failures": not failures,
        "no_rejected_samples": not rejected,
        "required_exit_verified": not require_exit or normal_exit,
    }
    observed = all(flags.values())
    observations = {
        "analog_nonzero_observed": any(any(sample["current"][:4]) for sample in accepted),
        "engine_velocity_nonzero_observed": any(any(sample["velocity_raw"].values()) for sample in accepted),
        "sprint_button_16_nonzero_observed": any(sample["current"][16] != 0 for sample in accepted),
        "sprint_button_index": 16,
    }
    return {
        "schema_version": 1,
        "status": "controller_reference_observed" if observed else "adapter_failed" if failures else "invalid_evidence" if issues else "incomplete",
        "controller_reference_observed": observed,
        "native_walk_jog_states_verified": False, "velocity_units_verified": False, "full_backend_verified": False,
        "physical_input_mapping_verified": False, "normal_exit_verified": normal_exit, "require_exit": require_exit,
        "flags": flags, "observations": observations, "event_counts": dict(counts), "events": events,
        "sample_count": sample_count, "accepted_sample_count": len(accepted), "accepted_samples": accepted,
        "rejected_sample_count": len(rejected), "rejected_samples": rejected,
        "issue_count": len(issues), "issues": issues[:64], "failures": failures[:32],
        "shutdown_evidence": {"host_shutdown_lines": shutdowns, "exit_lines": exits}, "notice": NOTICE,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-exit", action="store_true")
    args = parser.parse_args(argv)
    try:
        # A report must never overwrite its own evidence, including aliases and
        # hardlinks that happen to have different path strings.
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError("Output must differ from the raw evidence log")
        snapshot = read_snapshot(args.log)
        result = analyze_log(snapshot["data"].decode("utf-8-sig", errors="replace"), args.require_exit)
        result.update({"log": str(args.log.resolve()), "evidence_scope": "entire_log",
                       "log_fingerprint": {key: snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")},
                       "source_line_origin": str(args.log.resolve())})
    except (OSError, ValueError) as error:
        result = {"schema_version": 1, "status": "read_error", "error": str(error), "log": str(args.log),
                  "controller_reference_observed": False, "native_walk_jog_states_verified": False,
                  "velocity_units_verified": False, "full_backend_verified": False, "notice": NOTICE}
        if "Output must differ" in str(error):
            print(json.dumps(result, ensure_ascii=True), flush=True)
            return 2
    try:
        args.output.write_text(json.dumps(result, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    except OSError as error:
        print(json.dumps({"status": "write_error", "error": str(error), "controller_reference_observed": False}), flush=True)
        return 2
    print(json.dumps(result, ensure_ascii=True), flush=True)
    return 0 if result["controller_reference_observed"] else 2


if __name__ == "__main__":
    sys.exit(main())
