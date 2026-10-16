"""Assess saved Core-owned frontend neutral-input evidence without game access."""
import argparse
import hashlib
import json
from pathlib import Path
import re

import frontend_console_log as console_log
from gui_frontend_log import REASONS, UINT32_MAX, UINT64_MAX, pair, unsigned, unique_object

SCHEMA = "mta_core_gui_frontend_input_v1"
FIELD_COUNT = 256
NOTICE = (
    "This independent protocol binds the real MTA frontend, console, configured neutral producer and scoped native input counters. "
    "It does not prove camera capture, physical key interaction, screen pixels, script player-control commands, "
    "bridge/module unloading or full client startup. Those require separately matched evidence."
)
COMMON_BOOLEANS = (5, 6, 7, 8, 9, 10, 11, 19, 20, 21, 30, 31, 32, 37, 39, 43, 44, 45,
                   49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66,
                   72, 73, 74, 75, 76, 77, 78, 79, 80, 86)


def frontend_valid(f):
    if not any(f):
        return True
    flags = (4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28)
    return (f[0] == 1 and f[1] in (1, 2, 3, 4, 5) and f[2] > 0 and f[22] == 1 and
            not any(f[i] > 1 for i in flags) and not any(f[i] for i in (14, 23, 24, 26, 27, 29, 30, 31)) and
            (f[1] != 4 or (f[17] == 1 and not any(f[i] for i in (4, 5, 6, 8, 16, 18, 19, 20, 21)))))


def bindings_valid(b):
    if not any(b):
        return True
    flags = (3, 4, 10, 12, 13, 14, 15, 16, 18, 19, 20, 21)
    if (b[0] != 1 or b[1] not in range(6) or not b[2] or b[16] != 1 or
            any(b[i] > 1 for i in flags) or any(b[18:21]) or any(b[22:32]) or
            b[6] + b[7] > b[5] or b[8] > b[6] or b[9] > b[7] or b[15] and not b[3] or
            b[11] and not all(b[i] for i in (3, 4, 12, 13))):
        return False
    if b[1] == 0 and (not b[14] or any(b[i] for i in (3, 4, 5, 6, 7, 8, 9, 11, 15))):
        return False
    if b[1] == 2 and (not all(b[i] for i in (3, 4, 10, 12, 13, 15)) or b[14]):
        return False
    return b[1] != 4 or (b[14] == 1 and not b[10] and not b[15] and not b[21])


def bindings_neutral_verified(v):
    b = v[176:208]
    # Suppression is the current aggregate GUI request. Teardown clears it;
    # cached neutral frames and both controller states preserve the workload.
    return (b[0] == 1 and b[1] == 4 and b[3] == b[4] == b[12] == b[13] == b[14] == b[16] == 1 and
            b[5] > 0 and b[6] > 0 and b[8] > 0 and b[11] >= 2 and not b[10] and not b[15] and not any(b[18:22]))


def native_cleanup_verified(v):
    # Pinning is a module lifetime fact, not an active detour. An ended scope
    # may keep Game DE pinned until process exit, independently of Core unload.
    return (pair(v, 208) == pair(v, 210) == 0 and pair(v, 214) == pair(v, 220) and
            pair(v, 222) == 0 and v[238] == 1 and not v[239] and not v[243])


def cleanup_verified(v):
    f, c, b = v[96:128], v[128:160], v[176:208]
    frontend_clean = not any(f) or (f[1] == 4 and f[17] == 1 and not any(f[i] for i in (6, 16, 19, 20, 21)))
    console_clean = not any(c) or console_log.cleanup_verified(c)
    bindings_clean = not any(b) or (b[1] in (0, 4) and b[14] == 1 and not b[10] and not b[15] and not b[21])
    return (v[1] == 2 and v[6] == 1 and not v[5] and v[9] == v[39] == 1 and not v[41] and not v[43] and
            not any(v[49:52]) and not v[59] and not any(v[75:78]) and not v[72] and
            all(v[i] == 1 for i in (78, 79, 80)) and frontend_clean and console_clean and bindings_clean and
            not v[169] and not v[170] and (not v[161] or v[171] == 1) and native_cleanup_verified(v))


def native_neutral_verified(v):
    # neutralQueries counts only unknown action queries, not zero-valued known
    # axes/sprint. Routed queries plus actual neutral producer states prove the
    # admitted scope; requiring equal query counters would reject valid scopes.
    return (cleanup_verified(v) and bindings_neutral_verified(v) and
            all(v[i] == 1 for i in (232, 236, 237, 238)) and not v[241] and
            pair(v, 212) > 0 and pair(v, 214) > 0 and pair(v, 216) > 0 and
            pair(v, 220) == pair(v, 214) and pair(v, 222) == pair(v, 226) == pair(v, 228) == 0)


def workload_verified(v):
    return (native_neutral_verified(v) and v[4] == 1 and not v[7] and
            all(v[i] == 1 for i in (8, 10, 11, 19, 20, 21, 30, 31, 32, 37, 55, 56, 58,
                                  60, 61, 62, 63, 64, 65, 66, 78, 79, 80, 86, 96, 106, 107, 108, 109,
                                  113, 118, 124, 128, 131, 149, 152, 160, 161, 171)) and
            v[18] == 2 and all(pair(v, i) for i in (12, 14, 16, 33)) and
            not v[22] and not v[23] and v[24] > 0 and v[27] == 87 and
            0 < v[28] <= 16384 and 0 < v[29] <= 16384 and pair(v, 35) >= pair(v, 33) and
            pair(v, 35) != UINT64_MAX and v[38] > 10000 and not v[42] and v[46] > 0 and
            v[47] == 9 and not v[48] and not v[52] and v[68] >= 2 and v[67] == v[68] + v[87] and
            not v[69] and v[70] >= v[68] and not v[73] and v[81] > 0 and v[82] > 0 and
            v[83] == 1 and v[84] > 0 and v[85] == 20000 and v[99] >= 2 and
            (v[242] >= 20000 or v[103] == 1 or v[173] == 1) and not v[172])


def fields_valid(v):
    if type(v) is not list or len(v) != FIELD_COUNT or not all(unsigned(value) for value in v):
        return False
    if (v[0] != 1 or v[1] not in (1, 2, 3) or not v[2] or v[4] >= len(REASONS) or
            any(v[i] > 1 for i in COMMON_BOOLEANS) or v[18] not in (0, 2) or v[23] > 6 or
            v[41] > 4 or v[42] > 10 or v[47] > 9 or v[48] > 9 or any(v[88:96]) or
            any(v[i] for i in (44, 45, 53, 54, 174, 175, 233, 234, 235, 244, 245)) or any(v[246:256]) or
            v[160] != 1 or any(v[i] > 1 for i in (161, 162, 171, 172, 173, 232, 236, 237, 238, 239, 241, 243))):
        return False
    if (v[5] and v[6] or v[9] and v[43] or v[1] == 2 and (not v[6] or not v[4]) or
            v[1] == 3 and (not v[5] or v[6]) or not v[10] and (any(v[12:19]) or v[58]) or
            v[58] and (not v[10] or v[18] != 2 or not all(pair(v, i) for i in (12, 14, 16))) or
            v[19] and (not all(v[i] for i in (8, 9, 10, 11, 58)) or v[42]) or
            v[21] and (not v[20] or not v[19]) or v[4] == 1 and (not v[20] or not v[21]) or
            v[37] and (not all(v[i] for i in (19, 30, 31, 32, 55)) or v[38] <= 10000) or
            pair(v, 35) == UINT64_MAX and not v[52] or
            v[171] and any(v[i] for i in (162, 169, 170))):
        return False
    if not frontend_valid(v[96:128]) or not bindings_valid(v[176:208]):
        return False
    c = v[128:160]
    if any(c) and not console_log.fields_valid(c):
        return False
    if (pair(v, 208) > 1 or pair(v, 210) > 1 or pair(v, 228) > 18 or pair(v, 230) > UINT32_MAX or
            pair(v, 218) > pair(v, 216) or pair(v, 220) > pair(v, 214) or pair(v, 214) > pair(v, 212) or
            v[237] and (not v[236] or not all(pair(v, i) for i in (212, 214, 216))) or
            v[238] and not native_cleanup_verified(v) or
            # Deferred frontend configuration records Enabled before native
            # admission. Focus or actual events, rather than that historical
            # switch, require the live verified native scope.
            not v[171] and any(v[i] for i in (162, 165, 166, 169, 170)) and
            (not v[237] or not pair(v, 210) or v[238]) or
            v[236] and not v[232]):
        return False
    return not v[6] or cleanup_verified(v)


def assess(data, *, expected_pid, baseline, expected_run_ms=None, baseline_sha256=None):
    if type(data) is not bytes or type(baseline) is not bytes:
        raise ValueError("log and baseline must be bytes")
    if not unsigned(expected_pid) or not expected_pid:
        raise ValueError("expected PID must be a nonzero uint32")
    if expected_run_ms is not None and not unsigned(expected_run_ms, UINT64_MAX):
        raise ValueError("expected runMs must be uint64")
    if baseline_sha256 is not None and (type(baseline_sha256) is not str or not re.fullmatch('[0-9a-f]{64}', baseline_sha256)):
        raise ValueError("baseline SHA256 must be lowercase hexadecimal")
    prefix, boundary = data.startswith(baseline), not baseline or baseline.endswith(b"\n")
    hash_match = baseline_sha256 is None or hashlib.sha256(baseline).hexdigest() == baseline_sha256
    proven, issues, records, offset = prefix and boundary and hash_match, [], [], 0
    for valid, reason in ((prefix, "baseline_prefix_mismatch"), (boundary, "baseline_not_complete_line"), (hash_match, "baseline_sha256_mismatch")):
        if not valid:
            issues.append(dict(reason=reason, fresh=True))
    for number, line in enumerate(data.splitlines(keepends=True), 1):
        start, offset = offset, offset + len(line)
        fresh = proven and start >= len(baseline)
        try:
            if not line.endswith(b"\n"):
                raise ValueError("truncated record")
            record = json.loads(line.decode("utf-8"), object_pairs_hook=unique_object)
            if (type(record) is not dict or set(record) != {"schema", "pid", "runMs", "values"} or record["schema"] != SCHEMA or
                    not unsigned(record["pid"]) or not record["pid"] or not unsigned(record["runMs"], UINT64_MAX) or not fields_valid(record["values"])):
                raise ValueError("invalid scalar record")
        except (ValueError, UnicodeError, TypeError, RecursionError):
            issues.append(dict(line=number, reason="invalid_gui_frontend_input_record", fresh=fresh))
            continue
        records.append(dict(line=number, byte_offset=start, fresh=fresh, **record))
    old = {(record["pid"], record["runMs"]) for record in records if not record["fresh"]}
    fresh_pid = [record for record in records if record["fresh"] and record["pid"] == expected_pid]
    matching = [record for record in fresh_pid if expected_run_ms is None or record["runMs"] == expected_run_ms]
    if len(fresh_pid) > 1:
        issues.append(dict(reason="ambiguous_fresh_pid_records", fresh=True))
    if any((record["pid"], record["runMs"]) in old for record in matching):
        issues.append(dict(reason="run_identity_inherited_from_baseline", fresh=True))
    valid = proven and len(matching) == 1 and not any(issue["fresh"] for issue in issues)
    values = matching[0]["values"] if valid else None
    clean = bool(valid and cleanup_verified(values))
    workload = bool(valid and workload_verified(values))
    return dict(schema=SCHEMA, field_count=FIELD_COUNT, expected_pid=expected_pid, expected_run_ms=expected_run_ms,
                diagnostic_valid=valid, cleanup_complete=clean, own_frontend_input_workload_verified=workload,
                composition_present_verified=workload, native_scoped_neutral_verified=bool(valid and native_neutral_verified(values)),
                bindings_neutral_producer_verified=bool(valid and bindings_neutral_verified(values)),
                native_input_cleanup_verified=bool(valid and native_cleanup_verified(values)),
                console_command_workload_verified=bool(valid and console_log.command_workload_verified(values[128:160])),
                native_player_control_command_verified=False, camera_capture_verified=False, visible_gta_drawing_verified=False,
                bridge_worker_lifecycle_verified=False, gameplay_ready=False, full_mta_ready=False,
                baseline_bytes=len(baseline), baseline_sha256=hashlib.sha256(baseline).hexdigest(),
                baseline_prefix_retained=prefix, baseline_boundary_complete=boundary, baseline_hash_matches=hash_match,
                log_bytes=len(data), log_sha256=hashlib.sha256(data).hexdigest(), fresh_matching_records=len(matching),
                records=records, issues=issues, run_ms=matching[0]["runMs"] if valid else None, values=values,
                reason=REASONS[values[4]] if valid else None, scope=NOTICE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--pid", required=True, type=int)
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument("--baseline", type=Path)
    choice.add_argument("--baseline-absent", action="store_true")
    parser.add_argument("--baseline-sha256")
    parser.add_argument("--run-ms", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    paths = [args.log] + ([args.baseline] if args.baseline else [])
    if args.output and any(args.output.resolve() == path.resolve() or args.output.exists() and args.output.samefile(path) for path in paths):
        parser.error("output must differ from saved evidence")
    try:
        result = assess(args.log.read_bytes(), expected_pid=args.pid, baseline=args.baseline.read_bytes() if args.baseline else b"",
                        expected_run_ms=args.run_ms, baseline_sha256=args.baseline_sha256)
    except ValueError as error:
        parser.error(str(error))
    text = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0 if result["own_frontend_input_workload_verified"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
