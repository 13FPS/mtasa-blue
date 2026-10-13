"""Audit saved MTA local movement trials without inspecting or controlling GTA."""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import sys
try:
    from .session_log import MAX_LOG_BYTES, json_event, read_snapshot
except ImportError:
    from session_log import MAX_LOG_BYTES, json_event, read_snapshot

PROVENANCE = 'mta_game_de_scoped_local_movement'
PHASES = ((3000, 'neutral', 0), (7000, 'mta_walk', 1), (9000, 'pause', 0), (13000, 'mta_forwards', 2),
          (15000, 'pause', 0), (19000, 'mta_sprint', 3), (22000, 'final_neutral', 0))
METRICS = {'bound', 'active', 'callbackCount', 'scopedSteps', 'routedQueries', 'neutralQueries', 'restoredSteps',
           'restoreFailures', 'foreignThreadCallbacks', 'nestedCallbacks', 'failureReason', 'lastMoveState'}
COUNTERS = METRICS - {'bound', 'active', 'failureReason', 'lastMoveState'}
COMMON = {'event', 'reference_id', 'ped', 'at_ms', 'elapsed_ms', 'provenance', 'adapter_abi', 'scope',
          'input_writes', 'network_active', 'remote_peds_created', 'velocity_units'}
MAX_EVENTS, MAX_SAMPLES = 512, 176
# The producer checks every tick's 2000ms gap, but a tick up to 124ms after
# publication can refresh input without emitting a sample. This capture bound
# follows that actual scheduling contract; it does not relax native freshness.
MAX_CAPTURE_GAP = 2000 + 125 - 1
# Native End can defer detachment until an in-flight scope restores its pad.
# Keep that shutdown delay separate from the action schedule so clean teardown
# cannot disguise a stale action frame or an unbounded trial.
MAX_CLEANUP_WAIT = 2000
NOTICE = ('This verifies the saved local trial schedule and reported native callback/query/restoration counters. '
          'Raw motion and native state values do not establish visual success, no sliding, velocity units, '
          'remote ownership, all controller actions or a complete MTA backend.')


def clock(value):
    return type(value) is int and 0 <= value <= 9007199254740991


def finite(value):
    return type(value) in (int, float) and abs(value) <= 100000 and math.isfinite(value)


def vector(value):
    return isinstance(value, dict) and set(value) == {'x', 'y', 'z'} and all(finite(item) for item in value.values())


def phase_index(elapsed):
    return next((index for index, phase in enumerate(PHASES) if elapsed < phase[0]), None)


def analyze_log(text, require_exit=False):
    """Keep the entire bounded stream; never hide a later restart or failure."""
    if len(text.encode('utf-8', errors='replace')) > MAX_LOG_BYTES:
        raise ValueError('CLEO log exceeds the 16 MiB read limit')
    events, issues, failures, accepted, rejected = [], [], [], [], []
    counts = Counter()
    entries, shutdowns, exits, seen_phases = [], [], [], []
    started = finished = identity = origin = last_at = last_sample = last_state = None
    adapter_abi = None
    current_phase = -1
    overflow = False
    issue_total = 0

    def invalid(event, reason):
        nonlocal issue_total
        issue_total += 1
        if len(issues) < 128:
            issues.append({'line': event.get('line'), 'event': event.get('event'), 'reason': reason})

    def exact(event, extra, optional=()):
        keys = set(event) - {'line'}
        required = COMMON | set(extra)
        if not required <= keys or keys - required - set(optional):
            invalid(event, 'Trial payload has missing or unexpected fields')

    def common(event):
        nonlocal identity, origin, last_at
        if (event.get('provenance') != PROVENANCE or type(event.get('adapter_abi')) is not int or event['adapter_abi'] not in (5, 6)
                or event['adapter_abi'] != adapter_abi
                or event.get('scope') != 'local_on_foot_callback' or event.get('input_writes') is not True
                or event.get('network_active') is not False or event.get('remote_peds_created') is not False
                or event.get('velocity_units', 1) is not None):
            invalid(event, 'Trial provenance, ABI, local ownership scope or explicit activity flags mismatch')
        reference, ped, at, elapsed = (event.get(key) for key in ('reference_id', 'ped', 'at_ms', 'elapsed_ms'))
        if elapsed is None and started is None and event['event'] == 'native_movement_trial_finished':
            null_identity = reference is None and ped is None
            pending_identity = (isinstance(reference, str) and re.fullmatch(r'mta-movement-\d{1,16}', reference)
                                and type(ped) is int and 1 <= ped <= 0x7fffffff)
            if not (null_identity or pending_identity) or (at is not None and not clock(at)):
                invalid(event, 'Unbound finish ownership or clock is malformed')
            return False
        if (not isinstance(reference, str) or not re.fullmatch(r'mta-movement-\d{1,16}', reference)
                or type(ped) is not int or not 1 <= ped <= 0x7fffffff or not clock(at) or not clock(elapsed)):
            invalid(event, 'Trial identity or safe integer clocks are malformed')
            return False
        if identity is None:
            identity, origin = (reference, ped), at - elapsed
            if not clock(origin) or reference != 'mta-movement-' + str(origin):
                invalid(event, 'Reference ID does not bind the original start clock')
        if (reference, ped) != identity or at - origin != elapsed:
            invalid(event, 'Trial identity, local ped or elapsed clock changed')
        if last_at is not None and at < last_at:
            invalid(event, 'Trial clocks reversed')
        last_at = at
        return True

    def metrics(event, final=False):
        nonlocal last_state
        state = event.get('state')
        if (not isinstance(state, dict) or set(state) != METRICS
                or any(type(value) is not int or not 0 <= value <= 0xffffffff for value in state.values())):
            invalid(event, 'Expected all twelve unsigned integer metrics')
            return False
        if state['bound'] > 1 or state['active'] > 1 or state['failureReason'] > 17 or state['lastMoveState'] > 1000:
            invalid(event, 'Native state flags or raw move-state bounds mismatch')
        if state['scopedSteps'] > state['callbackCount'] or state['restoredSteps'] > state['scopedSteps'] or state['neutralQueries'] > state['routedQueries']:
            invalid(event, 'Native callback, routed-query or restoration counter relation is impossible')
        if last_state is not None and any(state[key] < last_state[key] for key in COUNTERS):
            invalid(event, 'Native counters regressed or reset during the same trial')
        last_state = state
        if any(state[key] for key in ('restoreFailures', 'foreignThreadCallbacks', 'nestedCallbacks', 'failureReason')):
            if len(failures) < 32:
                failures.append({'line': event['line'], 'event': event['event'], 'reason': 'Native trial reported a restoration/thread/nesting/binding failure', 'state': state})
        if final:
            if state['bound'] or state['active'] or state['restoredSteps'] != state['scopedSteps']:
                invalid(event, 'Cleanup must unbind and restore every applied scope before publication')
        elif state['bound'] != 1 or state['active'] != 1:
            invalid(event, 'A sample requires the bound active local trial')
        return True

    for line, raw in enumerate(text.splitlines(), 1):
        if re.search(r'\[INFO\]\s+New event received:\s*"host shut down"\s*$', raw): shutdowns.append(line)
        if re.search(r'\[INFO\]\s+Exit\s*$', raw): exits.append(line)
        try:
            payload = json_event(raw)
        except (RecursionError, OverflowError):
            invalid({'line': line}, 'JSON exceeds the bounded parser representation')
            continue
        if payload is None:
            if 'native_movement_trial_' in raw or 'cleanup_failed' in raw:
                invalid({'line': line}, 'Malformed trial marker cannot be silently skipped')
            continue
        event = {**payload, 'line': line}
        if len(events) >= MAX_EVENTS:
            if not overflow:
                invalid(event, 'Bounded event count exceeded'); overflow = True
            continue
        events.append(event); kind = event['event']; counts[kind] += 1
        if kind == 'adapter_phase' and event.get('phase') == 'entry':
            entries.append(event)
            if len(entries) != 1 or started or event.get('mode') != 'smoke': invalid(event, 'Exactly one Smoke entry must precede the trial')
        if kind in {'adapter_failed', 'cleanup_failed'}:
            if len(failures) < 32: failures.append(event)
        if (kind.startswith(('online_', 'remote_', 'native_pad_', 'native_input_', 'native_gait_'))
                or kind in {'connecting', 'connected', 'joined', 'welcome', 'input_sent', 'game_input_written', 'local_controller_sample'}
                or event.get('mode') == 'online' or event.get('network_active') is True or event.get('remote_peds_created') is True):
            invalid(event, 'Network, remote entity or unrelated reference activity is outside this trial')
        if event.get('input_writes') is True and not (kind.startswith('native_movement_trial_') or kind == 'adapter_start'):
            invalid(event, 'Input-write activity lacks the scoped trial provenance')
        if kind == 'adapter_start':
            adapter_abi = event.get('adapter_abi')
            if (counts[kind] != 1 or started is not None or len(entries) != 1 or event.get('mode') != 'smoke'
                    or event.get('host') != 'sa_unreal' or event.get('endpoint', 1) is not None
                    or event.get('stop_key') != 'Ctrl+Shift+End' or event.get('native_movement_trial') is not True
                    or event.get('provenance') != PROVENANCE or type(event.get('adapter_abi')) is not int or event['adapter_abi'] not in (5, 6)
                    or event.get('input_writes') is not True or event.get('network_active') is not False or event.get('remote_peds_created') is not False
                    or any(not isinstance(event.get(key), str) or not event[key] for key in ('cleo', 'host_version', 'definitions'))):
                invalid(event, 'Adapter start must identify one isolated ABI5 or ABI6 Smoke movement trial')
        if not kind.startswith('native_movement_trial_'): continue
        before = issue_total; owned = common(event)
        if len(entries) != 1 or counts['adapter_start'] != 1: invalid(event, 'Trial markers must follow one isolated Smoke adapter start')
        if kind == 'native_movement_trial_started':
            exact(event, {'duration_ms', 'phases'})
            declared = [{'until_ms': end, 'phase': name, 'code': code} for end, name, code in PHASES]
            schedule = event.get('phases')
            typed_schedule = (isinstance(schedule, list) and len(schedule) == 7
                              and all(isinstance(item, dict) and set(item) == {'until_ms', 'phase', 'code'}
                                      and type(item['until_ms']) is int and type(item['code']) is int
                                      and type(item['phase']) is str for item in schedule))
            if (started is not None or finished is not None or not owned or event.get('elapsed_ms') != 0
                    or type(event.get('duration_ms')) is not int or event['duration_ms'] != 22000 or not typed_schedule or schedule != declared):
                invalid(event, 'Successful start must bind the exact seven-phase twenty-two-second schedule')
            started = event
        elif kind in {'native_movement_trial_phase', 'native_movement_trial_sample'}:
            exact(event, {'phase', 'code'} | ({'position', 'heading', 'raw_velocity', 'state'} if kind.endswith('_sample') else set()))
            index = phase_index(event['elapsed_ms']) if owned else None
            if (not started or finished or index is None or event.get('phase') != PHASES[index][1]
                    or type(event.get('code')) is not int or event['code'] != PHASES[index][2]):
                invalid(event, 'Phase/sample is outside its announced bounded schedule')
            if kind.endswith('_phase'):
                if index is None or index != current_phase + 1: invalid(event, 'Every scheduled phase must occur once in order')
                if index is not None: seen_phases.append(index); current_phase = index
            else:
                if index != current_phase: invalid(event, 'A sample must follow its matching phase transition')
                if not vector(event.get('position')) or not vector(event.get('raw_velocity')) or not finite(event.get('heading')):
                    invalid(event, 'Position, heading or raw velocity is not finite bounded XYZ evidence')
                valid_metrics = metrics(event)
                if counts[kind] > MAX_SAMPLES: invalid(event, 'Sample count exceeds the fixed twenty-two-second/125ms cap')
                if last_sample is None:
                    if event.get('elapsed_ms') != 0: invalid(event, 'First sample must share the successful start clock')
                elif not owned or not clock(last_sample.get('at_ms')) or not 125 <= event['at_ms'] - last_sample['at_ms'] <= MAX_CAPTURE_GAP:
                    invalid(event, 'Sample gap is early, reversed or exceeds the observation budget')
                if owned and valid_metrics:
                    if event['elapsed_ms'] >= 2000 and event['state']['scopedSteps'] == 0: invalid(event, 'No native control scope observed within startup budget')
                    if event['elapsed_ms'] >= 4000 and event['state']['routedQueries'] == 0: invalid(event, 'No routed provider query observed within startup budget')
                (accepted if issue_total == before else rejected).append(event)
                last_sample = event
        elif kind == 'native_movement_trial_finished':
            exact(event, {'reason', 'samples', 'cleanup_passed', 'state', 'stop_requested_at_ms',
                          'cleanup_attempts', 'cleanup_wait_ms'}, {'error', 'cleanup_error'})
            if finished is not None: invalid(event, 'Trial may finish only once')
            if type(event.get('samples')) is not int or event['samples'] != counts['native_movement_trial_sample']:
                invalid(event, 'Finish sample count does not match the whole stream')
            if not isinstance(event.get('reason'), str) or not event['reason']: invalid(event, 'Finish reason is malformed')
            if event.get('cleanup_passed') is not True: invalid(event, 'Native cleanup did not report a complete restoration')
            request, wait, attempts = (event.get(key) for key in ('stop_requested_at_ms', 'cleanup_wait_ms', 'cleanup_attempts'))
            if type(attempts) is not int or not 1 <= attempts <= 0xffffffff:
                invalid(event, 'Cleanup must report a positive bounded integer attempt count')
            terminal_clock = (clock(request) and clock(event.get('at_ms')) and clock(wait)
                              and request <= event['at_ms'] and event['at_ms'] - request == wait)
            unknown_unbound_clock = started is None and request is None and wait is None
            if not terminal_clock and not unknown_unbound_clock:
                invalid(event, 'Cleanup request and terminal clocks must establish the exact nonnegative wait')
            if event.get('cleanup_passed') is True and terminal_clock and wait > MAX_CLEANUP_WAIT:
                invalid(event, 'Successful cleanup exceeds the two-second detachment deadline')
            if started:
                metrics(event, final=True)
                if not terminal_clock or not clock(origin) or request < origin:
                    invalid(event, 'A bound trial requires a cleanup request clock within its original lifetime')
                if event.get('reason') == 'time_budget':
                    if (not owned or not terminal_clock or not 22000 <= request - origin <= 23999
                            or not 22000 <= event['elapsed_ms'] <= 25999 or last_sample is None
                            or not clock(last_sample.get('at_ms'))
                            or not 0 <= request - last_sample['at_ms'] <= MAX_CAPTURE_GAP
                            or event['at_ms'] - last_sample['at_ms'] > MAX_CAPTURE_GAP + MAX_CLEANUP_WAIT):
                        invalid(event, 'Time-budget finish requires the full action schedule and separately bounded cleanup/final sample clocks')
            elif event.get('state') is not None or event.get('samples') != 0:
                invalid(event, 'An unbound finish cannot publish native scope evidence')
            if 'error' in event or 'cleanup_error' in event:
                if len(failures) < 32: failures.append(event)
            finished = event
        else:
            invalid(event, 'Unknown native movement trial marker')

    normal_exit = len(shutdowns) == len(exits) == 1 and finished is not None and finished['line'] < shutdowns[0] < exits[0]
    if (shutdowns or exits) and not normal_exit: invalid({'line': (shutdowns + exits)[0]}, 'Host shutdown and Exit must occur once in order after trial cleanup')
    summaries = []
    for index, (end, name, code) in enumerate(PHASES):
        rows = [row for row in accepted if phase_index(row['elapsed_ms']) == index]
        progress = bool(len(rows) >= 2 and all(rows[-1]['state'][key] > rows[0]['state'][key] for key in ('scopedSteps', 'routedQueries', 'restoredSteps')))
        summaries.append({'index': index, 'phase': name, 'code': code, 'until_ms': end, 'samples': len(rows),
                          'scope_query_restore_progress': progress,
                          'raw_velocity_nonzero_samples': sum(any(row['raw_velocity'].values()) for row in rows),
                          'raw_move_states': sorted({row['state']['lastMoveState'] for row in rows}),
                          'first_state': rows[0]['state'] if rows else None, 'last_state': rows[-1]['state'] if rows else None})
    flags = {'one_smoke_entry': len(entries) == 1, 'one_isolated_adapter_start': counts['adapter_start'] == 1,
             'one_trial_started': counts['native_movement_trial_started'] == 1, 'one_trial_finished': counts['native_movement_trial_finished'] == 1,
             'all_phases_announced': seen_phases == list(range(7)), 'all_phases_sampled': all(item['samples'] for item in summaries),
             'movement_phases_have_scoped_progress': all(item['scope_query_restore_progress'] for item in summaries if item['code'] in (1, 2, 3)),
             'normal_time_budget_completion': finished is not None and finished.get('reason') == 'time_budget',
             'valid_payloads': issue_total == 0 and not rejected, 'no_failures': not failures,
             'samples_present': 0 < len(accepted) <= MAX_SAMPLES, 'required_exit_verified': not require_exit or normal_exit}
    complete = all(flags.values())
    return {'schema_version': 1, 'status': 'native_movement_trial_observed' if complete else 'adapter_failed' if failures else 'invalid_evidence' if issue_total else 'incomplete',
            'native_movement_trial_complete': complete, 'native_scope_and_restoration_logged': complete,
            'visual_success_verified': False, 'no_sliding_verified': False, 'velocity_units_verified': False,
            'remote_player_context_verified': False, 'full_backend_verified': False, 'all_controller_actions_verified': False,
            'physical_input_mapping_verified': False, 'engine_input_write_verified': False,
            'normal_exit_verified': normal_exit, 'require_exit': require_exit, 'flags': flags,
            'phase_count': len(seen_phases), 'phases': summaries, 'sample_count': counts['native_movement_trial_sample'],
            'accepted_sample_count': len(accepted), 'accepted_samples': accepted, 'rejected_sample_count': len(rejected), 'rejected_samples': rejected,
            'event_counts': dict(counts), 'events': events, 'issue_count': issue_total, 'issues': issues[:64], 'failures': failures,
            'shutdown_evidence': {'host_shutdown_lines': shutdowns, 'exit_lines': exits}, 'notice': NOTICE}


def safe_report(value, depth=0):
    # Preserve invalid nonstandard JSON evidence visibly while producing a valid
    # assessment document. The immutable whole raw file remains authoritative.
    if depth > 64: return {'invalid_payload_depth_truncated': True}
    if isinstance(value, float) and not math.isfinite(value): return repr(value)
    if isinstance(value, list): return [safe_report(item, depth + 1) for item in value]
    if isinstance(value, dict): return {key: safe_report(item, depth + 1) for key, item in value.items()}
    return value


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-exit', action='store_true')
    parser.add_argument('--source-reference', type=Path, action='append', default=[])
    args = parser.parse_args(argv)
    try:
        if args.log.resolve() == args.output.resolve() or (args.output.exists() and args.log.samefile(args.output)):
            raise ValueError('Output must differ from the raw evidence log')
        snapshot = read_snapshot(args.log)
        result = analyze_log(snapshot['data'].decode('utf-8-sig', errors='replace'), args.require_exit)
        references = []
        for reference in args.source_reference:
            evidence = read_snapshot(reference)
            references.append({'path': str(reference.resolve()), 'sha256': evidence['sha256'], 'size_bytes': evidence['size_bytes']})
        result.update(log=str(args.log.resolve()), evidence_scope='entire_log', source_line_origin=str(args.log.resolve()),
                      log_fingerprint={key: snapshot[key] for key in ('size_bytes', 'mtime_ns', 'sha256')},
                      source_references=references, source_reference_scope='caller_supplied_references_only',
                      reader_path=str(Path(__file__).resolve()), reader_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    except (OSError, ValueError) as error:
        result = {'schema_version': 1, 'status': 'read_error', 'error': str(error), 'log': str(args.log),
                  'native_movement_trial_complete': False, 'native_scope_and_restoration_logged': False,
                  'visual_success_verified': False, 'no_sliding_verified': False, 'velocity_units_verified': False,
                  'remote_player_context_verified': False, 'full_backend_verified': False,
                  'engine_input_write_verified': False, 'notice': NOTICE}
        if 'Output must differ' in str(error):
            print(json.dumps(result)); return 2
    try:
        result = safe_report(result)
        args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    except (OSError, ValueError) as error:
        print(json.dumps({'status': 'write_error', 'error': str(error)})); return 2
    print(json.dumps(result, allow_nan=False))
    return 0 if result['native_movement_trial_complete'] else 2

if __name__ == '__main__': sys.exit(main())
