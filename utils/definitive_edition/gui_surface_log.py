"""Assess saved Core-owned child composition GUI evidence without game access."""
import argparse
import hashlib
import json
from pathlib import Path

SCHEMA = "mta_core_gui_surface_v1"
FIELD_COUNT = 96
UINT32_MAX = (1 << 32) - 1
UINT64_MAX = (1 << 64) - 1
BOOLEAN_FIELDS = (5, 6, 7, 8, 9, 10, 11, 19, 20, 21, 30, 31, 32, 37, 39, 43,
                  44, 45, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
                  60, 61, 62, 63, 64, 65, 66, 72, 73, 74, 75, 76, 77, 78, 79, 80, 86)
REASONS = ("Running", "Completed", "Cancelled", "PrepareError", "BeginError", "NativeError",
           "DeviceWaitTimeout", "GpuError", "PulseGap", "Exception", "CleanupError", "LeaseError")
NOTICE = (
    "This saved Core record describes an MTA-owned queue, child composition swapchain and GUI workload. "
    "Bridge/worker lifecycle, parent identity and installed/source bindings require separate evidence. "
    "Native queue/backbuffer writes, visible screen pixels and complete MTA startup are not established."
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
    """Validate Surface96 without conflating it with the old offscreen policy."""
    if (type(values) is not list or len(values) != FIELD_COUNT or
            not all(unsigned(value) for value in values)):
        return False
    if (values[0] != 1 or values[1] not in (1, 2, 3) or not values[2] or
            values[4] >= len(REASONS) or any(values[field] > 1 for field in BOOLEAN_FIELDS) or
            values[18] not in (0, 2) or values[23] > 6 or values[41] > 4 or values[42] > 10 or
            values[47] > 9 or values[48] > 9 or any(values[88:]) or values[44] or values[45]):
        return False
    if values[5] and values[6] or values[9] and values[43]:
        return False
    # Retained surface ownership is just as real as the common GUI/device lease.
    # A terminal cleanup claim cannot conceal a window, queue or compositor root.
    if values[6] and (values[1] != 2 or values[5] or not values[9] or not values[39] or
                      values[43] or any(values[49:52]) or values[59] or values[41] or
                      any(values[75:78]) or values[72] or not all(values[field] == 1 for field in (78, 79, 80))):
        return False
    if values[1] == 2 and (not values[6] or not values[4]):
        return False
    if values[1] == 3 and (not values[5] or values[6]):
        return False
    if not values[10] and (any(values[12:19]) or values[58]):
        return False
    if values[58] and (not values[10] or values[18] != 2 or
                       not all(pair(values, field) for field in (12, 14, 16))):
        return False
    if values[19] and (not all(values[field] for field in (8, 9, 10, 11, 58)) or values[42]):
        return False
    if values[20] and not values[19] or values[21] and (not values[19] or not values[20]):
        return False
    if values[37] and (not values[20] or not values[21]):
        return False
    if values[4] == 1 and (not values[20] or not values[21]):
        return False
    if pair(values, 35) == UINT64_MAX and not values[52]:
        return False
    return True


def cleanup_verified(values):
    return (values[1] == 2 and values[6] == 1 and values[5] == 0 and values[9] == 1 and
            values[39] == 1 and values[41] == 0 and values[43] == 0 and
            not any(values[49:52]) and values[59] == 0 and not any(values[75:78]) and values[72] == 0 and all(values[field] == 1 for field in (78, 79, 80)))


def workload_verified(values):
    """Recompute only the private surface workload; screen visibility is separate."""
    return (cleanup_verified(values) and values[7] == 0 and values[4] == 1 and
            all(values[field] == 1 for field in (8, 10, 11, 19, 20, 21, 30, 31, 32, 37,
                                               53, 54, 55, 56, 58, 60, 61, 62, 63, 64, 65, 66,
                                               78, 79, 80, 86)) and
            values[18] == 2 and all(pair(values, field) for field in (12, 14, 16, 33)) and
            values[22] == 0 and values[23] == 0 and values[24] > 0 and values[27] == 87 and
            0 < values[28] <= 16384 and 0 < values[29] <= 16384 and pair(values, 35) >= pair(values, 33) and
            pair(values, 35) != UINT64_MAX and values[38] > 0 and values[42] == 0 and
            values[44] == 0 and values[45] == 0 and values[46] > 0 and values[47] == 9 and
            values[48] == 0 and values[52] == 0 and values[59] == 0 and values[68] >= 2 and values[67] == values[68] + values[87] and
            values[69] == 0 and values[70] >= values[68] and values[73] == 0 and
            values[81] > 0 and values[82] > 0 and values[83] == 1 and values[84] > 0 and values[85] == 8000 and values[3] >= 8000)


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
                raise ValueError("invalid Core GUI surface scalar record")
        except (ValueError, UnicodeError, TypeError, RecursionError):
            issues.append(dict(line=number, reason="invalid_gui_surface_record", fresh=fresh))
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
                diagnostic_valid=valid, cleanup_complete=clean, own_surface_workload_verified=workload,
                composition_present_verified=workload,
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
    return 0 if result["own_surface_workload_verified"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
