"""Assess saved Core-owned real MTA menu/control-handshake evidence without game access."""
import argparse
import hashlib
import json
from pathlib import Path

SCHEMA = "mta_core_gui_frontend_v1"
FIELD_COUNT = 160
UINT32_MAX = (1 << 32) - 1
UINT64_MAX = (1 << 64) - 1
BOOLEAN_FIELDS = (5, 6, 7, 8, 9, 10, 11, 19, 20, 21, 30, 31, 32, 37, 39, 43,
                  44, 45, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
                  60, 61, 62, 63, 64, 65, 66, 72, 73, 74, 75, 76, 77, 78, 79, 80, 86,
                  100, 101, 102, 103, 104, 106, 107, 108, 109, 110, 112, 113, 114,
                  115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 129, 130, 139, 140, 141,
                  143, 144, 145, 146, 147, 148, 149, 152, 154, 155, 156, 157)
REASONS = ("Running", "Completed", "Cancelled", "PrepareError", "BeginError", "NativeError",
           "DeviceWaitTimeout", "GpuError", "PulseGap", "Exception", "CleanupError", "LeaseError")
NOTICE = (
    "This saved Core record binds real MTA menu rendering and the scalar engine-control handshake. "
    "Native SET_PLAYER_CONTROL/IS_PLAYER_CONTROL_ON facts require the matched CLEO log; "
    "camera exclusion, visible screen pixels, bridge/module lifecycle and full client startup remain separate gates."
)


def unsigned(value, maximum=UINT32_MAX):
    return type(value) is int and 0 <= value <= maximum


def pair(values, field):
    return values[field] | values[field + 1] << 32


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def fields_valid(values):
    """Validate the independent Frontend160 ownership contract, including early failure."""
    if (type(values) is not list or len(values) != FIELD_COUNT or not all(unsigned(v) for v in values)):
        return False
    v = values
    if (v[0] != 1 or v[128] != 1 or v[1] not in (1, 2, 3) or not v[2] or v[4] >= len(REASONS) or
            any(v[i] > 1 for i in BOOLEAN_FIELDS) or v[18] not in (0, 2) or v[23] > 6 or
            v[41] > 4 or v[42] > 10 or v[47] > 9 or v[48] > 9 or
            any(v[88:96]) or any(v[125:128]) or any(v[158:160]) or
            any(v[i] for i in (44, 45, 53, 54, 110, 119, 120, 122, 123, 156, 157))):
        return False
    if v[5] and v[6] or v[9] and v[43]:
        return False
    if v[1] == 2 and (not v[6] or not v[4]) or v[1] == 3 and (not v[5] or v[6]):
        return False
    if not v[10] and (any(v[12:19]) or v[58]):
        return False
    if v[58] and (not v[10] or v[18] != 2 or not all(pair(v, i) for i in (12, 14, 16))):
        return False
    if v[19] and (not all(v[i] for i in (8, 9, 10, 11, 58)) or v[42]):
        return False
    if v[21] and (not v[20] or not v[19] or not v[154]):
        return False
    if v[37] and (not all(v[i] for i in (19, 30, 31, 32, 55)) or v[38] <= 10000):
        return False
    if v[4] == 1 and (not v[20] or not v[21]):
        return False
    if pair(v, 35) == UINT64_MAX and not v[52]:
        return False
    # The control generation is separate from the retained native device lease.
    # An expired/revoked lease can still be restored and safely retired.
    if not v[143] and any(v[142:150]):
        return False
    if v[143] and not v[142] or v[144] and not v[143] or v[145] and (not v[144] or v[146] or v[148] or v[149]):
        return False
    if v[147] and (not v[143] or not v[146] or v[145]):
        return False
    if v[148] and not v[147]:
        return False
    if v[154] and (not v[144] or not v[129] or not v[19]):
        return False
    if v[139] and (v[137] or v[138] or v[130]):
        return False
    f = v[96:128]
    if f[1] == 0:
        if any(f) or v[154]:
            return False
    elif (f[0] != 1 or f[1] not in (1, 2, 3, 4, 5) or not f[2] or not f[22]):
        return False
    elif f[1] == 2 and not all(f[i] for i in (10, 11, 12, 13, 21, 28)):
        return False
    elif f[1] == 4 and (not f[17] or any(f[i] for i in (6, 16, 19, 20, 21))):
        return False
    if v[155] and any(f) and (f[1] != 4 or not f[17]):
        return False
    if v[6] and not cleanup_verified(v):
        return False
    return True


def cleanup_verified(v):
    frontend_clean = not any(v[96:128]) or (v[97] == 4 and v[113] == 1 and
                     not any(v[i] for i in (102, 112, 115, 116, 117)) and v[155] == 1)
    controls_clean = not v[143] or (v[147] == 1 and v[148] == 1 and not v[145])
    input_clean = not v[137] and not v[138] and (not v[129] or v[139] == 1)
    return (v[1] == 2 and v[6] == 1 and v[5] == 0 and v[9] == 1 and v[39] == 1 and
            v[41] == 0 and v[43] == 0 and not any(v[49:52]) and not v[59] and
            not any(v[75:78]) and not v[72] and all(v[i] == 1 for i in (78, 79, 80)) and
            frontend_clean and controls_clean and input_clean)


def workload_verified(v):
    """Recompute the real-menu workload; native bool and camera proof are separate."""
    return (cleanup_verified(v) and not v[7] and v[4] == 1 and
            all(v[i] == 1 for i in (8, 10, 11, 19, 20, 21, 30, 31, 32, 37, 55, 56, 58,
                                  60, 61, 62, 63, 64, 65, 66, 78, 79, 80, 86,
                                  129, 139, 143, 144, 146, 147, 148, 154, 155)) and
            v[18] == 2 and all(pair(v, i) for i in (12, 14, 16, 33)) and
            not v[22] and not v[23] and v[24] > 0 and v[27] == 87 and
            0 < v[28] <= 16384 and 0 < v[29] <= 16384 and pair(v, 35) >= pair(v, 33) and
            pair(v, 35) != UINT64_MAX and v[38] > 10000 and not v[42] and v[46] > 0 and
            v[47] == 9 and not v[48] and not v[52] and v[68] >= 2 and v[67] == v[68] + v[87] and
            not v[69] and v[70] >= v[68] and not v[73] and v[81] > 0 and v[82] > 0 and
            v[83] == 1 and v[84] > 0 and v[85] == 30000 and v[99] >= 2 and
            (v[153] >= 30000 or v[103] == 1 or v[141] == 1) and not v[140] and not v[149] and not v[152] and v[151] == 1000)


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
                raise ValueError("invalid Core GUI frontend scalar record")
        except (ValueError, UnicodeError, TypeError, RecursionError):
            issues.append(dict(line=number, reason="invalid_gui_frontend_record", fresh=fresh))
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
    workload = valid and workload_verified(values)
    return dict(schema=SCHEMA, field_count=FIELD_COUNT, expected_pid=expected_pid, expected_run_ms=expected_run_ms,
                diagnostic_valid=valid, cleanup_complete=clean, own_frontend_workload_verified=workload,
                composition_present_verified=workload,
                control_restore_acknowledged=bool(clean and values[143] and values[147] and values[148]),
                menu_callbacks_observed=bool(workload and values[105] > 0 and values[131] > 0 and pair(values, 133) > 0),
                native_control_command_verified=False, camera_capture_verified=False, credits_close_verified=False,
                visible_gta_drawing_verified=False, full_mta_ready=False, native_queue_commands_issued=False,
                native_backbuffer_touched=False, bridge_worker_lifecycle_verified=False,
                baseline_bytes=len(baseline), baseline_sha256=hashlib.sha256(baseline).hexdigest(),
                baseline_prefix_retained=prefix, baseline_boundary_complete=complete_boundary,
                baseline_hash_matches=hash_match, log_bytes=len(data), log_sha256=hashlib.sha256(data).hexdigest(),
                fresh_matching_records=len(matching), records=records, issues=issues,
                run_ms=matching[0]["runMs"] if valid else None, values=values,
                reason=REASONS[values[4]] if valid else None, scope=NOTICE)


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
    return 0 if result["own_frontend_workload_verified"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
