"""Audit actual clip cadence in one owned three-speed animation-rate trial."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re
import sys

try:
    from .session_log import MAX_LOG_BYTES, json_event, read_snapshot
    from .native_gait_log import clock, number, ped_id, slope, transform
except ImportError:
    from session_log import MAX_LOG_BYTES, json_event, read_snapshot
    from native_gait_log import clock, number, ped_id, slope, transform


REFERENCE_SPEED = 1.5216103452786847
BASELINE_CADENCE = 0.8823398519979847
RATE_MIN, RATE_MAX = 0.25, 2.0
CADENCE_TOLERANCE = 0.10
RATE_STABILITY_TOLERANCE = 0.02
SPEED_TOLERANCE = 0.10
MAX_EVENTS = 1024
REQUEST_SEQUENCE = ["idle", "walk", "idle", "walk", "idle", "walk", "idle"]
STAGE_SPEEDS = (0.5, -1.0, 1.5)
KINDS = {"joined", "remote_ped_created", "remote_ped_deleted", "remote_animation_requested",
         "remote_animation_rate_applied", "remote_gait_probe_started", "remote_gait_sample",
         "remote_gait_probe_finished", "remote_animation_clock_reset", "adapter_stopped"}
NOTICE = (
    "Setter markers describe requested rates; verification requires observed clip-phase cadence "
    "in every walking stage. The native reference supplies engine travel/clip timing, not proof "
    "of foot contact or no sliding. No human visual observation is inferred from logs."
)


def close(value, expected, tolerance=1e-9):
    return number(value) and abs(value - expected) <= tolerance


def sample_fit(samples):
    # A setter can succeed while this build ignores its rate. Fit the observed
    # association phase against actual clocks before comparing its multiplier.
    if len(samples) < 3:
        return None
    times = [(sample["sample_at_ms"] - samples[0]["sample_at_ms"]) / 1000 for sample in samples]
    phases = [sample["unwrapped_phase"] for sample in samples]
    cadence, residual = slope(phases, times)
    x_speed, x_residual = slope([sample["network_target"]["x"] for sample in samples], times)
    mean_rate = sum(sample["requested_animation_rate"] for sample in samples) / len(samples)
    expected = BASELINE_CADENCE * mean_rate
    wraps = sum(sample.get("phase_wrapped", False) for sample in samples[1:])
    return {"first_line": samples[0]["line"], "last_line": samples[-1]["line"],
            "source_lines": [sample["line"] for sample in samples], "sample_count": len(samples),
            "elapsed_seconds": times[-1], "phase_wraps_observed": wraps,
            "unwrapped_phase_advance": phases[-1] - phases[0],
            "actual_phase_cycles_per_second_fit": cadence,
            "actual_cycle_period_seconds_fit": 1 / cadence if cadence and cadence > 0 else None,
            "phase_fit_mean_squared_residual": residual,
            "signed_network_x_speed_units_per_second_fit": x_speed,
            "network_position_fit_mean_squared_residual": x_residual,
            "mean_requested_animation_rate": mean_rate,
            "expected_phase_cycles_per_second": expected,
            "cadence_relative_error": abs(cadence - expected) / expected if cadence is not None else None,
            "cadence_within_tolerance": cadence is not None and abs(cadence - expected) / expected <= CADENCE_TOLERANCE,
            "sufficient_contiguous_cycles": wraps >= 2 and times[-1] >= 1 and cadence is not None and cadence > 0}


def analyze_log(text, require_exit=False):
    if len(text.encode("utf-8")) > MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    events, issues, failures, entries = [], [], [], []
    shutdown_line = exit_line = None
    for line, raw in enumerate(text.splitlines(), 1):
        if re.search(r'\[INFO\]\s+New event received:\s*"host shut down"\s*$', raw):
            if shutdown_line is not None:
                issues.append({"line": line, "reason": "Repeated host shutdown"})
            shutdown_line = line
        if re.search(r"\[INFO\]\s+Exit\s*$", raw):
            if exit_line is not None:
                issues.append({"line": line, "reason": "Repeated runtime Exit"})
            exit_line = line
        value = json_event(raw)
        if value is None:
            if any(kind in raw for kind in KINDS if kind.startswith("remote_")):
                issues.append({"line": line, "reason": "Malformed remote trial JSON"})
            continue
        if value["event"] == "adapter_phase" and value.get("phase") == "entry":
            entries.append({**value, "line": line})
        if value["event"] in {"adapter_failed", "cleanup_failed"}:
            failures.append({**value, "line": line})
        if value["event"] in KINDS:
            if len(events) < MAX_EVENTS:
                events.append({**value, "line": line})
            else:
                issues.append({"line": line, "reason": "Rate trial event cap exceeded"})
    if len(entries) != 1 or entries[0].get("mode") != "online":
        issues.append({"line": entries[-1]["line"] if entries else None,
                       "reason": "One online adapter entry is required; restarts cannot combine trials"})
    elif any(event["line"] < entries[0]["line"] for event in events):
        issues.append({"line": entries[0]["line"], "reason": "Remote evidence preceded the online adapter entry"})
    counts = Counter(event["event"] for event in events)
    for kind in ("joined", "remote_ped_created", "remote_ped_deleted", "remote_gait_probe_started",
                 "remote_gait_probe_finished", "adapter_stopped"):
        if counts[kind] > 1:
            issues.append({"line": None, "reason": "Repeated " + kind})

    own_id = owned = probe = probe_anchor = finished = deletion = stopped = None
    request = applied = last_clock = last_sample = last_application = None
    requests, applications, samples, skipped, rejected, boundaries, groups = [], [], [], [], [], [], []
    segments, segment, current_group = [], [], None

    def invalid(event, reason):
        issues.append({"line": event["line"], "reason": reason})

    def end_segment(reason, line):
        nonlocal segment
        if segment:
            segments.append(segment)
            segment = []
        if reason:
            boundaries.append({"line": line, "reason": reason})

    def identity(event, ped_required=True):
        return (owned is not None and ped_id(event.get("player_id"))
                and event.get("player_id") == owned.get("player_id")
                and (not ped_required or (ped_id(event.get("ped")) and event.get("ped") == owned.get("ped"))))

    def task_link(event):
        return (request is not None and clock(event.get("task_requested_at_ms"))
                and event.get("task_requested_at_ms") == request.get("requested_at_ms"))

    for event in events:
        kind, at = event["event"], event.get("sample_at_ms")
        before = len(issues)
        if kind not in {"joined", "remote_ped_created", "adapter_stopped"}:
            if not identity(event, ped_required=kind != "remote_ped_deleted"):
                invalid(event, "Event does not match the one owned remote NPC")
            if deletion is not None:
                invalid(event, "Remote evidence followed owned deletion")
        if stopped is not None:
            invalid(event, "Evidence followed adapter stop")
        if "sample_at_ms" in event:
            if not clock(at) or (last_clock is not None and at < last_clock):
                invalid(event, "Rate trial clock is invalid or reversed")
            if clock(at):
                last_clock = at
        if kind == "joined":
            if not ped_id(event.get("player_id")) or own_id is not None:
                invalid(event, "Invalid or repeated local connection identity")
            own_id = event.get("player_id")
        elif kind == "remote_ped_created":
            if (owned is not None or own_id is None or not ped_id(event.get("player_id"))
                    or event.get("player_id") == own_id or not ped_id(event.get("ped")) or event.get("model") != 7):
                invalid(event, "Invalid remote creation identity or model")
            owned = event
        elif kind == "remote_animation_requested":
            end_segment("task_request", event["line"])
            requests.append(event)
            expected_clip = {"idle": "idle_stance", "walk": "Walk_civi"}.get(event.get("state"))
            if (expected_clip is None or event.get("animation") != expected_clip
                    or event.get("animation_file") != "PED" or not clock(event.get("requested_at_ms"))
                    or (request and (not clock(request.get("requested_at_ms"))
                                     or event.get("requested_at_ms", 0) <= request.get("requested_at_ms", 0)))):
                invalid(event, "Invalid animation task, clip or task clock")
            requested_at = event.get("requested_at_ms")
            if clock(requested_at):
                if last_clock is not None and requested_at < last_clock:
                    invalid(event, "Animation task clock reversed behind observed evidence")
                last_clock = requested_at
            request, applied = event, None
            if event.get("state") == "walk":
                current_group = {"walk_index": len(groups), "request_line": event["line"],
                                 "task_requested_at_ms": event.get("requested_at_ms"), "applications": []}
                groups.append(current_group)
            else:
                current_group = None
        elif kind == "remote_gait_probe_started":
            if (probe is not None or request is None or event.get("interval_ms") != 100
                    or event.get("budget_ms") != 30000 or event.get("max_samples") != 300 or not clock(at)):
                invalid(event, "Invalid or repeated bounded gait probe")
            probe = event
        elif kind == "remote_animation_rate_applied":
            applications.append(event)
            if (not task_link(event) or request.get("state") != "walk" or event.get("state") != "walk"
                    or event.get("clip") != "Walk_civi" or not close(event.get("reference_speed"), REFERENCE_SPEED)
                    or not close(event.get("rate_min"), RATE_MIN) or not close(event.get("rate_max"), RATE_MAX)
                    or event.get("setter_interval_ms") != 250 or not close(event.get("min_rate_delta"), 0.05)
                    or event.get("min_speed_window_ms") != 250 or event.get("max_speed_window_ms") != 500
                    or event.get("source") != "received_target_xy"):
                invalid(event, "Applied rate task, calibration or bounded setter policy mismatch")
            window = event.get("speed_window_ms")
            begin, end = event.get("speed_window_started_at_ms"), event.get("speed_window_finished_at_ms")
            path, speed, rate = event.get("speed_path_distance"), event.get("measured_speed"), event.get("requested_rate")
            if (not clock(window) or not 250 <= window <= 500 or not clock(begin) or not clock(end)
                    or not clock(at) or not 0 <= at - end <= 250 or end - begin != window
                    or event.get("max_speed_window_age_ms") != 250
                    or not clock(event.get("speed_window_age_ms")) or event.get("speed_window_age_ms") != at - end
                    or request is None or not clock(request.get("requested_at_ms")) or begin < request["requested_at_ms"]
                    or not clock(event.get("task_age_ms")) or event.get("task_age_ms") != at - request["requested_at_ms"]
                    or type(event.get("speed_sample_count")) is not int or event.get("speed_sample_count", 0) < 2
                    or not number(path) or path <= 0 or not number(speed) or not 0.25 <= speed <= 15
                    or not number(rate) or not RATE_MIN <= rate <= RATE_MAX):
                invalid(event, "Invalid received-path speed window or requested rate")
            elif (not close(speed, path * 1000 / window, 1e-6)
                  or not close(rate, max(RATE_MIN, min(RATE_MAX, speed / REFERENCE_SPEED)), 1e-9)):
                invalid(event, "Applied rate does not follow the measured-path calibration")
            if clock(at) and last_application is not None and at - last_application < 250:
                invalid(event, "Animation setter exceeded its bounded task cadence")
            if applied and number(rate) and abs(rate - applied["requested_rate"]) + 1e-12 < 0.05:
                invalid(event, "Animation setter repeated without the minimum rate change")
            if current_group:
                current_group["applications"].append(event)
            if len(issues) == before:
                applied = event
            if clock(at):
                last_application = at
        elif kind == "remote_gait_sample":
            samples.append(event)
            if (probe is None or finished is not None or not task_link(event) or request is None
                    or event.get("state") != request.get("state") or event.get("animation_file") != "PED"
                    or event.get("clip") != request.get("animation") or len(samples) > 300
                    or not all(transform(event.get(key)) for key in ("requested_transform", "observed_transform", "network_target"))):
                invalid(event, "Gait sample source, task, transform or lifecycle mismatch")
            if (not clock(at) or probe is None or not clock(probe.get("sample_at_ms"))
                    or not clock(event.get("probe_elapsed_ms")) or event.get("probe_elapsed_ms") != at - probe["sample_at_ms"]
                    or event.get("probe_elapsed_ms", 30251) > 30250
                    or event.get("probe_elapsed_ms", 30000) >= 30000
                    or request is None or not clock(request.get("requested_at_ms"))
                    or not clock(event.get("task_age_ms")) or event.get("task_age_ms") != at - request["requested_at_ms"]):
                invalid(event, "Gait sample elapsed time or task clock mismatch")
            if (type(event.get("playing")) is not bool
                    or (event.get("playing") is True and (not number(event.get("time"), 1) or event["time"] < 0))
                    or (event.get("playing") is False and event.get("time") is not None)):
                invalid(event, "Invalid observed animation condition or phase")
            expected_rate = applied.get("requested_rate") if applied else None
            if event.get("requested_animation_rate") != expected_rate or (expected_rate is not None and not number(event.get("requested_animation_rate"), 2)):
                invalid(event, "Gait sample requested rate differs from the latest applied task rate")
            if last_sample and clock(at) and clock(last_sample.get("sample_at_ms")) and at - last_sample["sample_at_ms"] < 100:
                invalid(event, "Gait sample cadence exceeds its bounded read rate")
            previous_sample = last_sample
            if len(issues) > before:
                rejected.append({**event, "reasons": [item["reason"] for item in issues[before:]]})
                end_segment("invalid_sample", event["line"])
                continue
            last_sample = event
            if probe_anchor is None:
                probe_anchor = event["network_target"]
            reason = ("idle_task" if event["state"] != "walk" else "clip_unavailable" if not event["playing"]
                      else "warmup" if event["task_age_ms"] < 1000 else "rate_not_applied" if applied is None else None)
            if reason:
                skipped.append({**event, "reason": reason})
                end_segment(reason, event["line"])
                continue
            if not previous_sample or previous_sample.get("task_requested_at_ms") != event["task_requested_at_ms"]:
                skipped.append({**event, "reason": "motion_baseline_missing"})
                end_segment("motion_baseline_missing", event["line"])
                continue
            gap = at - previous_sample["sample_at_ms"]
            if gap > 250:
                end_segment("sample_gap_over_250ms", event["line"])
            earlier, target = previous_sample["network_target"], event["network_target"]
            distance = math.hypot(target["x"] - earlier["x"], target["y"] - earlier["y"])
            speed = distance * 1000 / gap
            if distance < 0.02 or speed < 0.25:
                # Walking stays requested for 350 ms after movement stops.
                # Those stationary phases cannot validate moving stride timing.
                skipped.append({**event, "reason": "stationary_endpoint_or_pause"})
                end_segment("stationary_endpoint_or_pause", event["line"])
                continue
            if (speed > 3 or abs(target["y"] - earlier["y"]) > 0.05 or abs(target["z"] - earlier["z"]) > 0.05
                    or abs(target["y"] - probe_anchor["y"]) > 0.1 or abs(target["z"] - probe_anchor["z"]) > 0.1
                    or max(abs(event["observed_transform"][axis] - event["requested_transform"][axis]) for axis in ("x", "y", "z")) > 0.25):
                rejected.append({**event, "reasons": ["Observed route or native readback left fit bounds"]})
                end_segment("motion_or_readback_discontinuity", event["line"])
                continue
            sample = {**event, "walk_index": current_group["walk_index"], "phase_wrapped": False,
                      "unwrapped_phase": event["time"], "rate_application_line": applied["line"]}
            if segment:
                previous = segment[-1]
                if abs(sample["requested_animation_rate"] - segment[0]["requested_animation_rate"]) / segment[0]["requested_animation_rate"] > RATE_STABILITY_TOLERANCE:
                    # Never combine distinct requested rates into a deceptively
                    # plausible average cadence across an adaptation boundary.
                    end_segment("requested_rate_changed", event["line"])
                else:
                    delta = sample["time"] - previous["time"]
                    wrapped = delta < -0.5
                    if wrapped:
                        delta += 1
                    max_advance = BASELINE_CADENCE * RATE_MAX * (at - previous["sample_at_ms"]) / 1000 * 1.25 + 0.02
                    if (delta < 0 or delta > max_advance or (wrapped and (previous["time"] < 0.5 or sample["time"] > 0.5))):
                        rejected.append({**sample, "reasons": ["Suspicious observed phase discontinuity"]})
                        end_segment("phase_discontinuity", event["line"])
                        continue
                    sample["phase_wrapped"] = wrapped
                    sample["unwrapped_phase"] = previous["unwrapped_phase"] + delta
            segment.append(sample)
        elif kind == "remote_gait_probe_finished":
            end_segment("probe_finished", event["line"])
            if (probe is None or finished is not None or not clock(at) or not clock(probe.get("sample_at_ms"))
                    or not clock(event.get("probe_elapsed_ms")) or event.get("probe_elapsed_ms") != at - probe["sample_at_ms"]
                    or type(event.get("samples")) is not int or event.get("samples") != len(samples)
                    or event.get("reason") not in {"deleted", "time_budget", "sample_budget"}):
                invalid(event, "Invalid or abnormal bounded probe finish")
            if (event.get("reason") == "time_budget" and
                    (not clock(event.get("probe_elapsed_ms")) or not 30000 <= event["probe_elapsed_ms"] <= 30250)):
                invalid(event, "Probe time-budget finish differs from the finite deadline")
            if event.get("reason") == "sample_budget" and len(samples) != 300:
                invalid(event, "Probe sample-budget finish differs from the finite cap")
            finished = event
        elif kind == "remote_ped_deleted":
            if finished is None or ("ped" in event and not identity(event)):
                invalid(event, "Owned deletion preceded probe finish or mismatched the handle")
            deletion = event
        elif kind == "remote_animation_clock_reset":
            invalid(event, "Animation clock reset interrupts the rate trial")
            end_segment("clock_reset", event["line"])
        elif kind == "adapter_stopped":
            if deletion is None or event.get("reason") != "Ctrl+Shift+End":
                invalid(event, "Controlled adapter stop preceded owned cleanup or used a different shortcut")
            stopped = event
    end_segment(None, None)
    fits = []
    for contiguous in segments:
        fit = sample_fit(contiguous)
        if fit:
            fit["walk_index"] = contiguous[0]["walk_index"]
            fits.append(fit)
    for group in groups:
        index = group["walk_index"]
        group["segment_fits"] = [fit for fit in fits if fit["walk_index"] == index]
        sufficient = [fit for fit in group["segment_fits"] if fit["sufficient_contiguous_cycles"]]
        fit = max(sufficient, key=lambda value: value["sample_count"], default=None)
        group["selected_fit"] = fit
        expected_speed = STAGE_SPEEDS[index] if index < 3 else None
        expected_rate = max(RATE_MIN, min(RATE_MAX, abs(expected_speed) / REFERENCE_SPEED)) if expected_speed is not None else None
        group["expected_signed_speed"] = expected_speed
        group["expected_animation_rate"] = expected_rate
        group["actual_cadence_verified"] = bool(
            fit and expected_speed is not None and fit["cadence_within_tolerance"]
            and abs(fit["signed_network_x_speed_units_per_second_fit"] - expected_speed) / abs(expected_speed) <= SPEED_TOLERANCE
            and abs(fit["mean_requested_animation_rate"] - expected_rate) / expected_rate <= SPEED_TOLERANCE)
    cleanup_order = (stopped is not None and shutdown_line is not None and exit_line is not None
                     and stopped["line"] < shutdown_line < exit_line)
    if ((shutdown_line is not None or exit_line is not None) and not cleanup_order):
        issues.append({"line": exit_line or shutdown_line, "reason": "Host shutdown/Exit is missing or preceded controlled cleanup"})
    flags = {"one_owned_remote_created": counts["remote_ped_created"] == 1,
             "seven_task_sequence": [event.get("state") for event in requests] == REQUEST_SEQUENCE,
             "one_bounded_probe_started": counts["remote_gait_probe_started"] == 1,
             "one_bounded_probe_finished": counts["remote_gait_probe_finished"] == 1,
             "owned_deletion_recorded": counts["remote_ped_deleted"] == 1,
             "all_three_actual_cadences_verified": len(groups) == 3 and all(group["actual_cadence_verified"] for group in groups),
             "all_three_walk_rates_applied": len(groups) == 3 and all(group["applications"] for group in groups),
             "valid_trial_markers": not issues, "no_rejected_samples": not rejected,
             "no_adapter_failures": not failures, "required_exit_verified": not require_exit or cleanup_order}
    verified = all(flags.values())
    return {"schema_version": 1, "status": "animation_rate_runtime_verified" if verified else "adapter_failed" if failures
            else "invalid_evidence" if issues or rejected else "incomplete", "animation_rate_runtime_verified": verified,
            "foot_contact_or_no_sliding_verified": False, "stride_matching_verified": False,
            "human_visual_observation_inferred": False, "normal_exit_verified": cleanup_order,
            "require_exit": require_exit, "flags": flags, "events": events, "event_counts": dict(counts),
            "walking_groups": groups, "sample_count": len(samples), "skipped_sample_count": len(skipped),
            "skipped_samples": skipped, "skipped_reason_counts": dict(Counter(sample["reason"] for sample in skipped)),
            "rejected_sample_count": len(rejected), "rejected_samples": rejected,
            "selected_samples": [sample for contiguous in segments for sample in contiguous
                                 if any(group["selected_fit"] and sample["line"] in group["selected_fit"]["source_lines"] for group in groups)],
            "segmentation_boundaries": boundaries, "issue_count": len(issues), "issues": issues[:64],
            "failures": failures[:32], "policy": {"reference_speed_units_per_second": REFERENCE_SPEED,
            "baseline_cadence_hz": BASELINE_CADENCE, "cadence_relative_tolerance": CADENCE_TOLERANCE,
            "rate_stability_relative_tolerance": RATE_STABILITY_TOLERANCE, "speed_relative_tolerance": SPEED_TOLERANCE,
            "minimum_task_age_ms": 1000, "maximum_sample_gap_ms": 250, "minimum_observed_wraps": 2}, "notice": NOTICE}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-exit", action="store_true", help="Also require controlled End, host shutdown and Exit")
    args = parser.parse_args(argv)
    try:
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError("Output must differ from the raw evidence log")
        snapshot = read_snapshot(args.log)
        result = analyze_log(snapshot["data"].decode("utf-8-sig", errors="replace"), args.require_exit)
        result.update({"log": str(args.log.resolve()), "source_line_origin": str(args.log.resolve()),
                       "evidence_scope": "entire_log", "log_fingerprint": {
                           key: snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")}})
    except (OSError, ValueError) as error:
        result = {"schema_version": 1, "status": "read_error", "error": str(error),
                  "animation_rate_runtime_verified": False, "foot_contact_or_no_sliding_verified": False,
                  "stride_matching_verified": False, "human_visual_observation_inferred": False, "notice": NOTICE}
        if "Output must differ" in str(error):
            print(json.dumps(result), flush=True)
            return 2
    try:
        args.output.write_text(json.dumps(result, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    except OSError as error:
        print(json.dumps({"status": "write_error", "error": str(error), "animation_rate_runtime_verified": False}), flush=True)
        return 2
    print(json.dumps(result, ensure_ascii=True), flush=True)
    return 0 if result["animation_rate_runtime_verified"] else 2


if __name__ == "__main__":
    sys.exit(main())
