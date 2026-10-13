"""Audit one bounded native walking reference; never launch or inspect GTA."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path
import sys

try:
    from .session_log import MAX_LOG_BYTES, json_event, read_snapshot
except ImportError:
    from session_log import MAX_LOG_BYTES, json_event, read_snapshot


PREFIX = "native_gait_reference_"
KINDS = {PREFIX + name for name in ("started", "spawned", "task_requested", "sample",
                                   "finished", "cleanup_deleted", "stopped")}
NORMAL_FINISHES = {"arrived", "time_budget", "sample_budget"}
MAX_EVENTS = 512
MAX_CLOCK = 9007199254740991
FIT_LATERAL_BOUND = 0.5
FIT_Z_BOUND = 0.25
NOTICE = (
    "One owned engine-driven walking task can supply a travel/clip timing reference. "
    "Log evidence does not establish foot contact, no sliding, visible smoothness, "
    "rendering, multiplayer, or runtime stability. No animation rate is changed."
)


def number(value, bound=100000):
    # Reject bools and enormous JSON integers before conversion to float so
    # malformed evidence cannot overflow the audit or masquerade as native data.
    return type(value) in (int, float) and abs(value) <= bound and math.isfinite(value)


def clock(value):
    return type(value) is int and 0 <= value <= MAX_CLOCK


def ped_id(value):
    return type(value) is int and 1 <= value <= 0xFFFFFFFF


def transform(value):
    return isinstance(value, dict) and all(number(value.get(name)) for name in ("x", "y", "z", "heading"))


def geometry(start, target, observed):
    dx, dy = target["x"] - start["x"], target["y"] - start["y"]
    length = math.hypot(dx, dy)
    if length <= 0:
        return None
    ox, oy = observed["x"] - start["x"], observed["y"] - start["y"]
    return {"projected_units": (ox * dx + oy * dy) / length,
            "remaining_units": length - (ox * dx + oy * dy) / length,
            "lateral_units": abs(ox * dy - oy * dx) / length,
            "z_drift_units": abs(observed["z"] - start["z"])}


def slope(values, times):
    mean_t, mean_v = sum(times) / len(times), sum(values) / len(values)
    denominator = sum((value - mean_t) ** 2 for value in times)
    if denominator <= 0:
        return None, None
    rate = sum((t - mean_t) * (v - mean_v) for t, v in zip(times, values)) / denominator
    intercept = mean_v - rate * mean_t
    residual = sum((v - (intercept + rate * t)) ** 2 for t, v in zip(times, values)) / len(times)
    return rate, residual


def fit_segment(samples):
    """Use actual clocks and phases; never assume phase zero at task dispatch."""
    if len(samples) < 3:
        return None
    times = [(event["sample_at_ms"] - samples[0]["sample_at_ms"]) / 1000 for event in samples]
    phases = [event["unwrapped_phase"] for event in samples]
    positions = [event["projected_units"] for event in samples]
    cadence, phase_residual = slope(phases, times)
    speed, position_residual = slope(positions, times)
    wraps = sum(event.get("phase_wrapped", False) for event in samples[1:])
    displacement = positions[-1] - positions[0]
    sufficient = (wraps >= 2 and times[-1] >= 1 and displacement >= 1 and cadence is not None
                  and 0 < cadence <= 4 and speed is not None and 0.25 <= speed <= 3)
    return {"first_line": samples[0]["line"], "last_line": samples[-1]["line"],
            "source_lines": [event["line"] for event in samples], "sample_count": len(samples),
            "elapsed_seconds": times[-1], "phase_wraps_observed": wraps,
            "unwrapped_phase_advance": phases[-1] - phases[0],
            "projected_displacement_units": displacement, "phase_cycles_per_second_fit": cadence,
            "cycle_period_seconds_fit": 1 / cadence if cadence and cadence > 0 else None,
            "projected_speed_units_per_second_fit": speed,
            "projected_units_per_cycle_fit": speed / cadence if cadence and cadence > 0 else None,
            "phase_fit_mean_squared_residual": phase_residual,
            "position_fit_mean_squared_residual": position_residual,
            "sufficient_contiguous_cycles": sufficient}


def analyze_log(text):
    if len(text.encode("utf-8")) > MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    events, issues, failures = [], [], []
    entry_lines = []
    for line, raw in enumerate(text.splitlines(), 1):
        event = json_event(raw)
        if event is None:
            if PREFIX in raw:
                issues.append({"line": line, "reason": "Malformed native reference JSON"})
            continue
        kind = event["event"]
        if kind == "adapter_phase" and event.get("phase") == "entry":
            entry_lines.append(line)
            if event.get("mode") != "smoke":
                issues.append({"line": line, "reason": "Reference entry is not smoke mode"})
        if kind in {"adapter_failed", "cleanup_failed"}:
            failures.append({**event, "line": line})
        if kind.startswith(PREFIX):
            if kind not in KINDS:
                issues.append({"line": line, "reason": "Unknown native reference event"})
            elif len(events) < MAX_EVENTS:
                events.append({**event, "line": line})
            else:
                issues.append({"line": line, "reason": "Native reference event cap exceeded"})
    if len(entry_lines) > 1:
        issues.append({"line": entry_lines[1], "reason": "Adapter restarted inside the reference evidence"})

    counts = Counter(event["event"][len(PREFIX):] for event in events)
    for kind in ("started", "spawned", "task_requested", "finished", "cleanup_deleted", "stopped"):
        if counts[kind] > 1:
            issues.append({"line": None, "reason": "Multiple " + kind + " events"})
    start = spawned = task = finished = cleanup = stopped = None
    last_event_at = last_sample_at = None
    samples, accepted, skipped, rejected, boundaries, segments, segment = [], [], [], [], [], [], []

    def invalid(event, reason):
        issues.append({"line": event["line"], "reason": reason})

    def end_segment(reason, line):
        nonlocal segment
        if segment:
            segments.append(segment)
            segment = []
        if reason:
            boundaries.append({"line": line, "reason": reason})

    def linked(event, task_link=False, ownership=False):
        valid = (start is not None and event.get("reference_id") == start.get("reference_id")
                 and clock(event.get("reference_started_at_ms"))
                 and event.get("reference_started_at_ms") == start.get("sample_at_ms"))
        if task_link:
            valid &= (task is not None and event.get("task_id") == task.get("task_id")
                      and clock(event.get("task_requested_at_ms"))
                      and event.get("task_requested_at_ms") == task.get("sample_at_ms"))
        if ownership:
            valid &= (spawned is not None and event.get("ped") == spawned.get("ped")
                      and ped_id(event.get("ped")) and ped_id(event.get("local_ped"))
                      and event.get("local_ped") == spawned.get("local_ped")
                      and type(event.get("model")) is int and event.get("model") == 7)
        return valid

    for event in events:
        kind, at = event["event"][len(PREFIX):], event.get("sample_at_ms")
        prior_issues = len(issues)
        if not clock(at):
            invalid(event, "Invalid absolute reference clock")
        elif last_event_at is not None and at < last_event_at:
            invalid(event, "Reference clock reversed")
        if clock(at):
            last_event_at = at
        if kind != "started" and not linked(event):
            invalid(event, "Reference source linkage mismatch")
        if stopped is not None:
            invalid(event, "Event followed reference stop")
        if kind == "started":
            if start is not None:
                invalid(event, "Reference restarted")
                continue
            start = event
            expected = {"mode": "smoke", "model": 7, "interval_ms": 100,
                        "readiness_budget_ms": 10000, "settle_ms": 1000, "budget_ms": 10000,
                        "max_samples": 100, "min_arrival_age_ms": 2000, "arrival_radius": 0.5,
                        "lane_radius": 12, "max_z_drift": 2}
            if (event.get("reference_id") != "native-gait-" + str(at)
                    or not clock(event.get("reference_started_at_ms"))
                    or event.get("reference_started_at_ms") != at
                    or any(type(event.get(key)) is not type(value) or event.get(key) != value
                           for key, value in expected.items())):
                invalid(event, "Reference configuration does not match the bounded smoke candidate")
        elif kind == "spawned":
            if spawned or task or finished:
                invalid(event, "Owned spawn is out of order")
                continue
            spawned = event
            begin, target = event.get("start_transform"), event.get("target_transform")
            anchor = event.get("anchor_transform")
            if (not ped_id(event.get("ped")) or not ped_id(event.get("local_ped"))
                    or event.get("ped") == event.get("local_ped") or event.get("model") != 7):
                invalid(event, "Invalid owned NPC or local player identity")
            if (not transform(begin) or not transform(target)
                    or abs(target["x"] - begin["x"] - 8) > 1e-6
                    or any(target[name] != begin[name] for name in ("y", "z"))
                    or begin["heading"] != 270 or target["heading"] != 270):
                invalid(event, "Invalid fixed native reference route")
            if (not transform(anchor) or not transform(begin)
                    or abs(begin["x"] - anchor["x"] + 4) > 1e-6
                    or abs(begin["y"] - anchor["y"] - 3) > 1e-6 or begin["z"] != anchor["z"]):
                invalid(event, "Native route does not match the recorded local anchor")
        elif kind == "task_requested":
            if task or spawned is None or finished:
                invalid(event, "Native task is missing ownership or is out of order")
                continue
            task = event
            if (not linked(event, ownership=True) or event.get("task_id") != "walk-" + str(at)
                    or event.get("task_requested_at_ms") != at or event.get("clip") != "Walk_civi"
                    or type(event.get("move_state")) is not int or event.get("move_state") != 4
                    or type(event.get("time_limit_ms")) is not int or event.get("time_limit_ms") != 10000
                    or event.get("start_transform") != spawned.get("start_transform")
                    or event.get("target_transform") != spawned.get("target_transform")):
                invalid(event, "Native task identity, walk arguments or route mismatch")
            if clock(at) and clock(spawned.get("sample_at_ms")) and at - spawned["sample_at_ms"] < 1000:
                invalid(event, "Walking task preceded the native settling period")
            if start and clock(at) and clock(start.get("sample_at_ms")) and at - start["sample_at_ms"] > 11250:
                invalid(event, "Walking task exceeded the readiness and settling budgets")
        elif kind == "sample":
            samples.append(event)
            if len(samples) > 100:
                invalid(event, "Reference sample cap exceeded")
            if (not linked(event, task_link=True, ownership=True) or finished is not None
                    or event.get("clip") != "Walk_civi" or not transform(event.get("observed_transform"))):
                invalid(event, "Sample identity, clip, transform or lifecycle mismatch")
            if (type(event.get("playing")) is not bool
                    or (event.get("playing") is True and (not number(event.get("time"), 1) or event["time"] < 0))
                    or (event.get("playing") is False and event.get("time") is not None)):
                invalid(event, "Invalid native animation condition or phase")
            if (not clock(event.get("elapsed_ms")) or not clock(event.get("task_age_ms"))
                    or not clock(at) or start is None or task is None
                    or not clock(start.get("sample_at_ms")) or not clock(task.get("sample_at_ms"))
                    or event.get("elapsed_ms") != at - start["sample_at_ms"]
                    or event.get("task_age_ms") != at - task["sample_at_ms"]
                    or event.get("task_age_ms", 10000) >= 10000):
                invalid(event, "Sample clock or task age mismatch")
            if clock(at) and last_sample_at is not None and at - last_sample_at < 100:
                invalid(event, "Reference samples exceed the bounded read cadence")
            gap = clock(at) and last_sample_at is not None and at - last_sample_at > 250
            if clock(at):
                last_sample_at = at
            if len(issues) > prior_issues:
                rejected.append({**event, "reasons": [item["reason"] for item in issues[prior_issues:]]})
                end_segment("invalid_sample", event["line"])
                continue
            if gap:
                end_segment("sample_gap_over_250ms", event["line"])
            if not transform(spawned.get("start_transform")) or not transform(spawned.get("target_transform")):
                rejected.append({**event, "reasons": ["Invalid owned route"]})
                end_segment("invalid_route", event["line"])
                continue
            measure = geometry(spawned["start_transform"], spawned["target_transform"], event["observed_transform"])
            if measure is None:
                rejected.append({**event, "reasons": ["Invalid zero-length owned route"]})
                end_segment("invalid_route", event["line"])
                continue
            sample = {**event, **measure}
            reason = ("clip_unavailable" if not event["playing"] else "warmup" if event["task_age_ms"] < 1000
                      else "destination_stopping_window" if measure["remaining_units"] < 1 else None)
            if reason:
                skipped.append({**sample, "reason": reason})
                end_segment(reason, event["line"])
                continue
            if (measure["lateral_units"] > FIT_LATERAL_BOUND or measure["z_drift_units"] > FIT_Z_BOUND
                    or measure["projected_units"] < -0.1):
                rejected.append({**sample, "reasons": ["Observed pose left the straight level fit bounds"]})
                end_segment("route_or_height_departure", event["line"])
                continue
            sample["phase_wrapped"] = False
            sample["unwrapped_phase"] = event["time"]
            if segment:
                previous = segment[-1]
                delta = event["time"] - previous["time"]
                wrapped = delta < -0.5
                if wrapped:
                    delta += 1
                # A restart can resemble a wrap. Bound advancement and require
                # a visible end-to-start crossing rather than inventing cycles.
                if (delta < 0 or delta > 0.35 or
                        (wrapped and (previous["time"] < 0.65 or event["time"] > 0.35))):
                    rejected.append({**sample, "reasons": ["Suspicious animation phase discontinuity"]})
                    end_segment("phase_discontinuity", event["line"])
                    continue
                duration = (at - previous["sample_at_ms"]) / 1000
                travel = measure["projected_units"] - previous["projected_units"]
                if travel < -0.02 or travel / duration > 3:
                    rejected.append({**sample, "reasons": ["Implausible projected native motion"]})
                    end_segment("motion_discontinuity", event["line"])
                    continue
                sample["phase_wrapped"] = wrapped
                sample["unwrapped_phase"] = previous["unwrapped_phase"] + delta
            segment.append(sample)
            accepted.append(sample)
        elif kind in {"finished", "cleanup_deleted", "stopped"}:
            if task and not linked(event, task_link=True, ownership=True):
                invalid(event, "Terminal event task or ownership linkage mismatch")
            if task is None and (event.get("task_id") is not None or event.get("task_requested_at_ms") is not None):
                invalid(event, "Terminal event invented an unrequested task")
            if spawned and not linked(event, ownership=True):
                invalid(event, "Terminal event owned NPC linkage mismatch")
            if kind == "finished":
                if finished:
                    invalid(event, "Reference finish was repeated")
                finished = event
                if (event.get("outcome") != "observation_only" or type(event.get("samples")) is not int
                        or event.get("samples") != len(samples) or not isinstance(event.get("reason"), str)):
                    invalid(event, "Reference finish outcome or sample count mismatch")
                age = event.get("task_age_ms")
                if event.get("reason") == "time_budget" and (not clock(age) or not 10000 <= age <= 10250):
                    invalid(event, "Time-budget finish does not match its finite deadline")
                if event.get("reason") == "sample_budget" and (len(samples) != 100 or not clock(age) or not 9900 <= age <= 10250):
                    invalid(event, "Sample-budget finish does not match its finite sample cap")
                if event.get("reason") == "arrived" and (not clock(age) or age < 2000):
                    invalid(event, "Arrival finish preceded the minimum walking age")
                end_segment("reference_finished", event["line"])
            elif kind == "cleanup_deleted":
                if spawned is None or finished is None or cleanup:
                    invalid(event, "Cleanup is missing ownership or preceded finish")
                cleanup = event
            else:
                if finished is None or (spawned is not None and cleanup is None) or stopped:
                    invalid(event, "Reference stop preceded finish or owned cleanup")
                stopped = event
                if finished and event.get("reason") != finished.get("reason"):
                    invalid(event, "Stop reason differs from reference finish")
            if kind in {"finished", "stopped"}:
                if (not clock(event.get("elapsed_ms")) or not clock(at) or start is None
                        or not clock(start.get("sample_at_ms")) or (task and not clock(task.get("sample_at_ms")))
                        or event.get("elapsed_ms") != at - start["sample_at_ms"]
                        or (task and (not clock(event.get("task_age_ms"))
                                     or event.get("task_age_ms") != at - task["sample_at_ms"]))
                        or (task is None and event.get("task_age_ms") is not None)):
                    invalid(event, "Terminal event elapsed time or task age mismatch")
    end_segment(None, None)
    fits = [fit for samples_in_segment in segments if (fit := fit_segment(samples_in_segment)) is not None]
    sufficient = [fit for fit in fits if fit["sufficient_contiguous_cycles"]]
    selected = max(sufficient, key=lambda fit: fit["sample_count"], default=None)
    flags = {"one_reference_started": counts["started"] == 1,
             "one_owned_npc_spawned": counts["spawned"] == 1,
             "one_native_walking_task": counts["task_requested"] == 1,
             "bounded_samples_present": 0 < counts["sample"] <= 100,
             "one_reference_finished": counts["finished"] == 1,
             "owned_cleanup_recorded": counts["cleanup_deleted"] == 1,
             "reference_stopped": counts["stopped"] == 1,
             "normal_reference_completion": finished is not None and finished.get("reason") in NORMAL_FINISHES,
             "valid_marker_sequence": not issues, "no_adapter_failures": not failures,
             "no_rejected_fit_samples": not rejected, "contiguous_cycles_measured": selected is not None}
    measured = all(flags.values())
    status = ("engine_reference_measured" if measured else "adapter_failed" if failures
              else "invalid_evidence" if issues or rejected else "incomplete")
    return {"schema_version": 1, "status": status, "engine_reference_measured": measured,
            "foot_contact_or_no_sliding_verified": False, "stride_matching_verified": False,
            "bounded_animation_rate_implemented": False, "flags": flags,
            "event_counts": dict(counts), "events": events, "sample_count": len(samples),
            "accepted_sample_count": len(accepted), "accepted_samples": accepted,
            "skipped_sample_count": len(skipped), "skipped_samples": skipped,
            "skipped_reason_counts": dict(Counter(event["reason"] for event in skipped)),
            "rejected_sample_count": len(rejected), "rejected_samples": rejected,
            "segmentation_boundaries": boundaries, "segment_fits": fits, "selected_fit": selected,
            "selected_samples": [sample for sample in accepted if selected and sample["line"] in selected["source_lines"]],
            "fit_policy": {"minimum_task_age_ms": 1000, "minimum_remaining_route_units": 1,
                           "maximum_sample_gap_ms": 250, "minimum_observed_wraps": 2,
                           "maximum_lateral_drift_units": FIT_LATERAL_BOUND, "maximum_z_drift_units": FIT_Z_BOUND},
            "issue_count": len(issues), "issues": issues[:64], "failures": failures[:32], "notice": NOTICE}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        # An audit result must never replace its own raw evidence, including
        # when a caller supplies different spellings or a filesystem link.
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError("Output must differ from the raw evidence log")
        snapshot = read_snapshot(args.log)
        result = analyze_log(snapshot["data"].decode("utf-8-sig", errors="replace"))
        result.update({"log": str(args.log.resolve()), "evidence_scope": "entire_log",
                       "log_fingerprint": {key: snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")},
                       "source_line_origin": str(args.log.resolve())})
    except (OSError, ValueError) as error:
        result = {"schema_version": 1, "status": "read_error", "error": str(error),
                  "log": str(args.log), "engine_reference_measured": False,
                  "foot_contact_or_no_sliding_verified": False, "stride_matching_verified": False,
                  "bounded_animation_rate_implemented": False, "notice": NOTICE}
        if "Output must differ" in str(error):
            print(json.dumps(result, ensure_ascii=True), flush=True)
            return 2
    try:
        args.output.write_text(json.dumps(result, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    except OSError as error:
        print(json.dumps({"status": "write_error", "error": str(error), "engine_reference_measured": False}), flush=True)
        return 2
    print(json.dumps(result, ensure_ascii=True), flush=True)
    return 0 if result["engine_reference_measured"] else 2


if __name__ == "__main__":
    sys.exit(main())
