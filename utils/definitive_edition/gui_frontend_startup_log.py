"""Match saved completed-Core lifecycle evidence to the existing frontend input log."""
import argparse
import hashlib
import json
from pathlib import Path

import gui_frontend_input_log as input_log
from gui_frontend_log import unique_object, unsigned

SCHEMA = "mta_core_gui_frontend_startup_v1"
EVENT = "core_gui_frontend_startup_reference_"
NOTICE = (
    "Copied worker/lifecycle values are matched to a fresh Core input record. "
    "This does not independently observe module unloading, script PID, camera input, "
    "visible GTA pixels, networking or full game readiness."
)


def snapshot_valid(v):
    if type(v) is not list or len(v) != 32 or not all(unsigned(x) for x in v):
        return False
    if not any(v):
        return True
    flags = (3, 4, 5, 6, 8, 10, 11, 12, 13, 14, 15, 18, 19, 20, 23)
    return (v[0] == 1 and v[1] in (1, 2, 3, 4, 5) and v[2] > 0 and
            not any(v[i] > 1 for i in flags) and v[9] in range(6) and v[21] in range(7) and
            not any(v[18:21]) and not any(v[24:]) and not (v[13] and v[14]) and
            (not v[5] or v[3] and v[4]) and (not v[6] or v[5]) and
            (not v[8] or v[3]) and (not any(v[9:13]) or v[8]) and
            (v[1] != 4 or v[14] == 1 and not any(v[i] for i in (5, 6, 8, 9, 10, 11, 12, 13, 15, 16, 17))))


def report_valid(v):
    if type(v) is not list or len(v) != 96 or not all(unsigned(x) for x in v):
        return False
    snapshots = [v[i:i + 32] for i in (0, 32, 64)]
    if not all(snapshot_valid(s) for s in snapshots):
        return False
    populated = [s for s in snapshots if any(s)]
    return (bool(populated) and len({s[2] for s in populated}) == 1 and
            all(b[7] >= a[7] and b[22] >= a[22] for a, b in zip(populated, populated[1:])))


def lifecycle_verified(v):
    if not report_valid(v):
        return False
    begin, view, stopped = v[:32], v[32:64], v[64:]
    return (all(s[3] == s[4] == s[23] == 1 and not any(s[8:16]) and not s[21]
                for s in (begin, view)) and
            begin[1] == view[1] == 2 and begin[5] == view[5] == 1 and
            not begin[6] and not begin[22] and view[6] == view[22] == 1 and view[7] > begin[7] and
            stopped[1] == 4 and stopped[3] == stopped[4] == stopped[14] == stopped[22] == stopped[23] == 1 and
            not stopped[21])


def no_view_cleanup(v):
    if not report_valid(v):
        return False
    populated = [v[i:i + 32] for i in (0, 32, 64) if any(v[i:i + 32])]
    return (v[65] == 4 and v[67] == v[78] == 1 and
            all(not any(s[8:13]) and not s[22] and s[23] == 1 for s in populated))


def decoded_event(line):
    # CLEO quotes the emitted JSON as a JSON string after its log prefix. Also
    # accept raw events from the owned script fixture without altering either.
    text = line.decode("utf-8").strip()
    quoted = text.find('"{\\"event\\"')
    if quoted >= 0:
        text = json.loads(text[quoted:])
    else:
        start = text.find('{"event"')
        if start < 0:
            return None
        text = text[start:]
    record = json.loads(text, object_pairs_hook=unique_object)
    return record if type(record) is dict and str(record.get("event", "")).startswith(EVENT) else None


def assess(core_data, script_data, *, expected_pid, core_baseline, script_baseline, expected_run_ms=None):
    if type(script_data) is not bytes or type(script_baseline) is not bytes:
        raise ValueError("script log and baseline must be bytes")
    core = input_log.assess(core_data, expected_pid=expected_pid, baseline=core_baseline, expected_run_ms=expected_run_ms)
    prefix = script_data.startswith(script_baseline)
    boundary = not script_baseline or script_baseline.endswith(b"\n")
    issues, fresh, old, offset = [], [], [], 0
    if not prefix or not boundary:
        issues.append("script_baseline_mismatch")
    for line in script_data.splitlines(keepends=True):
        start, offset = offset, offset + len(line)
        is_fresh = prefix and boundary and start >= len(script_baseline)
        try:
            record = decoded_event(line)
            if record is None:
                continue
            if not line.endswith(b"\n"):
                raise ValueError("truncated event")
            (fresh if is_fresh else old).append(record)
        except (ValueError, TypeError, UnicodeError, RecursionError):
            if is_fresh and (EVENT.encode() in line or b"frontend_startup" in line):
                issues.append("invalid_startup_event")
    starts = [r for r in fresh if r["event"] == EVENT + "started"]
    ends = [r for r in fresh if r["event"] == EVENT + "finished"]
    end = ends[0] if len(ends) == 1 else None
    valid = prefix and boundary and len(starts) == len(ends) == 1 and not issues
    if valid:
        start = starts[0]
        valid = (fresh.index(start) < fresh.index(end) and start.get("core_api") == end.get("core_api") == 15 and start.get("trial_api") == end.get("trial_api") == 1 and
                 end.get("schema") == SCHEMA and unsigned(start.get("at_ms"), input_log.UINT64_MAX) and
                 end.get("started_at_ms") == start["at_ms"] and unsigned(end.get("at_ms"), input_log.UINT64_MAX) and end["at_ms"] >= start["at_ms"] and
                 all(type(end.get(k)) is bool for k in ("passed", "cleanup_passed", "cancel_requested")) and
                 (end.get("error") is None or type(end["error"]) is str) and
                 not any(r.get("at_ms") == start["at_ms"] or r.get("started_at_ms") == start["at_ms"] for r in old) and
                 report_valid(end.get("lifecycle_values")) and type(end.get("state")) is list and len(end["state"]) == 19 and
                 all(unsigned(x) for x in end["state"]) and not any(end["state"][:8]) and
                 end["state"][8] == end["lifecycle_values"][1] and end["state"][9] <= 5 and
                 end["state"][11] <= 3 and end["state"][12] <= 3 and end["state"][16] <= 25 and end["state"][17] <= 1)
    matching = False
    clean = workload = False
    if valid:
        worker, report, values = end["state"], end["lifecycle_values"], end.get("input_values")
        samples = [r for r in fresh if r["event"] == EVENT + "sample"]
        previous = None
        for sample in samples:
            v = sample.get("input_values")
            at = sample.get("at_ms")
            if (not fresh.index(starts[0]) < fresh.index(sample) < fresh.index(end) or
                    not input_log.fields_valid(v) or not unsigned(at, input_log.UINT64_MAX) or not starts[0]["at_ms"] <= at <= end["at_ms"] or
                    previous is not None and (v[2] <= previous[2] or v[3] < previous[3])):
                issues.append("invalid_sample_sequence")
            previous = v if input_log.fields_valid(v) else None
        if values is not None:
            # Match owner identity as well as copied bytes. Internally coherent
            # lifecycle snapshots from another worker cannot describe this GUI.
            same_owner = input_log.fields_valid(values) and all(not values[i] or values[i] == report[2] for i in (81, 98, 130, 178))
            matching = core["diagnostic_valid"] and same_owner and values == core["values"] and (not samples or samples[-1].get("input_values") == values)
            input_clean = bool(matching and core["cleanup_complete"])
        else:
            input_clean = (no_view_cleanup(report) and core["fresh_matching_records"] == 0 and core["baseline_prefix_retained"] and
                           core["baseline_boundary_complete"] and core["baseline_hash_matches"] and not any(issue["fresh"] for issue in core["issues"]))
        worker_clean = (worker[13] == 1 and worker[14] == worker[18] == 0 and not (worker[12] & ~worker[11]) and
                        worker[15] == (values[57] if values is not None and matching else 0) and worker[10] == 0)
        clean = bool(not issues and input_clean and worker_clean and report[65] == 4 and report[78] == report[87] == 1 and end["cleanup_passed"])
        workload = bool(clean and matching and lifecycle_verified(report) and worker[8:11] == [2, 0, 0] and worker[16:18] == [0, 1] and
                        core["own_frontend_input_workload_verified"] and not end["cancel_requested"] and end["error"] is None and end["passed"])
    return dict(schema=SCHEMA, diagnostic_valid=bool(valid and not issues), cleanup_complete=clean,
                completed_frontend_workload_verified=workload, lifecycle_retention_verified=bool(workload),
                core_terminal_matches_script=bool(matching), core_input=core, issues=issues, terminal=end,
                script_baseline_prefix_retained=prefix, script_baseline_boundary_complete=boundary,
                script_sha256=hashlib.sha256(script_data).hexdigest(), script_baseline_sha256=hashlib.sha256(script_baseline).hexdigest(),
                script_pid_independently_verified=False, module_unloading_independently_verified=False,
                camera_capture_verified=False, visible_gta_drawing_verified=False, gameplay_ready=False, full_mta_ready=False, scope=NOTICE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core-log", required=True, type=Path)
    parser.add_argument("--script-log", required=True, type=Path)
    parser.add_argument("--pid", required=True, type=int)
    parser.add_argument("--run-ms", type=int)
    core_choice = parser.add_mutually_exclusive_group(required=True)
    core_choice.add_argument("--core-baseline", type=Path)
    core_choice.add_argument("--core-baseline-absent", action="store_true")
    script_choice = parser.add_mutually_exclusive_group(required=True)
    script_choice.add_argument("--script-baseline", type=Path)
    script_choice.add_argument("--script-baseline-absent", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    inputs = [p for p in (args.core_log, args.script_log, args.core_baseline, args.script_baseline) if p]
    if args.output and any(args.output.resolve() == p.resolve() or args.output.exists() and args.output.samefile(p) for p in inputs):
        parser.error("output must differ from saved evidence")
    try:
        result = assess(args.core_log.read_bytes(), args.script_log.read_bytes(), expected_pid=args.pid, expected_run_ms=args.run_ms,
                        core_baseline=args.core_baseline.read_bytes() if args.core_baseline else b"",
                        script_baseline=args.script_baseline.read_bytes() if args.script_baseline else b"")
    except ValueError as error:
        parser.error(str(error))
    text = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0 if result["completed_frontend_workload_verified"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
