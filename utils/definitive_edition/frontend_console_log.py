"""Assess saved Console32 ownership and command counters without launching GTA."""
import argparse
import hashlib
import json
from pathlib import Path

SCHEMA = "mta_core_frontend_console_v1"
FIELD_COUNT = 32
UINT32_MAX = (1 << 32) - 1
UINT64_MAX = (1 << 64) - 1
BOOLEAN_FIELDS = (3, 4, 5, 6, 7, 8, 9, 20, 21, 22, 23, 24, 25, 26, 27, 28)
PHASES = ("Disabled", "Starting", "Running", "Stopping", "Stopped", "Failed")
COMMANDS = ("None", "Help", "Clear", "Rejected")
NOTICE = (
    "This saved Core record binds the optional real MTA console's API command counters and CPU ownership. "
    "It does not contain widget text or prove unchanged CVars/history files, visible GTA pixels, GPU drain, "
    "native player-control restoration, camera exclusion, bridge/module lifecycle or full client startup. "
    "Those require the matched frontend/CLEO logs, owned tests and human observation."
)


def unsigned(value, maximum=UINT32_MAX):
    return type(value) is int and 0 <= value <= maximum


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def fields_valid(values):
    """Validate Console API1 scalars without treating cached text as a live owner."""
    if type(values) is not list or len(values) != FIELD_COUNT or not all(unsigned(v) for v in values):
        return False
    v = values
    if (v[0] != 1 or v[1] >= len(PHASES) or not v[2] or
            any(v[i] > 1 for i in BOOLEAN_FIELDS) or v[13] >= len(COMMANDS) or
            v[10] > 2 or v[15] > 128 or v[29] != 255 or v[31] or not v[24] or
            any(v[i] for i in (20, 25, 26, 27)) or v[19] > 4 * v[29]):
        return False
    if v[8] != bool(v[6] or v[7]):
        return False
    if v[4] and (not v[3] or not v[22] or not v[7]):
        return False
    if v[5] and not v[4] or v[22] and not v[3] or v[7] and not v[3]:
        return False
    if bool(v[10]) != bool(v[23]) or v[9] and not v[23]:
        return False
    if v[13] in (1, 2) and not v[11] or v[13] == 3 and not v[12]:
        return False
    if v[13] == 0 and (v[11] or v[12]):
        return False
    if v[15] > v[14] or v[17] and not v[15]:
        return False
    if not v[3] and any(v[i] for i in (4, 5, 7, 14, 15, 16, 17, 18, 19, 22)):
        return False
    # A never-enabled optional console can share a live menu's input request.
    # Its Disabled phase and initial clean flag do not prove parent teardown.
    if v[1] == 0 and (not v[21] or any(v[i] for i in (3, 9, 10, 11, 12, 13, 23))):
        return False
    if v[1] == 1 and v[21]:
        return False
    if v[1] == 2 and (not all(v[i] for i in (3, 9, 22, 23)) or v[10] != 2 or v[21]):
        return False
    if v[1] == 4 and not cleanup_verified(v):
        return False
    return True


def cleanup_verified(v):
    # This is only the optional CPU console/command/input boundary. Retained
    # byte counts describe the destroyed widgets' last text, not GPU owners.
    return (v[1] in (0, 4) and v[21] == 1 and not v[28] and
            not any(v[i] for i in (4, 5, 6, 7, 8, 9, 10, 22, 23)))


def command_workload_verified(v):
    # Clear intentionally permits empty output. Help needs actual memo bytes;
    # no output text or command parameters cross this independent protocol.
    return (cleanup_verified(v) and v[1] == 4 and v[3] == 1 and v[14] >= 1 and v[11] >= 1 and
            v[13] in (1, 2) and (v[13] != 1 or v[18] > 0))


def assess(data, *, expected_pid, baseline, expected_run_ms=None, baseline_sha256=None):
    """Bind exactly one fresh terminal record to an explicit PID and retained baseline.

    ``baseline=b''`` explicitly describes an absent/empty pretrial log. The
    caller must capture that baseline before the trial; an old pass alone is
    not fresh evidence. An optional hash binds the supplied baseline itself.
    """
    if type(data) is not bytes or type(baseline) is not bytes:
        raise ValueError("log and baseline must be bytes")
    if not unsigned(expected_pid) or not expected_pid:
        raise ValueError("expected PID must be a nonzero uint32")
    if expected_run_ms is not None and not unsigned(expected_run_ms, UINT64_MAX):
        raise ValueError("expected runMs must be uint64")
    if baseline_sha256 is not None and (type(baseline_sha256) is not str or len(baseline_sha256) != 64 or
                                       any(character not in "0123456789abcdef" for character in baseline_sha256)):
        raise ValueError("baseline SHA256 must be lowercase hexadecimal")
    issues, records, offset = [], [], 0
    prefix = data.startswith(baseline)
    complete_boundary = not baseline or baseline.endswith(b"\n")
    hash_match = baseline_sha256 is None or hashlib.sha256(baseline).hexdigest() == baseline_sha256
    scope_proven = prefix and complete_boundary and hash_match
    if not prefix:
        issues.append(dict(reason="baseline_prefix_mismatch", fresh=True))
    if not complete_boundary:
        issues.append(dict(reason="baseline_not_complete_line", fresh=True))
    if not hash_match:
        issues.append(dict(reason="baseline_sha256_mismatch", fresh=True))
    for number, line in enumerate(data.splitlines(keepends=True), 1):
        start, offset = offset, offset + len(line)
        fresh = scope_proven and start >= len(baseline)
        try:
            if not line.endswith(b"\n"):
                raise ValueError("truncated JSONL record")
            record = json.loads(line.decode("utf-8"), object_pairs_hook=unique_object)
            if (type(record) is not dict or set(record) != {"schema", "pid", "runMs", "values"} or
                    record["schema"] != SCHEMA or not unsigned(record["pid"]) or not record["pid"] or
                    not unsigned(record["runMs"], UINT64_MAX) or not fields_valid(record["values"])):
                raise ValueError("invalid Core frontend console scalar record")
        except (ValueError, UnicodeError, TypeError, RecursionError):
            issues.append(dict(line=number, reason="invalid_frontend_console_record", fresh=fresh))
            continue
        records.append(dict(line=number, byte_offset=start, fresh=fresh, **record))
    old_keys = {(record["pid"], record["runMs"]) for record in records if not record["fresh"]}
    fresh_pid = [record for record in records if record["fresh"] and record["pid"] == expected_pid]
    matching = [record for record in fresh_pid if expected_run_ms is None or record["runMs"] == expected_run_ms]
    # Core emits one terminal line per run. Repeated lines, including an exact
    # duplicate, cannot stand in for a single attributable completed trial.
    if len(fresh_pid) > 1:
        issues.append(dict(reason="ambiguous_fresh_pid_records", fresh=True))
    if any((record["pid"], record["runMs"]) in old_keys for record in matching):
        issues.append(dict(reason="run_identity_inherited_from_baseline", fresh=True))
    valid = scope_proven and len(matching) == 1 and not any(issue["fresh"] for issue in issues)
    values = matching[0]["values"] if valid else None
    clean = valid and cleanup_verified(values)
    workload = valid and command_workload_verified(values)
    return dict(schema=SCHEMA, field_count=FIELD_COUNT, expected_pid=expected_pid, expected_run_ms=expected_run_ms,
                diagnostic_valid=valid, cleanup_complete=clean, console_command_workload_verified=workload,
                console_created=bool(valid and values[3]),
                command_execution_observed=bool(valid and values[3] and values[14] and values[11]),
                command_refusals_observed=bool(valid and values[3] and values[12]),
                history_selection_calls_observed=bool(valid and values[16]),
                autocomplete_observed=bool(valid and values[17] and values[15]),
                configuration_read_only_declared=bool(valid and values[24]),
                native_control_command_verified=False, camera_capture_verified=False,
                visible_gta_drawing_verified=False, gui_gpu_cleanup_verified=False,
                gameplay_ready=False, full_mta_ready=False, bridge_worker_lifecycle_verified=False,
                network_started=False, history_file_mutation_verified=False,
                baseline_bytes=len(baseline), baseline_sha256=hashlib.sha256(baseline).hexdigest(),
                baseline_prefix_retained=prefix, baseline_boundary_complete=complete_boundary,
                baseline_hash_matches=hash_match, log_bytes=len(data), log_sha256=hashlib.sha256(data).hexdigest(),
                fresh_matching_records=len(matching), records=records, issues=issues,
                run_ms=matching[0]["runMs"] if valid else None, values=values,
                phase=PHASES[values[1]] if valid else None,
                last_command=COMMANDS[values[13]] if valid else None, scope=NOTICE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--pid", required=True, type=int)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--baseline", type=Path)
    selection.add_argument("--baseline-absent", action="store_true")
    parser.add_argument("--baseline-sha256")
    parser.add_argument("--run-ms", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    paths = [args.log] + ([args.baseline] if args.baseline else [])
    if args.output and any(args.output.resolve() == path.resolve() or
                           (args.output.exists() and args.output.samefile(path)) for path in paths):
        parser.error("output must differ from saved evidence")
    try:
        result = assess(args.log.read_bytes(), expected_pid=args.pid,
                        baseline=args.baseline.read_bytes() if args.baseline else b"",
                        expected_run_ms=args.run_ms, baseline_sha256=args.baseline_sha256)
    except ValueError as error:
        parser.error(str(error))
    text = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0 if result["console_command_workload_verified"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
