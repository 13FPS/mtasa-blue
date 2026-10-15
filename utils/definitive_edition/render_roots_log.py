"""Read saved, pointer-free native-root diagnostics without granting frame admission."""
import argparse
import hashlib
import json
from pathlib import Path

API_VERSION, FIELD_COUNT, RECORD_LIMIT = 1, 14, 32
CHECKPOINTS = (
    "NotAttempted", "ViewportTable", "Owner", "Device", "Manager", "ManagerTable", "Queue", "Swapchain",
    "CustomPresent", "GpuCount", "AfrCount", "ContextRoot", "Context", "ContextDevice", "Alternate", "NodeMask",
    "ContextManager", "CommandData", "CommandManager", "BufferIndex", "SelectorA", "SelectorB", "Texture",
    "NativeTexture", "View", "Resource", "ResourceTable", "RawResource", "TrackedState", "ReadComplete",
    "CustomClassification", "AssociationValidation",
)
QUERY_PHASES = ("NotAttempted", "NativeRoots", "BeforeCloseReadback", "AfterCloseReadback")
REASONS = ("None", "GuardedRead", "RequiredNull", "TableMismatch", "TextureBias", "StructuredException")
NOTICE = (
    "Separate producer-reported root checkpoints and known scalar values only. A diagnostic record, including "
    "ReadComplete or AssociationValidation without a refusal, never proves frame association, GPU execution or "
    "drawing. This report does not identify the cause or failed root of an earlier trial without this diagnostic."
)


def uint32(value):
    return type(value) is int and 0 <= value <= 0xffffffff


def unsigned_array(value, width):
    return isinstance(value, list) and len(value) == width and all(uint32(item) for item in value)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def split(fields, offset):
    return fields[offset] | fields[offset + 1] << 32


def fields_valid(fields):
    if not unsigned_array(fields, FIELD_COUNT) or fields[0] != API_VERSION or not split(fields, 1) or not split(fields, 3):
        return False
    phase, checkpoint, reason, exception, mask, node, index = fields[7:]
    if phase > 3 or checkpoint > 31 or reason > 5 or mask > 3:
        return False
    # Validity bits preserve the distinction between an unread scalar and a
    # known zero. Unknown payload slots must stay zero and cannot imply a read.
    if (not mask & 1 and node) or (not mask & 2 and index):
        return False
    if reason != 5 and exception:
        return False
    if not phase:
        return not any(fields[8:])
    if not checkpoint or not split(fields, 5):
        return False
    # Readback phases execute ReadRoots only. Classification/association belong
    # to the initial query, and scalar validity follows successful read order.
    if phase > 1 and checkpoint > 29:
        return False
    if checkpoint < 15:
        allowed_masks = (0,)
    elif checkpoint == 15:
        allowed_masks = (0, 1) if reason == 5 else (0,)
    elif checkpoint < 19:
        allowed_masks = (1,)
    elif checkpoint == 19:
        allowed_masks = (1, 3) if reason == 5 else (1,)
    else:
        allowed_masks = (3,)
    if mask not in allowed_masks:
        return False
    if reason == 0:
        return checkpoint in (29, 31)
    if reason == 1:
        return checkpoint <= 28
    if reason == 2:
        return checkpoint in (2, 3, 4, 6, 7, 11, 12, 17, 22, 24, 25, 27)
    if reason == 3:
        return checkpoint in (1, 5, 26)
    if reason == 4:
        return checkpoint == 23
    return True


def analyze_log(data, baseline=b"", baseline_existed=False, expected_pid=None, expected_run=None, expected_generation=None):
    """Attribute only fresh coherent records to the explicitly known test identity.

    Historical runs remain visible but cannot supply a missing record or scalar
    for a new trial. Caller-supplied run/generation selectors narrow attribution.
    """
    if not isinstance(data, bytes) or not isinstance(baseline, bytes):
        raise ValueError("log and baseline must be bytes")
    if type(baseline_existed) is not bool or (not baseline_existed and baseline):
        raise ValueError("absent baseline must be empty")
    if expected_pid is not None and (not uint32(expected_pid) or not expected_pid):
        raise ValueError("expected PID must be a nonzero uint32")
    for selector in (expected_run, expected_generation):
        if selector is not None and not unsigned_array(selector, 2):
            raise ValueError("run/generation selector must contain two uint32 values")
    prefix = data.startswith(baseline)
    boundary = len(baseline) if baseline_existed and prefix else 0
    scope_proven = (not baseline_existed or prefix) and (not boundary or baseline.endswith(b"\n"))
    relationship = ("new_generated_roots_log" if not baseline_existed else "unchanged_roots_log" if data == baseline else
                    "appended_whole_roots_log" if prefix else "replaced_whole_roots_log")
    issues, valid, offset = [], [], 0
    for number, line in enumerate(data.splitlines(keepends=True), 1):
        start, offset = offset, offset + len(line)
        if not line.strip():
            continue
        fresh = scope_proven and start >= boundary
        try:
            value = json.loads(line.decode("utf-8-sig"), object_pairs_hook=unique_object)
            if (set(value) != {"event", "pid", "run_ms", "phase", "fields"} or value["event"] != "de_render_roots" or
                    not uint32(value["pid"]) or not value["pid"] or not unsigned_array(value["run_ms"], 2) or
                    not uint32(value["phase"]) or value["phase"] > 2 or not fields_valid(value["fields"])):
                raise ValueError("invalid scalar record")
        except (ValueError, UnicodeError, TypeError, RecursionError):
            issues.append(dict(line=number, reason="invalid_pointer_free14_scalar_record", afterBaseline=fresh))
            continue
        valid.append(dict(line=number, byteOffset=start, pid=value["pid"], runMs=value["run_ms"], phase=value["phase"],
                          fields=value["fields"], afterBaseline=fresh))
    if not scope_proven:
        issues.append(dict(reason="whole_baseline_prefix_unproven", afterBaseline=True))
    groups = {}
    for value in valid:
        key = (value["pid"], *value["runMs"])
        groups.setdefault(key, []).append(value)
    runs = []
    for key, rows in groups.items():
        phases = [row["phase"] for row in rows]
        generation = rows[0]["fields"][1:3]
        coherent = (len(rows) <= RECORD_LIMIT and phases == sorted(phases) and phases.count(0) <= 1 and
                    phases.count(2) <= 1 and sum(phase != 2 for phase in phases) < RECORD_LIMIT and
                    all(row["fields"][1:3] == generation for row in rows))
        for before, after in zip(rows, rows[1:]):
            coherent = coherent and all(split(after["fields"], at) >= split(before["fields"], at) for at in (3, 5))
        fresh = [row for row in rows if row["afterBaseline"]]
        if not coherent:
            issues.append(dict(pid=key[0], runMs=list(key[1:]), reason="incoherent_or_unbounded_roots_run", afterBaseline=bool(fresh)))
        final = rows[-1]["fields"]
        runs.append(dict(pid=key[0], runMs=list(key[1:]), generation=generation, records=rows, coherent=coherent,
                         complete=coherent and 1 in phases and phases[-1] == 2,
                         entirelyAfterBaseline=bool(fresh) and len(fresh) == len(rows), sampleCount=len(fresh),
                         acceptedSnapshots=split(final, 3), dispatchEntries=split(final, 5), queryPhase=final[7],
                         queryPhaseName=QUERY_PHASES[final[7]], checkpoint=final[8], checkpointName=CHECKPOINTS[final[8]],
                         refusalReason=final[9], refusalReasonName=REASONS[final[9]], exceptionCode=final[10],
                         scalarValidMask=final[11], contextNodeMask=final[12] if final[11] & 1 else None,
                         nativeBufferIndex=final[13] if final[11] & 2 else None))
    matching = [run for run in runs if expected_pid is not None and run["pid"] == expected_pid and run["coherent"] and
                run["entirelyAfterBaseline"] and (expected_run is None or run["runMs"] == expected_run) and
                (expected_generation is None or run["generation"] == expected_generation)]
    if len(matching) > 1:
        issues.append(dict(reason="ambiguous_fresh_roots_runs", afterBaseline=True))
    new_issues = [issue for issue in issues if issue["afterBaseline"]]
    observed = len(matching) == 1 and not new_issues and scope_proven
    return dict(apiVersion=API_VERSION, fieldCount=FIELD_COUNT, observed=observed, diagnosticObserved=observed,
                sampleCount=matching[0]["sampleCount"] if observed else 0, allValidRuns=runs, matchingNewRuns=matching,
                expectedPid=expected_pid, expectedRunMs=expected_run, expectedGeneration=expected_generation,
                pidBound=expected_pid is not None, baselinePrefixRetained=prefix, baselineRelationship=relationship,
                baselineBytes=len(baseline), issueCount=len(issues), newIssueCount=len(new_issues), issues=issues,
                status="roots_diagnostic_observed" if observed else "roots_diagnostic_unattributed_or_missing",
                rootsAdmissionVerified=False, rootsAdmissionGrantedByDiagnostic=False, associationEvidenceFromDiagnostic=False,
                nativePresentObserved=False, nativeDispatchObserved=False, bufferAssociationObserved=False,
                nativeDispatchBufferAssociationObserved=False, gpuOwnershipVerified=False, gpuFrameBoundaryVerified=False,
                gpuCommandsIssued=False, visualRenderingVerified=False, fullMtaReady=False, earlierTrialRootCauseEstablished=False,
                scope=NOTICE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--run-ms", type=int, nargs=2)
    parser.add_argument("--generation", type=int, nargs=2)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    paths = [args.log] + ([args.baseline] if args.baseline else [])
    if any(args.output.resolve() == path.resolve() or (args.output.exists() and args.output.samefile(path)) for path in paths):
        parser.error("output must differ from log and baseline evidence")
    data = args.log.read_bytes()
    result = analyze_log(data, args.baseline.read_bytes() if args.baseline else b"", args.baseline is not None,
                         args.pid, args.run_ms, args.generation)
    result.update(log=str(args.log.resolve()), logFingerprint=dict(sha256=hashlib.sha256(data).hexdigest(), bytes=len(data)),
                  readerPath=str(Path(__file__).resolve()), readerSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return 0 if result["observed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
