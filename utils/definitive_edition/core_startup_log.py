"""Audit saved read-only MTA Core startup evidence without operating GTA."""

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import sys

try:
    from .session_log import MAX_LOG_BYTES, read_snapshot
except ImportError:
    from session_log import MAX_LOG_BYTES, read_snapshot

PROVENANCE = "mta_core_de_startup"
MAX_EVENTS = 128
FLAGS = {"network_active", "input_writes", "remote_peds_created"}
FIELD_NAMES = (
    "version", "initialize_result", "phase", "capabilities_low", "capabilities_high",
    "gameplay_ready", "full_core_ready", "adapter_cleanup_complete", "first_initialize_result",
    "second_initialize_result", "last_initialize_result", "legacy_renderer_before", "legacy_renderer_after",
    "core_unloaded", "backend_before", "backend_after", "error_reason", "completed", "win32_load_error",
)
EXPECTED_PREFIX = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5]
NOTICE = (
    "This verifies saved reports of the real Core's exact-image binding, pulse, expected incomplete-gameplay "
    "rejection and cleanup. It does not establish full MTA startup, rendering, input writes, remote ownership "
    "or the absence of unlogged game operations."
)


def integer(value, maximum=9007199254740991):
    return type(value) is int and 0 <= value <= maximum


def logged_event(raw):
    # Keep CLEO's quoted-message compatibility, but refuse duplicate fields and
    # nonstandard numeric constants rather than silently replacing evidence.
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("Duplicate JSON field")
            result[key] = value
        return result

    def constant(_):
        raise ValueError("Nonstandard JSON constant")

    decoder = json.JSONDecoder(object_pairs_hook=pairs, parse_constant=constant)
    for match in re.finditer(r'"\s*\{|\{', raw):
        try:
            value, _ = decoder.raw_decode(raw, match.start())
            if isinstance(value, str):
                value = json.loads(value, object_pairs_hook=pairs, parse_constant=constant)
        except (ValueError, TypeError):
            continue
        if isinstance(value, dict) and isinstance(value.get("event"), str):
            return value
    return None


def analyze_log(text, require_exit=False):
    """Validate the entire stream; an earlier success cannot hide a restart."""
    if len(text.encode("utf-8", errors="replace")) > MAX_LOG_BYTES:
        raise ValueError("CLEO log exceeds the 16 MiB read limit")
    events, issues, failures, shutdowns, exits = [], [], [], [], []
    counts = Counter()
    entry = adapter = armed = requested = released = started = finished = stopped = None
    expected_result = False
    issue_count = 0

    def invalid(event, reason):
        nonlocal issue_count
        issue_count += 1
        if len(issues) < 64:
            issues.append({"line": event.get("line"), "event": event.get("event"), "reason": reason})

    for line, raw in enumerate(text.splitlines(), 1):
        if re.search(r'\[INFO\]\s+New event received:\s*"host shut down"\s*$', raw):
            shutdowns.append(line)
        if re.search(r"\[INFO\]\s+Exit\s*$", raw):
            exits.append(line)
        try:
            payload = logged_event(raw)
        except (RecursionError, OverflowError):
            invalid({"line": line}, "JSON exceeds the parser representation")
            continue
        if payload is None:
            if any(marker in raw for marker in ("core_startup_reference", "adapter_failed", "cleanup_failed")):
                invalid({"line": line}, "Malformed startup/failure marker cannot be skipped")
            continue
        event = {**payload, "line": line}
        if len(events) >= MAX_EVENTS:
            invalid(event, "Bounded event count exceeded")
            continue
        events.append(event)
        kind = event["event"]
        counts[kind] += 1
        if any(flag in event and event[flag] is not False for flag in FLAGS) or event.get("mode") == "online":
            invalid(event, "Input writes, networking or remote activity is outside this read-only reference")
        if (kind.startswith(("native_", "remote_", "online_", "core_input_reference", "core_movement_reference", "core_render_reference")) or
                kind in {"connecting", "connected", "joined", "welcome", "input_sent", "game_input_written", "local_controller_sample"} or
                any(key in event for key in ("ped", "position", "heading", "raw_velocity", "localPedHandle"))):
            invalid(event, "Movement, player observations or unrelated native references are outside this startup probe")
        if kind in {"adapter_failed", "cleanup_failed", "adapter_activation_cancelled", "adapter_activation_release_timeout"}:
            if len(failures) < 16:
                failures.append(event)
        if kind == "adapter_phase" and event.get("phase") == "entry":
            if entry is not None or event.get("mode") != "smoke" or started is not None:
                invalid(event, "Exactly one Smoke entry must precede startup")
            entry = event
        if kind in {"adapter_armed", "adapter_activation_requested", "adapter_activation_released"}:
            if event.get("mode") != "smoke" or event.get("activate_key") != "Ctrl+Shift+F9":
                invalid(event, "Startup requires explicit F9 activation")
            if kind == "adapter_armed":
                if armed is not None or not entry or adapter:
                    invalid(event, "Arming must occur once before DLL startup")
                if event.get("stop_key") != "Ctrl+Shift+End" or any(event.get(flag) is not False for flag in ("bridge_loaded", "game_adapter_active", "network_active")):
                    invalid(event, "Arming must preserve an inactive runtime")
                armed = event
            elif kind == "adapter_activation_requested":
                if requested is not None or not armed or released or adapter:
                    invalid(event, "Activation request must follow arming exactly once")
                requested = event
            else:
                if released is not None or not requested or adapter or not integer(event.get("at_ms")) or not integer(event.get("release_wait_ms"), 9999):
                    invalid(event, "Activation release clock/order is invalid")
                if any(event.get(flag) is not False for flag in ("bridge_loaded", "game_adapter_active", "network_active")):
                    invalid(event, "DLL activity must wait for activation release")
                released = event
        if kind == "adapter_start":
            if (adapter is not None or not entry or not released or started or event.get("mode") != "smoke" or
                    event.get("core_startup_reference") is not True or event.get("provenance") != PROVENANCE or
                    type(event.get("core_api")) is not int or event["core_api"] not in (1, 2) or event.get("endpoint", 1) is not None or
                    any(event.get(flag) is not False for flag in FLAGS)):
                invalid(event, "Adapter start must identify one isolated read-only Core reference")
            if any(event.get(flag) is True for flag in ("native_pad_reference", "native_movement_trial", "native_input_reference", "gait_reference")):
                invalid(event, "Other experiments cannot accompany Core startup")
            adapter = event
        if kind == "core_startup_reference_started":
            if (set(payload) != {"event", "at_ms", "core_api"} or started is not None or finished or not adapter or
                    type(event.get("core_api")) is not int or not adapter or event["core_api"] != adapter.get("core_api") or
                    not integer(event.get("at_ms")) or
                    (released and integer(released.get("at_ms")) and event.get("at_ms", -1) < released["at_ms"])):
                invalid(event, "Core start must bind one safe clock after activation release")
            started = event
        elif kind == "core_startup_reference_stop_requested":
            if (set(payload) != {"event", "at_ms", "reason"} or stopped or finished or not started or
                    started.get("core_api") != 2 or not integer(event.get("at_ms")) or
                    not integer(started.get("at_ms")) or event.get("at_ms", -1) < started["at_ms"] or
                    event.get("reason") not in ("Ctrl+Shift+End", "loading")):
                invalid(event, "One cooperative stop request must follow worker start and precede its terminal result")
            stopped = event
        elif kind == "core_startup_reference_finished":
            expected_keys = {"event", "at_ms", "elapsed_ms", "run_result", "state", "passed", "cleanup_passed", "full_core_ready", "rendering_started"} | FLAGS
            api = started.get("core_api") if started else None
            if api == 2:
                expected_keys.add("stop_requested")
            if set(payload) != expected_keys or finished is not None or not started:
                invalid(event, "One complete terminal payload must follow startup")
            if (not integer(event.get("at_ms")) or not integer(event.get("elapsed_ms")) or not started or
                    not integer(started.get("at_ms")) or event.get("at_ms", -1) - started.get("at_ms", 0) != event.get("elapsed_ms")):
                invalid(event, "Terminal elapsed clock must match the nonnegative startup duration")
            if api == 2 and (type(event.get("stop_requested")) is not bool or event["stop_requested"] != bool(stopped)):
                invalid(event, "Terminal stop flag must match the single logged cooperative request")
            if stopped and (not integer(stopped.get("at_ms")) or not integer(event.get("at_ms")) or event["at_ms"] < stopped["at_ms"]):
                invalid(event, "Terminal cleanup must finish after the cooperative request")
            state = event.get("state")
            valid_state = isinstance(state, list) and len(state) == (19 if api == 2 else 18) and all(integer(value, 0xffffffff) for value in state)
            if not valid_state:
                invalid(event, "Expected the protocol's exact unsigned integer startup field count")
            else:
                expected_state = (state[:11] == EXPECTED_PREFIX and state[11] <= 3 and state[12] <= 3 and
                                  not state[12] & ~state[11] and state[13:18] == [1, 0, 0, 0, 1] and
                                  (api != 2 or state[18] == 0))
                # Preserve the old synchronous failure and its native load
                # diagnostics, but certify success only from the yielding API.
                expected_result = bool(api == 2 and expected_state and not stopped and event.get("passed") is True and
                                       event.get("run_result") == 1 and event.get("cleanup_passed") is True)
                if event.get("passed") is True and (api != 2 or not expected_state or stopped):
                    invalid(event, "Only uncancelled asynchronous startup with exact gates and cleanup can pass")
                if api == 2 and event.get("run_result") != (1 if state[17] == 1 and state[16] == 0 else 0):
                    invalid(event, "Terminal run result must derive from the completed worker snapshot")
                cleanup = state[7] == 1 and state[13] == 1 and state[14] == 0 and state[15] == 0
                if event.get("cleanup_passed") is not cleanup:
                    invalid(event, "Terminal cleanup flag disagrees with module and adapter diagnostics")
            if (type(event.get("run_result")) is not int or event["run_result"] not in (0, 1) or
                    type(event.get("passed")) is not bool or type(event.get("cleanup_passed")) is not bool or
                    event.get("full_core_ready") is not False or event.get("rendering_started") is not False or
                    any(event.get(flag) is not False for flag in FLAGS)):
                invalid(event, "Terminal result must use typed read-only flags while full Core and rendering remain unavailable")
            finished = event
        elif kind.startswith("core_startup_reference_"):
            invalid(event, "Unknown Core startup marker")

    normal_exit = bool(len(shutdowns) == len(exits) == 1 and finished and finished["line"] < shutdowns[0] < exits[0])
    if (shutdowns or exits) and not normal_exit:
        invalid({"line": (shutdowns + exits)[0]}, "Shutdown and Exit must occur once in order after terminal cleanup")
    flags = {
        "one_smoke_entry": bool(entry and counts["adapter_phase"] and sum(event["event"] == "adapter_phase" and event.get("phase") == "entry" for event in events) == 1),
        "explicit_activation": bool(armed and requested and released),
        "one_adapter_start": counts["adapter_start"] == 1,
        "one_reference_started": counts["core_startup_reference_started"] == 1,
        "one_reference_finished": counts["core_startup_reference_finished"] == 1,
        "expected_async_result": expected_result,
        "valid_payloads": issue_count == 0,
        "no_failures": not failures,
        "required_exit_verified": not require_exit or normal_exit,
    }
    complete = all(flags.values())
    state = finished.get("state") if finished else None
    named_state = dict(zip(FIELD_NAMES, state)) if isinstance(state, list) and len(state) in (18, 19) else None
    return {
        "schema_version": 1,
        "status": "core_startup_reference_observed" if complete else "adapter_failed" if failures else "invalid_evidence" if issue_count else "incomplete",
        "core_startup_reference_complete": complete, "read_only_core_startup_logged": complete,
        "full_mta_boot_verified": False, "de_renderer_verified": False, "engine_input_write_verified": False,
        "remote_player_context_verified": False, "normal_exit_verified": normal_exit, "require_exit": require_exit,
        "flags": flags, "state": named_state, "event_counts": dict(counts), "events": events,
        "issue_count": issue_count, "issues": issues, "failures": failures,
        "shutdown_evidence": {"host_shutdown_lines": shutdowns, "exit_lines": exits}, "notice": NOTICE,
    }


def safe_report(value, depth=0):
    # Invalid payloads remain visible without producing invalid JSON or letting
    # a deeply nested extra field overwhelm the saved assessment serializer.
    if depth > 64:
        return {"invalid_payload_depth_truncated": True}
    if isinstance(value, float) and not math.isfinite(value):
        return repr(value)
    if isinstance(value, dict):
        return {key: safe_report(item, depth + 1) for key, item in value.items()}
    if isinstance(value, list):
        return [safe_report(item, depth + 1) for item in value]
    return value


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--require-exit", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError("Output must differ from the raw evidence log")
        snapshot = read_snapshot(args.log)
        result = analyze_log(snapshot["data"].decode("utf-8-sig", errors="replace"), args.require_exit)
        result.update(log=str(args.log.resolve()), evidence_scope="entire_log", source_line_origin=str(args.log.resolve()),
                      log_fingerprint={key: snapshot[key] for key in ("size_bytes", "mtime_ns", "sha256")},
                      reader_path=str(Path(__file__).resolve()), reader_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    except (OSError, ValueError) as error:
        result = {"schema_version": 1, "status": "read_error", "error": str(error), "core_startup_reference_complete": False,
                  "read_only_core_startup_logged": False, "full_mta_boot_verified": False, "de_renderer_verified": False,
                  "engine_input_write_verified": False, "remote_player_context_verified": False, "notice": NOTICE}
        if "Output must differ" in str(error):
            print(json.dumps(result))
            return 2
    try:
        result = safe_report(result)
        args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    except (OSError, ValueError) as error:
        print(json.dumps({"status": "write_error", "error": str(error)}))
        return 2
    print(json.dumps(result, allow_nan=False))
    return 0 if result["core_startup_reference_complete"] else 2


if __name__ == "__main__":
    sys.exit(main())
