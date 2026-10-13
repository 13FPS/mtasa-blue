"""Read remote-ped lifecycle markers without launching or inspecting GTA."""

import argparse
import json
import math
from pathlib import Path
import re
import sys

try:
    from .session_log import DEFAULT_LOG, fresh_content, json_event, read_snapshot
except ImportError:
    from session_log import DEFAULT_LOG, fresh_content, json_event, read_snapshot


NOTICE = (
    "Only ordered game-adapter log markers are verified. A deletion marker can also mean "
    "an owned ped was already absent. These markers do not prove visible rendering, "
    "successful engine effects, two real clients, multiplayer, or runtime stability."
)
COMPONENTS = ("x", "y", "z", "heading")


def positive_id(value):
    return type(value) is int and 1 <= value <= 0xFFFFFFFF


def transform(event):
    values = tuple(event.get(name) for name in COMPONENTS)
    # Bound integers before float conversion: an arbitrary JSON integer must
    # be rejected as evidence rather than overflowing math.isfinite.
    return values if all(type(v) in (int, float) and abs(v) <= 100000 and math.isfinite(v) for v in values) else None


def analyze_log(text):
    lines = text.splitlines()
    entries = [(i, json_event(line)) for i, line in enumerate(lines)]
    starts = [(i, event) for i, event in entries if event and event.get("event") == "adapter_phase"
              and event.get("phase") == "entry"]
    start, entry = starts[-1] if starts else (len(lines), None)
    active, completed, issues, failures = {}, [], [], []
    own_id = None
    join_line = stop_line = shutdown_line = exit_line = None
    joined = False

    def invalid(number, reason):
        issues.append({"line": number, "reason": reason})

    # Each creation owns its own evidence. Reconnection or a reused player ID
    # must never combine one ped's first transform with another ped's movement.
    for index, event in entries[start:]:
        number, raw = index + 1, lines[index]
        if re.search(r'\[INFO\]\s+New event received:\s*"host shut down"\s*$', raw):
            if shutdown_line or not stop_line or active:
                invalid(number, "host shutdown was duplicated or preceded cleanup/stop")
            shutdown_line = number
        if re.search(r"\[INFO\]\s+Exit\s*$", raw):
            if exit_line or not shutdown_line or not stop_line:
                invalid(number, "Exit was duplicated or preceded host shutdown/stop")
            exit_line = number
        if event is None:
            continue
        kind = event["event"]
        if kind in ("adapter_failed", "cleanup_failed"):
            failures.append({**event, "line": number})
        if kind not in {"connecting", "transport_lost", "joined", "remote_ped_created",
                        "remote_transform_applied", "remote_ped_deleted", "adapter_stopped"}:
            continue
        if stop_line or shutdown_line or exit_line:
            invalid(number, "lifecycle event followed adapter stop or host shutdown")
        player_id, ped = event.get("player_id"), event.get("ped")
        if kind in ("connecting", "transport_lost"):
            if active:
                invalid(number, "connection changed before owned peds were cleaned up")
            own_id = None
        elif kind == "joined":
            if not positive_id(player_id) or own_id is not None or active:
                invalid(number, "invalid or duplicated join")
            else:
                own_id, join_line, joined = player_id, number, True
        elif kind == "remote_ped_created":
            if (own_id is None or not positive_id(player_id) or player_id == own_id
                    or not positive_id(ped) or player_id in active
                    or any(value["ped"] == ped for value in active.values())):
                invalid(number, "invalid remote creation identity or missing join")
            else:
                active[player_id] = {"player_id": player_id, "ped": ped, "join_line": join_line,
                                     "created_line": number, "transform_count": 0, "changed": False}
        elif kind == "remote_transform_applied":
            record, values = active.get(player_id) if positive_id(player_id) else None, transform(event)
            if record is None or not positive_id(ped) or record["ped"] != ped or values is None:
                invalid(number, "invalid transform or unmatched remote identity")
            else:
                record["transform_count"] += 1
                if "first_transform" not in record:
                    record["first_transform"] = values
                record["changed"] |= values != record["first_transform"]
                record["last_transform"], record["last_applied_line"] = values, number
        elif kind == "remote_ped_deleted":
            record = active.get(player_id) if positive_id(player_id) else None
            if record is None or ("ped" in event and (not positive_id(ped) or ped != record["ped"])):
                invalid(number, "deletion did not match a live owned ped")
            else:
                record["deleted_line"] = number
                completed.append(active.pop(player_id))
        elif kind == "adapter_stopped":
            if event.get("reason") != "Ctrl+Shift+End" or active:
                invalid(number, "stop shortcut was wrong or preceded remote cleanup")
            stop_line = number

    created = bool(completed or active)
    moved = created and all(record["transform_count"] >= 2 and record["changed"]
                           for record in completed + list(active.values()))
    flags = {"online_entry": entry is not None and entry.get("mode") == "online", "joined": joined,
             "remote_ped_created": created, "changed_transforms_applied": moved,
             "remote_ped_deleted": bool(completed) and not active, "adapter_stopped": stop_line is not None,
             "host_shutdown": shutdown_line is not None, "runtime_exit": exit_line is not None,
             "valid_event_order": not issues, "no_failures": not failures}
    passed = all(flags.values())
    status = ("remote_command_sequence_passed" if passed else "adapter_failed" if failures
              else "invalid_evidence" if issues else "different_mode" if entry and not flags["online_entry"]
              else "incomplete")
    return {"schema_version": 1, "status": status, "remote_command_sequence_passed": passed,
            "rendering_verified": False, "multiplayer_verified": False, "flags": flags,
            "latest_entry": entry, "completed_lifecycle_count": len(completed),
            "completed_lifecycles": completed[:32], "active_lifecycle_count": len(active),
            "issue_count": len(issues), "issues": issues[:32], "failures": failures[:32], "notice": NOTICE}


def snapshot_report(path, snapshot, baseline=None):
    # Subtract existing content after truncation/replacement as well as append;
    # old success markers cannot provide acceptance for a fresh manual run.
    data = snapshot["data"] if baseline is None else fresh_content(baseline, snapshot["data"])
    result = analyze_log(data.decode("utf-8-sig", errors="replace"))
    result.update({"log": str(path), "evidence_scope": "entire_log" if baseline is None else "fresh_content",
                   "log_fingerprint": {key: snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")}})
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, default=DEFAULT_LOG)
    parser.add_argument("--baseline", type=Path, help="Saved pre-run CLEO log; exclude its existing markers")
    args = parser.parse_args(argv)
    try:
        baseline = None if args.baseline is None else read_snapshot(args.baseline)["data"]
        result = snapshot_report(args.log, read_snapshot(args.log), baseline)
    except (OSError, ValueError) as error:
        result = {"schema_version": 1, "status": "read_error", "error": str(error), "log": str(args.log),
                  "remote_command_sequence_passed": False, "rendering_verified": False,
                  "multiplayer_verified": False, "notice": NOTICE}
    print(json.dumps(result, ensure_ascii=True), flush=True)
    return 0 if result["remote_command_sequence_passed"] else 2


if __name__ == "__main__":
    sys.exit(main())
