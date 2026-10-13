"""Read CLEO logs to verify Diagnostic script markers; never launch or inspect GTA."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys
import time


DEFAULT_LOG = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition"
) / "Gameface" / "Binaries" / "Win64" / "cleo_redux.log"
MAX_LOG_BYTES = 16 * 1024 * 1024
INACTIVE_FLAGS = ("bridge_loaded", "game_adapter_active", "network_active")
BRIDGE_PHASES = {
    "bridge_load", "bridge_loading", "bridge_loaded", "bridge_abi_check", "game_build_check",
    "game_build_verified", "runtime_bind", "runtime_bound", "controller_run",
}
NOTICE = (
    "Log markers verify only the Diagnostic script path. They do not establish gameplay stability, "
    "rendering, multiplayer, or GPU health."
)


def json_event(line):
    """CLEO prefixes JSON messages and may JSON-quote the entire message again."""
    decoder = json.JSONDecoder()
    for match in re.finditer(r'"\s*\{|\{', line):
        try:
            value, _ = decoder.raw_decode(line, match.start())
            if isinstance(value, str):
                value = json.loads(value)
        except (ValueError, TypeError):
            continue
        if isinstance(value, dict) and isinstance(value.get("event"), str):
            return value
    return None


def analyze_log(text):
    """Use the latest adapter entry so an earlier success cannot hide a later failure."""
    lines = text.splitlines()
    events = []
    latest_entry = None
    entry_line = 0
    for number, line in enumerate(lines, 1):
        event = json_event(line)
        if event is None:
            continue
        if event.get("event") == "adapter_phase" and event.get("phase") == "entry":
            latest_entry = event
            entry_line = number
            events = []
        events.append((number, event))

    phases = []
    failures = []
    bridge_evidence = []
    active_evidence = []
    completions = []
    for number, event in events:
        evidence = {**event, "line": number}
        if event["event"] == "adapter_phase":
            phases.append({"line": number, "phase": event.get("phase"), "mode": event.get("mode")})
            if isinstance(event.get("phase"), str) and event["phase"] in BRIDGE_PHASES:
                bridge_evidence.append(evidence)
        if event["event"] == "adapter_start":
            bridge_evidence.append(evidence)
        if event["event"] == "adapter_failed":
            failures.append(evidence)
        if any(event.get(flag) is True for flag in INACTIVE_FLAGS):
            active_evidence.append(evidence)
        if event["event"] == "adapter_diagnostic_complete":
            completions.append(evidence)

    valid_entry = latest_entry is not None and latest_entry.get("mode") == "diagnostic"
    for number, raw_line in enumerate(lines, 1):
        if number >= entry_line and re.search(r"Loading dynamic library\s+.*de_client_bridge\.dll", raw_line, re.IGNORECASE):
            bridge_evidence.append({"line": number, "event": "dynamic_library_load", "message": raw_line})
    valid_completions = []
    invalid_completions = []
    for event in completions:
        valid = (event["line"] > entry_line and valid_entry and event.get("mode") == "diagnostic"
                 and event.get("phase") == "diagnostic_complete"
                 and all(event.get(flag) is False for flag in INACTIVE_FLAGS))
        (valid_completions if valid else invalid_completions).append(event)
    if failures:
        status = "adapter_failed"
    elif valid_entry and (bridge_evidence or active_evidence):
        status = "unexpected_diagnostic_activity"
    elif invalid_completions:
        status = "invalid_diagnostic_completion"
    elif latest_entry is not None and not valid_entry:
        status = "different_mode"
    elif valid_entry and valid_completions:
        status = "diagnostic_complete"
    else:
        status = "incomplete"

    # Shutdown strings are evidence only. Script completion intentionally does
    # not require game exit, and neither string proves a stable render session.
    session_lines = lines[max(0, entry_line - 1):]
    return {
        "schema_version": 1,
        "status": status,
        "diagnostic_complete": status == "diagnostic_complete",
        "diagnostic_markers": {"entry": valid_entry, "completion": bool(completions),
                               "inactive_flags_valid": bool(valid_completions) and not invalid_completions},
        "latest_entry": latest_entry,
        "phases": phases,
        "failures": failures,
        "bridge_evidence": bridge_evidence,
        "active_evidence": active_evidence,
        "shutdown_evidence": {
            "host_shutdown": any("host shut down" in line.lower() for line in session_lines),
            "runtime_exit": any(re.search(r"\[INFO\]\s+Exit\s*$", line) for line in session_lines),
        },
        "notice": NOTICE,
    }


def read_snapshot(path):
    """Bound the actual read as well as stat size because CLEO can append meanwhile."""
    metadata = path.stat()
    if metadata.st_size > MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    with path.open("rb") as stream:
        data = stream.read(MAX_LOG_BYTES + 1)
    if len(data) > MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    return {"data": data, "size_bytes": len(data), "mtime_ns": metadata.st_mtime_ns,
            "sha256": hashlib.sha256(data).hexdigest()}


def fresh_content(baseline, current):
    """Exclude existing markers even when the log is truncated, rotated, or rewritten."""
    if current.startswith(baseline):
        # A partial line present before watching cannot provide a fresh entry.
        suffix = current[len(baseline):]
        if baseline and not baseline.endswith((b"\n", b"\r")):
            _, separator, suffix = suffix.partition(b"\n")
            if not separator:
                return b""
        return suffix
    # On replacement, subtract old complete lines by occurrence count. Merely
    # touching or editing other lines must not resurrect old success markers.
    previous_lines = Counter(baseline.splitlines())
    fresh_lines = []
    for line in current.splitlines():
        if previous_lines[line]:
            previous_lines[line] -= 1
        else:
            fresh_lines.append(line)
    return b"\n".join(fresh_lines)


def snapshot_report(path, snapshot, baseline=None):
    data = snapshot["data"] if baseline is None else fresh_content(baseline, snapshot["data"])
    result = analyze_log(data.decode("utf-8-sig", errors="replace"))
    result.update({"log": str(path), "evidence_scope": "entire_log" if baseline is None else "fresh_content",
                   "log_fingerprint": {key: snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")}})
    return result


def error_report(path, error):
    return {"schema_version": 1, "status": "read_error", "diagnostic_complete": False,
            "log": str(path), "error": str(error), "notice": NOTICE}


def emit_report(result):
    print(json.dumps(result, ensure_ascii=True), flush=True)


def watch_log(path, seconds, emit=emit_report, clock=time.monotonic, sleep=time.sleep):
    """A fresh baseline makes watching safe to start before a manual game launch."""
    try:
        baseline_snapshot = read_snapshot(path)
        baseline = baseline_snapshot["data"]
    except FileNotFoundError:
        baseline_snapshot = None
        baseline = b""
    except (OSError, ValueError) as error:
        emit(error_report(path, error))
        return 2
    baseline_fingerprint = None if baseline_snapshot is None else {
        key: baseline_snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")
    }
    emit({"schema_version": 1, "status": "watching", "diagnostic_complete": False, "log": str(path),
          "baseline_fingerprint": baseline_fingerprint, "watch_seconds": seconds, "notice": NOTICE})
    deadline = clock() + seconds
    last_state = None
    result = None
    while True:
        try:
            snapshot = read_snapshot(path)
            result = snapshot_report(path, snapshot, baseline)
        except (OSError, ValueError) as error:
            result = error_report(path, error)
        encoded = json.dumps(result, sort_keys=True)
        if encoded != last_state:
            emit(result)
            last_state = encoded
        if result["diagnostic_complete"]:
            return 0
        if result["status"] in {"adapter_failed", "unexpected_diagnostic_activity", "invalid_diagnostic_completion", "different_mode"}:
            return 2
        remaining = deadline - clock()
        if remaining <= 0:
            emit({"schema_version": 1, "status": "watch_timeout", "diagnostic_complete": False,
                  "log": str(path), "last_status": result["status"], "watch_seconds": seconds,
                  "notice": NOTICE})
            return 2
        sleep(min(1.0, remaining))


def bounded_seconds(value):
    try:
        seconds = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("watch seconds must be an integer from 1 through 600") from None
    if not 1 <= seconds <= 600:
        raise argparse.ArgumentTypeError("watch seconds must be from 1 through 600")
    return seconds


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, default=DEFAULT_LOG, help="CLEO log path (default: installed game's Win64 log)")
    parser.add_argument("--watch-seconds", type=bounded_seconds, help="Wait for fresh markers after taking a baseline; 1..600 seconds")
    args = parser.parse_args(argv)
    if args.watch_seconds is not None:
        return watch_log(args.log, args.watch_seconds)
    try:
        result = snapshot_report(args.log, read_snapshot(args.log))
    except (OSError, ValueError) as error:
        emit_report(error_report(args.log, error))
        return 2
    emit_report(result)
    return 0 if result["diagnostic_complete"] else 2


if __name__ == "__main__":
    sys.exit(main())
