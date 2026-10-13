"""Audit saved Core configured movement writes without operating the game."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

try:
    from .core_startup_log import integer, logged_event, safe_report
    from .session_log import MAX_LOG_BYTES, read_snapshot
except ImportError:
    from core_startup_log import integer, logged_event, safe_report
    from session_log import MAX_LOG_BYTES, read_snapshot

PROVENANCE = 'mta_core_configured_movement'
ACTIONS = dict(forwards=4, backwards=5, left=6, right=7, sprint=13, walk=17)
MOVEMENT_MASK = sum(1 << action for action in ACTIONS.values())
METRICS = ('bound', 'active', 'callbackCount', 'scopedSteps', 'routedQueries', 'neutralQueries', 'restoredSteps',
           'restoreFailures', 'foreignThreadCallbacks', 'nestedCallbacks', 'failureReason', 'lastMoveState')
REQUIRED = {'forwards', 'sprint', 'walk', 'neutral'}
PREFIX = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5]
NOTICE = ('This checks saved Core-owned configured frames and reported eligible local callback/query/restoration activity. '
          'Walk and sprint require forward input with the respective isolated modifier. Visual movement, complete engine input, '
          'remote ownership, rendering and full MTA boot remain unverified by this log.')


def signed(value):
    return value - 0x100000000 if value & 0x80000000 else value


def metrics(state):
    return {name: state[64 + index * 2] | (state[65 + index * 2] << 32) for index, name in enumerate(METRICS)}


def analyze_log(text, require_exit=False):
    """Validate this write lifecycle directly; never rewrite it as a read-only trial."""
    if len(text.encode('utf-8', errors='replace')) > MAX_LOG_BYTES:
        raise ValueError('CLEO log exceeds the 16 MiB read limit')
    issues, failures, samples, observed, counts = [], [], [], set(), Counter()
    entry = armed = requested = released = adapter = started = stop = finished = None
    shutdowns, exits = [], []
    last_clock = None

    def invalid(line, message):
        if len(issues) < 128:
            issues.append(dict(line=line, reason=message))

    def activity(event):
        return (event.get('input_writes') is True and event.get('network_active') is False and
                event.get('remote_peds_created') is False)

    def unsigned_array(values, count):
        return isinstance(values, list) and len(values) == count and all(integer(value, 0xffffffff) for value in values)

    for line, raw in enumerate(text.splitlines(), 1):
        if re.search(r'\[INFO\]\s+New event received:\s*"host shut down"\s*$', raw):
            shutdowns.append(line)
        if re.search(r'\[INFO\]\s+Exit\s*$', raw):
            exits.append(line)
        try:
            event = logged_event(raw)
        except (RecursionError, OverflowError):
            invalid(line, 'JSON exceeds the parser representation')
            continue
        if event is None:
            if 'core_movement_reference' in raw:
                invalid(line, 'Malformed Core movement marker')
            continue
        kind = event['event']; counts[kind] += 1
        if len(samples) > 4096:
            invalid(line, 'Bounded saved sample count exceeded')
            break
        if (kind.startswith(('core_input_reference', 'core_startup_reference', 'core_render_reference', 'native_','online_', 'remote_')) or
                kind in {'connecting', 'connected', 'joined', 'welcome', 'local_controller_sample', 'game_input_written'} or
                event.get('network_active') is True or event.get('remote_peds_created') is True or event.get('mode') == 'online'):
            invalid(line, 'Another mode or remote/network activity cannot accompany the Core movement trial')
        if event.get('input_writes') is True and kind not in {'adapter_start', 'core_movement_reference_finished'}:
            invalid(line, 'Write activity must identify this scoped Core movement lifecycle')
        if kind in {'adapter_failed', 'cleanup_failed'}:
            failures.append(dict(line=line, **event))
        if kind == 'adapter_phase' and event.get('phase') == 'entry':
            if entry or started or event.get('mode') != 'smoke':
                invalid(line, 'Exactly one Smoke entry must precede activation')
            entry = dict(line=line, **event)
        elif kind == 'adapter_armed':
            if not entry or armed or requested or any(event.get(flag) is not False for flag in ('bridge_loaded', 'game_adapter_active', 'network_active')) or event.get('mode') != 'smoke' or event.get('activate_key') != 'Ctrl+Shift+F9' or event.get('stop_key') != 'Ctrl+Shift+End':
                invalid(line, 'One inactive F9 arming marker must follow entry')
            armed = dict(line=line, **event)
        elif kind == 'adapter_activation_requested':
            if not armed or requested or released or event.get('mode') != 'smoke' or event.get('activate_key') != 'Ctrl+Shift+F9':
                invalid(line, 'Activation request must follow arming once')
            requested = dict(line=line, **event)
        elif kind == 'adapter_activation_released':
            if (not requested or released or started or event.get('mode') != 'smoke' or event.get('activate_key') != 'Ctrl+Shift+F9' or
                    not integer(event.get('at_ms')) or not integer(event.get('release_wait_ms'), 9999) or
                    any(event.get(flag) is not False for flag in ('bridge_loaded', 'game_adapter_active', 'network_active'))):
                invalid(line, 'All-key activation release must precede bridge access')
            released = dict(line=line, **event)
        elif kind == 'adapter_start':
            if (not released or adapter or started or not activity(event) or event.get('mode') != 'smoke' or
                    event.get('host') != 'sa_unreal' or event.get('endpoint', 1) is not None or event.get('stop_key') != 'Ctrl+Shift+End' or
                    event.get('core_movement_reference') is not True or event.get('provenance') != PROVENANCE or
                    type(event.get('core_api')) is not int or event['core_api'] != 4 or type(event.get('adapter_abi')) is not int or event['adapter_abi'] != 6 or
                    any(not isinstance(event.get(key), str) or not event[key] for key in ('cleo', 'host_version', 'definitions'))):
                invalid(line, 'Adapter start must identify one isolated Core API4 and adapter ABI6 write probe')
            adapter = dict(line=line, **event)
        elif kind == 'core_movement_reference_started':
            required = {'event', 'at_ms', 'core_api', 'adapter_abi', 'sample_interval_ms', 'budget_ms', 'readiness_budget_ms', 'cleanup_deadline_ms', 'context_source'}
            constants = dict(core_api=4, adapter_abi=6, sample_interval_ms=100, budget_ms=30000, readiness_budget_ms=10000, cleanup_deadline_ms=2000)
            if (set(event) != required or not adapter or started or finished or not integer(event.get('at_ms')) or
                    not released or not integer(released.get('at_ms')) or event['at_ms'] < released['at_ms'] or
                    any(type(event.get(key)) is not int or event[key] != value for key, value in constants.items()) or
                    event.get('context_source') != 'engine_eligibility'):
                invalid(line, 'Start must declare the bounded verified-eligibility write contract')
            started = dict(line=line, **event)
        elif kind == 'core_movement_reference_stop_requested':
            if (set(event) != {'event', 'at_ms', 'reason'} or not started or stop or finished or not integer(event.get('at_ms')) or
                    event.get('reason') not in ('loading', 'Ctrl+Shift+End')):
                invalid(line, 'Cancellation must be one typed request during the active lifecycle')
            stop = dict(line=line, **event)
        elif kind == 'core_movement_reference_sample':
            state = event.get('state')
            if (set(event) != {'event', 'at_ms', 'elapsed_ms', 'state'} or not started or finished or
                    not integer(event.get('at_ms')) or not integer(event.get('elapsed_ms')) or
                    not integer(started.get('at_ms')) or event['at_ms'] - started['at_ms'] != event['elapsed_ms'] or not unsigned_array(state, 96)):
                invalid(line, 'Sample must contain exactly96 unsigned fields with bound script clocks')
                continue
            if last_clock is not None and event['at_ms'] < last_clock:
                invalid(line, 'Script sample clock reversed')
            last_clock = event['at_ms']
            previous = samples[-1]['state'] if samples else None
            if (state[0] != 1 or state[1] not in (1, 2, 3) or state[2] < 1 or state[3] > 31000 or state[4] not in (0, 1, 3) or
                    state[5] > 10000 or state[6] > 10000 or state[7] > state[5] or state[8] > state[6] or
                    state[9] > 600 or state[10] > 600 or state[11] > 600 or state[12] & ~MOVEMENT_MASK or state[13] or
                    state[14] > 13 or state[15] > 1 or state[88] & ~15 or state[89] & ~15 or state[90] > 1 or state[91] > 1 or
                    state[93] > 1 or state[94] > 1 or state[95] > 1):
                invalid(line, 'Snapshot version, movement scope, eligibility or clock bounds are invalid')
            if previous and (state[2] <= previous[2] or state[3] < previous[3] or state[11] < previous[11] or
                             state[9] < previous[9] or state[10] < previous[10] or previous[1] == 2 or
                             state[91] < previous[91] or state[92] < previous[92] or state[93] < previous[93] or state[94] < previous[94] or
                             (previous[1] == 3 and state[1] == 1)):
                invalid(line, 'Native publication sequence or monotone counters regressed')
            current = [signed(value) for value in state[16:40]]
            old = [signed(value) for value in state[40:64]]
            for controller in (current, old):
                if (any(value not in (-128, 0, 128) for value in controller[:2]) or
                        any(controller[index] for index in range(24) if index not in (0, 1, 16, 21)) or
                        controller[16] not in (0, 255) or controller[21] not in (0, 255)):
                    invalid(line, 'Canonical frame exceeds the supported movement-only values')
            native = metrics(state)
            if (native['bound'] > 1 or native['active'] > 1 or native['failureReason'] > 18 or native['lastMoveState'] > 0xffffffff or
                    native['scopedSteps'] > native['callbackCount'] or native['restoredSteps'] > native['scopedSteps'] or native['neutralQueries'] > native['routedQueries']):
                invalid(line, 'Native callback/query/restoration relations are impossible')
            if previous:
                old_native = metrics(previous)
                if any(native[name] < old_native[name] for name in METRICS[2:10]):
                    invalid(line, 'Native64-bit counters regressed')
            if state[1] == 1:
                if (not 1 <= state[11] <= 600 or state[2] != state[11] or state[3] >= 30000 or state[4] != 3 or
                        not state[5] or not state[7] or state[14] or state[15] or state[88:92] != [15, 15, 1, 1] or
                        state[92] or state[93] or state[94:96] != [1, 0] or native['bound'] != 1 or native['active'] != 1):
                    invalid(line, 'Regular writes require focused neutral-armed binding and complete engine eligibility')
                if previous and previous[1] == 1 and state[2] == previous[2] + 1 and state[40:64] != previous[16:40]:
                    invalid(line, 'Consecutive previous/current canonical frames disagree')
                held = state[12]
                for positive, negative, index in (('right', 'left', 0), ('backwards', 'forwards', 1)):
                    p, n = bool(held & (1 << ACTIONS[positive])), bool(held & (1 << ACTIONS[negative]))
                    expected = 128 if p and not n else -128 if n and not p else 0 if not p and not n else None
                    if expected is not None and current[index] != expected:
                        invalid(line, 'Configured direction mask disagrees with the canonical frame')
                    if p and not n and current[index] == 128: observed.add(positive)
                    if n and not p and current[index] == -128: observed.add(negative)
                for name, index in (('sprint', 16), ('walk', 21)):
                    if current[index] != (255 if held & (1 << ACTIONS[name]) else 0):
                        invalid(line, 'Configured modifier mask disagrees with its canonical field')
                if current[1] == -128 and current[16] == 255 and current[21] == 0: observed.add('sprint')
                if current[1] == -128 and current[21] == 255 and current[16] == 0: observed.add('walk')
                if not held and not any(current): observed.add('neutral')
            elif state[1] == 3:
                if state[95] != 1 or state[15] or not state[92]:
                    invalid(line, 'Pending cleanup must retain ownership without claiming completion')
            elif state[95] or not state[92]:
                invalid(line, 'Terminal publication must follow actual cleanup attempts')
            samples.append(dict(line=line, **event))
        elif kind == 'core_movement_reference_finished':
            keys = {'event', 'at_ms', 'elapsed_ms', 'state', 'run_result', 'passed', 'cleanup_passed', 'stop_requested', 'samples',
                    'full_core_ready', 'rendering_started', 'input_writes', 'network_active', 'remote_peds_created'}
            state = event.get('state')
            if (set(event) != keys or not started or finished or (samples and samples[-1]['state'][1] != 2) or
                    not integer(event.get('at_ms')) or not integer(event.get('elapsed_ms')) or not integer(started.get('at_ms')) or event['at_ms'] - started['at_ms'] != event['elapsed_ms'] or
                    (samples and event['at_ms'] < samples[-1]['at_ms']) or not unsigned_array(state, 19) or
                    type(event.get('samples')) is not int or event['samples'] != len(samples) or not activity(event) or
                    event.get('full_core_ready') is not False or event.get('rendering_started') is not False or
                    any(type(event.get(key)) is not bool for key in ('passed', 'cleanup_passed', 'stop_requested'))):
                invalid(line, 'One finished result must bind the terminal96 publication,19 diagnostics and narrow write scope')
            if unsigned_array(state, 19):
                pinned = samples[-1]['state'][94] if samples else 0
                cleanup = state[7] == 1 and state[13] == 1 and state[14] == 0 and state[15] == pinned
                if event.get('cleanup_passed') is not cleanup:
                    invalid(line, 'Cleanup result must preserve the actual Game DE pin instead of unloading its code')
                run = int(state[17] == 1 and state[16] == 0)
                if type(event.get('run_result')) is not int or event['run_result'] != run:
                    invalid(line, 'Run result must derive from immutable worker diagnostics')
            if event.get('stop_requested') is not bool(stop):
                invalid(line, 'Terminal cancellation flag requires its actual request marker')
            finished = dict(line=line, **event)
        elif kind.startswith('core_movement_reference_'):
            invalid(line, 'Unknown Core movement marker')

    normal_exit = bool(len(shutdowns) == len(exits) == 1 and finished and finished['line'] < shutdowns[0] < exits[0])
    if (shutdowns or exits) and not normal_exit:
        invalid((shutdowns + exits)[0], 'Normal shutdown and Exit must occur once after terminal cleanup')
    terminal = samples[-1]['state'] if samples else None
    native = metrics(terminal) if terminal else None
    native_clean = bool(native and not native['bound'] and not native['active'] and native['scopedSteps'] > 0 and
                        native['routedQueries'] > 0 and native['restoredSteps'] == native['scopedSteps'] and native['lastMoveState'] != 0xffffffff and
                        not any(native[name] for name in ('restoreFailures', 'foreignThreadCallbacks', 'nestedCallbacks', 'failureReason')))
    input_clean = bool(terminal and terminal[1] == 2 and terminal[4] == 0 and terminal[88:91] == [15, 15, 1] and
                       30000 <= terminal[3] <= 31000 and terminal[14:16] == [1, 1] and
                       terminal[91] == 1 and terminal[92] >= 1 and terminal[93:96] == [0, 1, 0] and
                       not any(terminal[12:14]) and not any(terminal[16:64]))
    final_state = finished.get('state') if finished else None
    worker_clean = bool(unsigned_array(final_state, 19) and final_state[:11] == PREFIX and
                        final_state[11] <= 3 and final_state[12] <= 3 and not final_state[12] & ~final_state[11] and
                        final_state[13:] == [1, 0, 1, 0, 1, 0])
    lifecycle = bool(entry and armed and requested and released and adapter and started and finished and
                     counts['adapter_start'] == counts['core_movement_reference_started'] == counts['core_movement_reference_finished'] == 1 and
                     not stop and not issues and not failures and native_clean and input_clean and worker_clean and
                     finished.get('passed') is True and finished.get('cleanup_passed') is True and finished.get('run_result') == 1 and
                     (not require_exit or normal_exit))
    if finished and finished.get('passed') is True and not (native_clean and input_clean and worker_clean and not stop):
        invalid(finished['line'], 'A passed flag cannot override eligibility, native restoration or worker cleanup failure')
        lifecycle = False
    complete = lifecycle and REQUIRED <= observed
    return dict(schema_version=1, status='core_movement_reference_observed' if complete else
                'movement_coverage_missing' if lifecycle else 'trial_failed' if failures or finished else 'incomplete',
                core_movement_reference_complete=complete, configured_movement_session_complete=lifecycle,
                configured_movement_coverage_verified=complete, scoped_local_engine_input_write_observed=lifecycle,
                observed_controls=sorted(observed), missing_controls=sorted(REQUIRED - observed), sample_count=len(samples),
                native_sample_count=terminal[11] if terminal else None, native_metrics=native, terminal_movement_state=terminal,
                worker_state=final_state, cleanup_pending=bool(terminal and terminal[1] == 3 and terminal[95] == 1),
                normal_exit_verified=normal_exit, require_exit=require_exit, issue_count=len(issues), issues=issues,
                failures=failures, event_counts=dict(counts), samples=samples,
                visual_success_verified=False, full_mta_boot_verified=False, de_renderer_verified=False,
                unqualified_engine_input_write_verified=False, remote_player_context_verified=False,
                context_source='engine_eligibility', notice=NOTICE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--require-exit', action='store_true')
    args = parser.parse_args(argv)
    try:
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError('Output must differ from the raw evidence log')
        raw = read_snapshot(args.log)
        result = analyze_log(raw['data'].decode('utf-8-sig', errors='replace'), args.require_exit)
        result.update(log=str(args.log.resolve()), evidence_scope='entire_log', source_line_origin=str(args.log.resolve()),
                      log_fingerprint={key: raw[key] for key in ('size_bytes', 'mtime_ns', 'sha256')},
                      reader_path=str(Path(__file__).resolve()), reader_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    except (OSError, ValueError) as error:
        result = dict(status='read_error', error=str(error), core_movement_reference_complete=False)
        if 'Output must differ' in str(error):
            print(json.dumps(result)); return 2
    args.output.write_text(json.dumps(safe_report(result), indent=2, allow_nan=False) + '\n', encoding='utf-8')
    print(json.dumps(safe_report(result), allow_nan=False))
    return 0 if result['core_movement_reference_complete'] else 2


if __name__ == '__main__':
    sys.exit(main())
