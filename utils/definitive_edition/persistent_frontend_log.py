"""Match saved renewable MTA frontend lifetimes, leases and copied input records."""
import argparse
import hashlib
import json
from pathlib import Path
import re

import frontend_console_log as console_log
import gui_frontend_input_log as input_log
import gui_frontend_startup_log as startup_log
from gui_frontend_log import UINT32_MAX, UINT64_MAX, pair, unsigned, unique_object

SCHEMA = "mta_core_persistent_frontend_v1"
EVENT = "core_persistent_frontend_"
FIELD_COUNT = 288
NOTICE = (
    "Fresh saved Core and script records prove copied frontend lifecycle/lease counters only. "
    "They do not independently verify script PID, module unloading, camera capture, visible GTA pixels, "
    "script player-control commands, networking or full game readiness. Baseline resets are never ignored."
)


def fields_valid(v):
    # Share unchanged component invariants, not the old bounded workload's
    # duration or schema. Input256 APIs cannot describe this separate record.
    if (type(v) is not list or len(v) != FIELD_COUNT or not all(unsigned(x) for x in v) or
            not input_log.fields_valid(v[:256])):
        return False
    return (v[85] == 0 and v[256] == 1 and pair(v, 257) > 0 and
            all(v[i] <= 1 for i in (259, 262, 263)) and v[264] <= 6 and
            v[265] == 1000 and v[266] == 500 and v[269] <= 5 and not any(v[270:]) and
            (not v[262] or pair(v, 260) > 0 and not v[263] and not v[264]) and
            (not v[263] or pair(v, 260) > 0 and not v[262] and v[264] > 0) and
            (not v[236] or pair(v, 260) > 0) and
            (v[1] != 2 or not v[262] and v[269] > 0))


def cleanup_verified(v):
    return (fields_valid(v) and input_log.cleanup_verified(v[:256]) and not v[262] and
            (not pair(v, 260) or v[263] == 1 and v[264] > 0) and v[269] > 0)


def workload_verified(v):
    # The shared GUI counters retain their positions, but persistence needs its
    # own positive proof: no display timeout, renewed leases beyond 35 seconds,
    # an actual GUI close and a still-fresh host at orderly retirement.
    return (cleanup_verified(v) and input_log.native_neutral_verified(v[:256]) and
            v[4] == 1 and not v[7] and
            all(v[i] == 1 for i in (8, 10, 11, 19, 20, 21, 30, 31, 32, 37, 55, 56, 58,
                                  60, 61, 62, 63, 64, 65, 66, 78, 79, 80, 86, 96, 106, 107, 108, 109,
                                  113, 118, 124, 128, 131, 149, 152, 160, 161, 171)) and
            v[18] == 2 and all(pair(v, i) for i in (12, 14, 16, 33)) and
            not v[22] and not v[23] and v[24] > 0 and v[27] == 87 and
            0 < v[28] <= 16384 and 0 < v[29] <= 16384 and pair(v, 35) >= pair(v, 33) and
            pair(v, 35) != UINT64_MAX and v[38] > 10000 and not v[42] and v[46] > 0 and
            v[47] == 9 and not v[48] and not v[52] and v[68] >= 2 and v[67] == v[68] + v[87] and
            not v[69] and v[70] >= v[68] and not v[73] and v[81] > 0 and v[82] > 0 and
            v[83] == 1 and v[84] > 0 and v[99] >= 2 and not v[172] and
            v[242] > 35000 and v[3] >= v[242] and (v[103] == 1 or v[173] == 1) and
            v[259] == 1 and v[263] == 1 and v[264] == 5 and pair(v, 267) > 0 and v[269] == 1)


def _baseline(data, baseline, expected_hash, name):
    if type(data) is not bytes or type(baseline) is not bytes:
        raise ValueError(name + " log and explicit baseline must be bytes")
    if expected_hash is not None and (type(expected_hash) is not str or not re.fullmatch("[0-9a-f]{64}", expected_hash)):
        raise ValueError(name + " baseline SHA256 must be lowercase hexadecimal")
    return dict(prefix=data.startswith(baseline), boundary=not baseline or baseline.endswith(b"\n"),
                hash_matches=expected_hash is None or hashlib.sha256(baseline).hexdigest() == expected_hash,
                baseline_sha256=hashlib.sha256(baseline).hexdigest(), log_sha256=hashlib.sha256(data).hexdigest(),
                baseline_bytes=len(baseline), log_bytes=len(data))


def _core_records(data, baseline, facts):
    issues, records, offset = [], [], 0
    proven = all(facts[k] for k in ("prefix", "boundary", "hash_matches"))
    for number, line in enumerate(data.splitlines(keepends=True), 1):
        start, offset = offset, offset + len(line)
        fresh = proven and start >= len(baseline)
        try:
            if not line.endswith(b"\n"):
                raise ValueError("truncated record")
            record = json.loads(line.decode("utf-8"), object_pairs_hook=unique_object)
            if (type(record) is not dict or set(record) != {"schema", "pid", "runMs", "values"} or record["schema"] != SCHEMA or
                    not unsigned(record["pid"]) or not record["pid"] or not unsigned(record["runMs"], UINT64_MAX) or
                    not fields_valid(record["values"])):
                raise ValueError("invalid scalar record")
            records.append(dict(line=number, byte_offset=start, fresh=fresh, **record))
        except (ValueError, TypeError, UnicodeError, RecursionError):
            issues.append(dict(line=number, reason="invalid_persistent_frontend_record", fresh=fresh))
    return records, issues


def _decode_event(line):
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


def _script_records(data, baseline, facts):
    issues, fresh, old, offset = [], [], [], 0
    proven = all(facts[k] for k in ("prefix", "boundary", "hash_matches"))
    for number, line in enumerate(data.splitlines(keepends=True), 1):
        start, offset = offset, offset + len(line)
        is_fresh = proven and start >= len(baseline)
        try:
            record = _decode_event(line)
            if record is None:
                continue
            if not line.endswith(b"\n"):
                raise ValueError("truncated event")
            (fresh if is_fresh else old).append(record)
        except (ValueError, TypeError, UnicodeError, RecursionError):
            if is_fresh and EVENT.encode() in line:
                issues.append(dict(line=number, reason="invalid_persistent_frontend_event", fresh=True))
    return fresh, old, issues


def _ordered_samples(samples, start, end, generation):
    previous = None
    previous_at = start["at_ms"]
    for sample in samples:
        v, at = sample.get("input_values"), sample.get("at_ms")
        if (not fields_valid(v) or not unsigned(at, UINT64_MAX) or not previous_at <= at <= end["at_ms"] or
                not unsigned(sample.get("elapsed_ms"), UINT64_MAX) or sample["elapsed_ms"] != at - start["at_ms"] or pair(v, 257) != generation):
            return False
        if previous is not None:
            if (v[2] <= previous[2] or v[3] < previous[3] or
                    any(v[i] < previous[i] for i in (99, 139, 142, 143, 165, 187, 242)) or
                    any(pair(v, i) < pair(previous, i) for i in (212, 214, 216, 218, 220, 222, 224, 226, 267)) or
                    pair(previous, 260) and pair(v, 260) != pair(previous, 260) or
                    previous[263] and not v[263]):
                return False
        previous, previous_at = v, at
    return True


def assess(core_data, script_data, *, expected_pid, core_baseline, script_baseline, expected_run_ms=None,
           core_baseline_sha256=None, script_baseline_sha256=None, console_data=None, console_baseline=None):
    if not unsigned(expected_pid) or not expected_pid:
        raise ValueError("expected PID must be nonzero uint32")
    if expected_run_ms is not None and not unsigned(expected_run_ms, UINT64_MAX):
        raise ValueError("expected runMs must be uint64")
    core_facts = _baseline(core_data, core_baseline, core_baseline_sha256, "Core")
    script_facts = _baseline(script_data, script_baseline, script_baseline_sha256, "script")
    records, issues = _core_records(core_data, core_baseline, core_facts)
    fresh, old, script_issues = _script_records(script_data, script_baseline, script_facts)
    issues += script_issues
    for name, facts in (("core", core_facts), ("script", script_facts)):
        for field in ("prefix", "boundary", "hash_matches"):
            if not facts[field]:
                issues.append(dict(reason=name + "_baseline_" + field + "_mismatch", fresh=True))
    fresh_pid = [r for r in records if r["fresh"] and r["pid"] == expected_pid]
    matching = [r for r in fresh_pid if expected_run_ms is None or r["runMs"] == expected_run_ms]
    old_runs = {(r["pid"], r["runMs"]) for r in records if not r["fresh"]}
    if len(fresh_pid) > 1 or any((r["pid"], r["runMs"]) in old_runs for r in matching):
        issues.append(dict(reason="ambiguous_or_replayed_core_identity", fresh=True))
    starts = [r for r in fresh if r["event"] == EVENT + "started"]
    generations = [r for r in fresh if r["event"] == EVENT + "generation"]
    ends = [r for r in fresh if r["event"] == EVENT + "finished"]
    samples = [r for r in fresh if r["event"] == EVENT + "sample"]
    end = ends[0] if len(ends) == 1 else None
    valid = not any(i["fresh"] for i in issues) and len(starts) == len(generations) == len(ends) == 1
    if valid:
        start, generation_event = starts[0], generations[0]
        generation = generation_event.get("generation")
        valid = (fresh.index(start) < fresh.index(generation_event) < fresh.index(end) and
                 all(unsigned(start.get(k)) for k in ("core_api", "trial_api", "fields", "input_fields", "lifecycle_fields",
                                                     "maximum_duration_ms", "display_duration_ms", "host_heartbeat_budget_ms", "native_heartbeat_budget_ms")) and
                 all(unsigned(end.get(k)) for k in ("core_api", "trial_api", "generation", "samples")) and
                 unsigned(generation) and generation > 0 and generation_event.get("at_ms") == start.get("at_ms") and
                 start.get("core_api") == end.get("core_api") == 16 and start.get("trial_api") == end.get("trial_api") == 1 and
                 start.get("schema") == end.get("schema") == SCHEMA and start.get("fields") == 288 and
                 start.get("input_fields") == 288 and start.get("lifecycle_fields") == 96 and
                 start.get("maximum_duration_ms") == start.get("display_duration_ms") == 0 and
                 start.get("host_heartbeat_budget_ms") == 1000 and start.get("native_heartbeat_budget_ms") == 500 and
                 unsigned(start.get("at_ms"), UINT64_MAX) and unsigned(end.get("at_ms"), UINT64_MAX) and
                 end["at_ms"] >= start["at_ms"] and end.get("started_at_ms") == start["at_ms"] and end.get("generation") == generation and
                 unsigned(end.get("elapsed_ms"), UINT64_MAX) and end["elapsed_ms"] <= end["at_ms"] - start["at_ms"] and
                 all(type(end.get(k)) is bool for k in ("passed", "cleanup_passed", "cancel_requested", "host_lease_lost")) and
                 type(end.get("host_heartbeat_rejected", False)) is bool and
                 (end.get("error") is None or type(end["error"]) is str) and end.get("samples") == len(samples) and
                 not any(r.get("at_ms") == start["at_ms"] or r.get("started_at_ms") == start["at_ms"] or
                         r.get("generation") == generation for r in old) and
                 startup_log.report_valid(end.get("lifecycle_values")) and type(end.get("state")) is list and len(end["state"]) == 19 and
                 all(unsigned(x) for x in end["state"]) and not any(end["state"][:8]) and
                 end["state"][8] == end["lifecycle_values"][1] and end["state"][9] <= 5 and
                 end["state"][11] <= 3 and end["state"][12] <= 3 and end["state"][14] <= 1 and
                 end["state"][16] <= 25 and end["state"][17] <= 1 and
                 all(type(end.get(k, False)) is bool and not end.get(k, False) for k in
                     ("network_active", "full_core_ready", "gameplay_ready", "full_mta_ready", "camera_capture_verified")) and
                 all(fresh.index(generation_event) < fresh.index(s) < fresh.index(end) for s in samples) and
                 _ordered_samples(samples, start, end, generation))
    values = matching[0]["values"] if len(matching) == 1 else None
    matching_input = clean = workload = False
    console = None
    if valid:
        worker, report, script_values = end["state"], end["lifecycle_values"], end.get("input_values")
        matching_input = bool(values is not None and fields_valid(script_values) and values == script_values and
                              pair(values, 257) == generation and
                              all(not values[i] or values[i] == report[2] for i in (81, 98, 130, 178)) and
                              (not samples or samples[-1]["input_values"] == values))
        if script_values is not None and (not fields_valid(script_values) or end["host_lease_lost"] != (script_values[269] == 3)):
            valid = False
        worker_clean = (worker[13] == 1 and worker[18] == 0 and not (worker[12] & ~worker[11]) and worker[10] == 0 and
                        worker[15] == (values[57] if matching_input else 0))
        stopped_clean = report[65] == 4 and report[67] == report[78] == report[87] == 1
        no_view = (script_values is None and not samples and startup_log.no_view_cleanup(report) and not matching and
                   not any(i["fresh"] for i in issues))
        clean = bool(valid and worker_clean and stopped_clean and end["cleanup_passed"] and
                     (matching_input and cleanup_verified(values) or no_view))
        workload = bool(clean and matching_input and workload_verified(values) and startup_log.lifecycle_verified(report) and
                        worker[8:11] == [2, 0, 0] and worker[16:18] == [0, 1] and
                        not end["cancel_requested"] and not end["host_lease_lost"] and end["error"] is None and end["passed"] and
                        end["at_ms"] - start["at_ms"] > 35000)
    if console_data is not None:
        if console_baseline is None:
            raise ValueError("optional console log needs an explicit baseline")
        console = console_log.assess(console_data, expected_pid=expected_pid, baseline=console_baseline,
                                     expected_run_ms=matching[0]["runMs"] if len(matching) == 1 else expected_run_ms)
        console_matches = bool(matching_input and console["diagnostic_valid"] and console["values"] == values[128:160])
        if not console_matches:
            clean = workload = False
    else:
        console_matches = None
    return dict(schema=SCHEMA, field_count=FIELD_COUNT, diagnostic_valid=bool(valid), cleanup_complete=clean,
                persistent_frontend_workload_verified=workload, lifecycle_retention_verified=workload,
                core_terminal_matches_script=matching_input, optional_console_matches_input=console_matches, console=console,
                native_scoped_neutral_verified=bool(workload), expected_pid=expected_pid, expected_run_ms=expected_run_ms,
                run_ms=matching[0]["runMs"] if matching_input else None, values=values, terminal=end,
                core_baseline=core_facts, script_baseline=script_facts, core_records=records,
                fresh_matching_records=len(matching), issues=issues,
                script_pid_independently_verified=False, module_unloading_independently_verified=False,
                native_player_control_command_verified=False, camera_capture_verified=False, visible_gta_drawing_verified=False,
                network_started=False, gameplay_ready=False, full_mta_ready=False, scope=NOTICE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("core", "script"):
        parser.add_argument("--" + name + "-log", required=True, type=Path)
        choice = parser.add_mutually_exclusive_group(required=True)
        choice.add_argument("--" + name + "-baseline", type=Path)
        choice.add_argument("--" + name + "-baseline-absent", action="store_true")
        parser.add_argument("--" + name + "-baseline-sha256")
    parser.add_argument("--console-log", type=Path)
    console_choice = parser.add_mutually_exclusive_group()
    console_choice.add_argument("--console-baseline", type=Path)
    console_choice.add_argument("--console-baseline-absent", action="store_true")
    parser.add_argument("--pid", required=True, type=int)
    parser.add_argument("--run-ms", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    if bool(args.console_log) != bool(args.console_baseline or args.console_baseline_absent):
        parser.error("optional console log requires its baseline or explicit absent declaration")
    inputs = [p for p in (args.core_log, args.script_log, args.core_baseline, args.script_baseline, args.console_log, args.console_baseline) if p]
    if args.output and any(args.output.resolve() == p.resolve() or args.output.exists() and args.output.samefile(p) for p in inputs):
        parser.error("output must differ from saved evidence")
    try:
        result = assess(args.core_log.read_bytes(), args.script_log.read_bytes(), expected_pid=args.pid, expected_run_ms=args.run_ms,
                        core_baseline=args.core_baseline.read_bytes() if args.core_baseline else b"",
                        script_baseline=args.script_baseline.read_bytes() if args.script_baseline else b"",
                        core_baseline_sha256=args.core_baseline_sha256, script_baseline_sha256=args.script_baseline_sha256,
                        console_data=args.console_log.read_bytes() if args.console_log else None,
                        console_baseline=args.console_baseline.read_bytes() if args.console_baseline else b"" if args.console_baseline_absent else None)
    except ValueError as error:
        parser.error(str(error))
    text = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0 if result["persistent_frontend_workload_verified"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
