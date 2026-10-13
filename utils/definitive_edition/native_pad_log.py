"""Validate saved MTA native-pad observations without launching or inspecting GTA."""

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
PROVENANCE = "mta_game_de_native_pad_reader"
FLAGS = {"network_active", "input_writes", "remote_peds_created"}
COMMON = {"event", "reference_id", "ped", "at_ms", "elapsed_ms", "provenance", "field_count", "pad_count",
          "previous_state", "previous_source", "velocity_units"} | FLAGS
NORMAL = {"time_budget", "sample_budget", "Ctrl+Shift+End"}
NOTICE = ("This verifies a bounded saved native NewState/OldState reference and its cleanup. Nonzero fields and raw "
          "velocity are observations, not proof of keyboard/device mapping, velocity units, walk/sprint semantics, "
          "engine writes, multiplayer or full MTA startup.")


def clock(value):
    return type(value) is int and 0 <= value <= 9007199254740991


def finite(value):
    # Bound numeric conversion and derived statistics without assigning units.
    return type(value) in (int, float) and abs(value) <= 100000 and math.isfinite(value)


def vector(value):
    return isinstance(value, dict) and set(value) == {"x", "y", "z"} and all(finite(item) for item in value.values())


def controls(value):
    return isinstance(value, list) and len(value) == 24 and all(type(item) is int and -32768 <= item <= 32767 for item in value)


def analyze_log(text, require_complete=False):
    """Preserve the whole bounded stream; never select an earlier successful run."""
    if len(text.encode("utf-8", errors="replace")) > MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    events, issues, failures, accepted, rejected = [], [], [], [], []
    counts = Counter()
    state, identity, origin, last_at, last_sample = "initial", None, None, None, None
    started = finished = stopped = None
    entries = 0
    overflow = False
    options = {"duration_ms": 30000, "sample_interval_ms": 125, "max_samples": 300,
               "render_gap_ms": 2000, "readiness_budget_ms": 10000}

    def invalid(event, reason):
        issues.append({"line": event.get("line"), "event": event.get("event"), "reason": reason})

    def exact(event, extra, optional=()):
        if set(event) - {"line"} - set(optional) != COMMON | set(extra):
            invalid(event, "Reference has missing or unexpected payload fields")

    def common(event):
        nonlocal identity, origin, last_at
        if (event.get("provenance") != PROVENANCE or type(event.get("field_count")) is not int or event["field_count"] != 24
                or type(event.get("pad_count")) is not int or event["pad_count"] != 2
                or event.get("previous_state") != "engine_OldState" or event.get("previous_source") != "native_OldState"
                or event.get("velocity_units", 1) is not None or any(event.get(flag) is not False for flag in FLAGS)):
            invalid(event, "Native provenance, dimensions, OldState source or read-only flags mismatch")
        reference, ped, at, elapsed = (event.get(key) for key in ("reference_id", "ped", "at_ms", "elapsed_ms"))
        # Cancellation before readiness has no engine snapshot or local identity.
        if ped is None and elapsed is None and identity is None:
            if at is not None and not clock(at):
                invalid(event, "Unbound reference clock is malformed")
            return
        if (not isinstance(reference, str) or not re.fullmatch(r"native-pad-\d{1,16}", reference)
                or type(ped) is not int or not 1 <= ped <= 0x7fffffff or not clock(at) or not clock(elapsed)):
            invalid(event, "Reference identity or integer clocks malformed")
            return
        if identity is None:
            identity, origin = (reference, ped), at - elapsed
            activation = int(reference.removeprefix("native-pad-"))
            if not clock(origin) or not clock(activation) or not 0 <= origin - activation < 10000:
                invalid(event, "Reference activation and bound observation clocks mismatch")
        if (reference, ped) != identity or at - origin != elapsed:
            invalid(event, "Reference identity, local ped or elapsed clock changed")
        if last_at is not None and at < last_at:
            invalid(event, "Reference clocks reversed")
        last_at = at

    for line, raw in enumerate(text.splitlines(), 1):
        try:
            payload = json_event(raw)
        except (RecursionError, OverflowError):
            invalid({"line": line}, "JSON exceeds the parser's bounded representation")
            continue
        if payload is None:
            if any(marker in raw for marker in ("native_pad_reference_", "native_pad_sample", "cleanup_failed")):
                invalid({"line": line}, "Malformed reference JSON cannot be silently skipped")
            continue
        event = {**payload, "line": line}
        if len(events) >= MAX_EVENTS:
            if payload["event"] in {"adapter_failed", "cleanup_failed"} and len(failures) < 32:
                failures.append(event)
            if not overflow:
                invalid(event, "Bounded event count exceeded")
                overflow = True
            continue
        events.append(event)
        kind = event["event"]
        counts[kind] += 1
        if kind == "adapter_phase" and event.get("phase") == "entry":
            entries += 1
            if entries != 1 or state != "initial" or event.get("mode") != "smoke":
                invalid(event, "Exactly one Smoke entry must precede this reference")
        if kind == "adapter_start":
            if (counts[kind] != 1 or state != "initial" or event.get("mode") != "smoke"
                    or event.get("endpoint", 1) is not None or event.get("native_pad_reference") is not True
                    or event.get("provenance") != PROVENANCE or any(event.get(flag) is not False for flag in FLAGS)):
                invalid(event, "Adapter start must identify one isolated native-pad reference")
        if (kind.startswith(("online_", "remote_", "native_gait_reference_", "native_input_reference_"))
                or kind in {"connecting", "connected", "joined", "welcome", "local_transform", "local_controller_sample", "input_sent", "game_input_written"}
                or event.get("mode") == "online" or any(flag in event and event[flag] is not False for flag in FLAGS)):
            invalid(event, "Network, input-write, NPC or unrelated reference activity is forbidden")
        if kind in {"adapter_failed", "cleanup_failed"}:
            failures.append(event)
        if not (kind.startswith("native_pad_reference_") or kind == "native_pad_sample"):
            continue
        before = len(issues)
        if entries != 1:
            invalid(event, "Reference must follow one Smoke entry")
        common(event)
        if kind == "native_pad_reference_reset":
            exact(event, {"reason"})
            if event.get("reason") == "start":
                if state != "initial" or event.get("elapsed_ms") != 0 or identity is None:
                    invalid(event, "Start reset must be the first bound reference marker")
                state = "start_reset"
            elif event.get("reason") == "cleanup":
                if state != "finished":
                    invalid(event, "Cleanup reset must follow the finish marker")
                if finished and event.get("at_ms") != finished.get("at_ms"):
                    invalid(event, "Cleanup clock must match completion")
                state = "cleanup"
            else:
                invalid(event, "Unknown reset reason")
        elif kind == "native_pad_reference_started":
            exact(event, {"duration_ms", "sample_interval_ms", "max_samples", "render_gap_ms", "readiness_budget_ms"})
            for key, low, high in (("duration_ms", 1, 30000), ("sample_interval_ms", 100, 2000), ("max_samples", 1, 300),
                                   ("render_gap_ms", 1, 2000), ("readiness_budget_ms", 1, 10000)):
                if type(event.get(key)) is not int or not low <= event[key] <= high:
                    invalid(event, "Unbounded declared sampling option: " + key)
                else:
                    options[key] = event[key]
            if state != "start_reset" or started is not None or event.get("elapsed_ms") != 0:
                invalid(event, "Exactly one successful start must follow start reset")
            started, state = event, "sampling"
        elif kind == "native_pad_sample":
            exact(event, {"position", "heading", "raw_velocity", "pads"})
            pads = event.get("pads")
            if (not isinstance(pads, list) or len(pads) != 2 or any(not isinstance(pad, dict) or set(pad) != {"current", "previous"}
                    or not controls(pad.get("current")) or not controls(pad.get("previous")) for pad in pads)):
                invalid(event, "Expected two pads with current/previous twenty-four signed16 values")
            if not vector(event.get("position")) or not vector(event.get("raw_velocity")) or not finite(event.get("heading")):
                invalid(event, "Position, heading or raw velocity is not finite")
            if state != "sampling" or not started:
                invalid(event, "Sample must follow the successful start and precede finish")
            elif (not clock(event.get("elapsed_ms")) or event["elapsed_ms"] >= options["duration_ms"]
                  or counts[kind] > options["max_samples"]):
                invalid(event, "Sample exceeds the declared time/count budget")
            if last_sample is None:
                if event.get("elapsed_ms") != 0:
                    invalid(event, "First capture must occur at the successful start")
            elif (not clock(event.get("at_ms")) or not started or not clock(last_sample.get("at_ms"))
                  or not options["sample_interval_ms"] <= event["at_ms"] - last_sample["at_ms"] <= options["render_gap_ms"]):
                invalid(event, "Capture gap is duplicate, early, reversed or exceeds the render budget")
            (accepted if len(issues) == before else rejected).append(event)
            last_sample = event
        elif kind == "native_pad_reference_finished":
            exact(event, {"reason", "outcome", "samples"}, {"error"})
            if finished is not None or state not in {"initial", "start_reset", "sampling"}:
                invalid(event, "Only one finish may precede cleanup")
            if (type(event.get("samples")) is not int or event["samples"] != counts["native_pad_sample"]
                    or not isinstance(event.get("reason"), str) or not event["reason"] or event.get("outcome") != "observation_only"):
                invalid(event, "Finish outcome, reason or published sample count mismatch")
            if isinstance(event.get("reason"), str) and event["reason"] in NORMAL:
                elapsed = event.get("elapsed_ms")
                if (not started or not last_sample or not clock(elapsed) or not clock(event.get("at_ms"))
                        or not clock(last_sample.get("at_ms"))):
                    invalid(event, "Normal completion requires successful capture evidence")
                elif (event["at_ms"] - last_sample["at_ms"] > options["render_gap_ms"]
                      or elapsed > options["duration_ms"] + options["render_gap_ms"]):
                    invalid(event, "Normal completion exceeded deadline or final capture gap")
                elif event["reason"] == "time_budget" and elapsed < options["duration_ms"]:
                    invalid(event, "Time-budget finish preceded the declared duration")
                elif event["reason"] == "sample_budget" and (counts["native_pad_sample"] != options["max_samples"] or elapsed >= options["duration_ms"]):
                    invalid(event, "Sample-budget finish does not match the bounded sample cap")
            if "error" in event:
                failures.append(event)
            finished, state = event, "finished"
        elif kind == "native_pad_reference_stopped":
            exact(event, {"reason", "samples"})
            if (state != "cleanup" or stopped is not None or not finished or event.get("reason") != finished.get("reason")
                    or event.get("at_ms") != finished.get("at_ms") or type(event.get("samples")) is not int
                    or event["samples"] != counts["native_pad_sample"]):
                invalid(event, "Stopped must match one finish and cleanup")
            stopped, state = event, "stopped"
        else:
            invalid(event, "Unknown native-pad event")

    lifecycle = {
        "one_smoke_entry": entries == 1, "one_reference_started": counts["native_pad_reference_started"] == 1,
        "one_start_reset": sum(e.get("reason") == "start" for e in events if e["event"] == "native_pad_reference_reset") == 1,
        "one_cleanup_reset": sum(e.get("reason") == "cleanup" for e in events if e["event"] == "native_pad_reference_reset") == 1,
        "one_reference_finished": counts["native_pad_reference_finished"] == 1,
        "one_reference_stopped": counts["native_pad_reference_stopped"] == 1,
        "normal_completion": finished is not None and isinstance(finished.get("reason"), str) and finished["reason"] in NORMAL,
        "samples_present": 0 < len(accepted) <= 300, "valid_payloads": not issues and not rejected, "no_failures": not failures,
    }
    complete = all(lifecycle.values())
    activity = []
    for pad_index in range(2):
        states = {}
        for which in ("current", "previous"):
            rows = [sample["pads"][pad_index][which] for sample in accepted]
            states[which] = {"nonzero_samples": sum(any(row) for row in rows), "analog_nonzero_samples": sum(any(row[:4]) for row in rows),
                             "button_16_nonzero_samples": sum(row[16] != 0 for row in rows),
                             "changed_from_prior_capture": sum(a != b for a, b in zip(rows, rows[1:])),
                             "nonzero_field_indices": [index for index in range(24) if any(row[index] != 0 for row in rows)]}
        activity.append(states)
    velocities = [sample["raw_velocity"] for sample in accepted]
    speeds = [math.hypot(value["x"], value["y"], value["z"]) for value in velocities]
    displacement = None if not accepted else {axis: accepted[-1]["position"][axis] - accepted[0]["position"][axis] for axis in ("x", "y", "z")}
    return {"schema_version": 1, "status": "native_pad_reference_observed" if complete else "adapter_failed" if failures else "invalid_evidence" if issues else "incomplete",
            "native_pad_reference_observed": complete, "native_pad_reference_complete": complete, "require_complete": require_complete,
            "physical_input_mapping_verified": False, "velocity_units_verified": False, "native_walk_jog_states_verified": False,
            "engine_input_write_verified": False, "full_backend_verified": False, "flags": lifecycle,
            "observations": {"pads": activity, "sprint_button_index": 16, "button_16_semantics_verified": False,
                             "raw_velocity_nonzero_samples": sum(any(value.values()) for value in velocities),
                             "raw_velocity_magnitude_max": max(speeds, default=None), "raw_velocity_magnitude_mean": sum(speeds) / len(speeds) if speeds else None,
                             "displacement": displacement},
            "sample_count": counts["native_pad_sample"], "accepted_sample_count": len(accepted), "accepted_samples": accepted,
            "rejected_sample_count": len(rejected), "rejected_samples": rejected, "event_counts": dict(counts), "events": events,
            "issue_count": len(issues), "issues": issues[:64], "failures": failures[:32], "notice": NOTICE}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-complete", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError("Output must differ from the raw evidence log")
        snapshot = read_snapshot(args.log)
        report = analyze_log(snapshot["data"].decode("utf-8-sig", errors="replace"), args.require_complete)
        report.update(log=str(args.log.resolve()), evidence_scope="entire_log", source_line_origin=str(args.log.resolve()),
                      log_fingerprint={key: snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")})
    except (OSError, ValueError) as error:
        report = {"schema_version": 1, "status": "read_error", "error": str(error), "native_pad_reference_observed": False,
                  "native_pad_reference_complete": False, "physical_input_mapping_verified": False, "velocity_units_verified": False,
                  "engine_input_write_verified": False, "full_backend_verified": False, "notice": NOTICE}
        if "Output must differ" in str(error):
            print(json.dumps(report), flush=True)
            return 2
    try:
        args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    except (OSError, ValueError) as error:
        print(json.dumps({"status": "write_error", "error": str(error)}), flush=True)
        return 2
    print(json.dumps(report, allow_nan=False), flush=True)
    return 0 if report["native_pad_reference_complete"] or (not args.require_complete and report["status"] == "incomplete"
                                                          and report.get("accepted_sample_count", 0) > 0) else 2


if __name__ == "__main__":
    sys.exit(main())
