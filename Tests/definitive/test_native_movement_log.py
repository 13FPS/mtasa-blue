"""Synthetic full-stream movement evidence; no game or process operations."""
from copy import deepcopy
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'utils/definitive_edition'))
import native_movement_log as reader

EPOCH = 1000000
PHASES = [(3000, 'neutral', 0), (7000, 'mta_walk', 1), (9000, 'pause', 0), (13000, 'mta_forwards', 2),
          (15000, 'pause', 0), (19000, 'mta_sprint', 3), (22000, 'final_neutral', 0)]

def event(kind, elapsed=0, **extra):
    return {'event': kind, 'reference_id': 'mta-movement-' + str(EPOCH), 'ped': 17, 'at_ms': EPOCH + elapsed,
            'elapsed_ms': elapsed, 'provenance': 'mta_game_de_scoped_local_movement', 'adapter_abi': 5,
            'scope': 'local_on_foot_callback', 'input_writes': True, 'network_active': False,
            'remote_peds_created': False, 'velocity_units': None, **extra}

def state(steps, active=1):
    return {'bound': active, 'active': active, 'callbackCount': steps + 2, 'scopedSteps': steps,
            'routedQueries': steps * 4, 'neutralQueries': steps * 2, 'restoredSteps': steps, 'restoreFailures': 0,
            'foreignThreadCallbacks': 0, 'nestedCallbacks': 0, 'failureReason': 0, 'lastMoveState': 0}

def fixture(exit=True):
    rows = [{'event': 'adapter_phase', 'phase': 'entry', 'mode': 'smoke'},
            {'event': 'adapter_start', 'host': 'sa_unreal', 'cleo': '1.5.1', 'host_version': '1.0', 'definitions': '0.5',
             'mode': 'smoke', 'endpoint': None, 'stop_key': 'Ctrl+Shift+End', 'native_movement_trial': True,
             'provenance': 'mta_game_de_scoped_local_movement', 'adapter_abi': 5, 'input_writes': True,
             'network_active': False, 'remote_peds_created': False},
            event('native_movement_trial_started', duration_ms=22000,
                  phases=[{'until_ms': end, 'phase': name, 'code': code} for end, name, code in PHASES])]
    previous = -1
    for index, elapsed in enumerate(range(0, 22000, 125)):
        phase = next(i for i, value in enumerate(PHASES) if elapsed < value[0])
        _, name, code = PHASES[phase]
        if phase != previous:
            rows.append(event('native_movement_trial_phase', elapsed, phase=name, code=code))
            previous = phase
        rows.append(event('native_movement_trial_sample', elapsed, phase=name, code=code,
                          position={'x': elapsed / 1000, 'y': 0, 'z': 3}, heading=0,
                          raw_velocity={'x': code / 3, 'y': 0, 'z': 0}, state=state(index + 1)))
    rows.append(event('native_movement_trial_finished', 22000, reason='time_budget', samples=176,
                      cleanup_passed=True, state=state(178, 0), stop_requested_at_ms=EPOCH + 22000,
                      cleanup_attempts=1, cleanup_wait_ms=0))
    if exit:
        rows += ['[INFO] New event received: "host shut down"', '[INFO] Exit']
    return rows

def text(rows):
    return '\n'.join(row if isinstance(row, str) else '[INFO] ' + json.dumps(row) for row in rows)

def samples(rows):
    return [row for row in rows if isinstance(row, dict) and row['event'] == 'native_movement_trial_sample']

class MovementLogTests(unittest.TestCase):
    def analyze(self, rows=None, require_exit=False):
        return reader.analyze_log(text(fixture() if rows is None else rows), require_exit)

    def test_complete_stream(self):
        result = self.analyze(require_exit=True)
        self.assertTrue(result['native_movement_trial_complete'])
        self.assertEqual(result['accepted_sample_count'], 176)
        self.assertTrue(result['normal_exit_verified'])
        self.assertEqual(result['phase_count'], 7)
        for name in ('visual_success_verified', 'no_sliding_verified', 'remote_player_context_verified', 'full_backend_verified', 'velocity_units_verified'):
            self.assertIs(result[name], False)

    def test_quoted_cleo_json(self):
        raw = '\n'.join(row if isinstance(row, str) else '[INFO] ' + json.dumps(json.dumps(row)) for row in fixture())
        self.assertTrue(reader.analyze_log(raw)['native_movement_trial_complete'])

    def test_new_abi6_retains_the_same_scoped_legacy_trial_contract(self):
        rows = fixture()
        for row in rows:
            if isinstance(row, dict) and 'adapter_abi' in row:
                row['adapter_abi'] = 6
        self.assertTrue(self.analyze(rows, True)['native_movement_trial_complete'])

    def test_mixed_abi_versions_cannot_share_a_single_trial(self):
        rows = fixture()
        rows[3]['adapter_abi'] = 6
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_require_exit(self):
        self.assertTrue(self.analyze(fixture(False))['native_movement_trial_complete'])
        self.assertFalse(self.analyze(fixture(False), True)['native_movement_trial_complete'])

    def test_extra_entry_cannot_hide_earlier_success(self):
        rows = fixture(); rows.append({'event': 'adapter_phase', 'phase': 'entry', 'mode': 'smoke'})
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_missing_adapter_start(self):
        rows = fixture(); del rows[1]
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_start_contract_mutations(self):
        for key, value in [('native_movement_trial', False), ('adapter_abi', 4), ('input_writes', False),
                           ('network_active', True), ('endpoint', '127.0.0.1'), ('mode', 'online')]:
            with self.subTest(key=key):
                rows = fixture(); rows[1][key] = value
                self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_common_contract_mutations(self):
        for key, value in [('provenance', 'other'), ('adapter_abi', True), ('scope', 'remote'), ('input_writes', False),
                           ('remote_peds_created', True), ('velocity_units', 'm/s'), ('ped', 18), ('reference_id', 'other')]:
            with self.subTest(key=key):
                rows = fixture(); samples(rows)[3][key] = value
                self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_integer_clock_and_consistency(self):
        for key, value in [('at_ms', True), ('elapsed_ms', -1), ('at_ms', EPOCH), ('elapsed_ms', 126)]:
            with self.subTest(key=key, value=value):
                rows = fixture(); samples(rows)[1][key] = value
                self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_early_sample_and_large_gap(self):
        for elapsed in (124, 2126):
            rows = fixture(); target = samples(rows)[1]; target['elapsed_ms'] = elapsed; target['at_ms'] = EPOCH + elapsed
            self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_unsampled_tick_prefix_has_actual_producer_bound(self):
        for gap, complete in ((2124, True), (2125, False)):
            with self.subTest(gap=gap):
                rows = fixture()
                rows = [row for row in rows if not (isinstance(row, dict) and row['event'] == 'native_movement_trial_sample'
                                                   and 0 < row['elapsed_ms'] < 2250)]
                rows.insert(5, event('native_movement_trial_sample', gap, phase='neutral', code=0,
                                     position={'x': 0, 'y': 0, 'z': 3}, heading=0,
                                     raw_velocity={'x': 0, 'y': 0, 'z': 0}, state=state(2)))
                rows[-3]['samples'] = len(samples(rows))
                self.assertEqual(self.analyze(rows)['native_movement_trial_complete'], complete)

    def test_final_unsampled_tick_prefix_is_bounded(self):
        for elapsed, complete in ((23999, True), (24000, False)):
            rows = fixture(); rows[-3]['elapsed_ms'] = elapsed; rows[-3]['at_ms'] = EPOCH + elapsed
            rows[-3]['stop_requested_at_ms'] = EPOCH + elapsed
            self.assertEqual(self.analyze(rows)['native_movement_trial_complete'], complete)

    def test_cleanup_wait_has_separate_bounded_clock(self):
        for request, wait, complete in ((22000, 100, True), (22000, 2000, True),
                                        (23999, 2000, True), (22000, 2001, False), (24000, 0, False)):
            with self.subTest(request=request, wait=wait):
                rows = fixture(); finish = rows[-3]
                finish.update(at_ms=EPOCH + request + wait, elapsed_ms=request + wait,
                              stop_requested_at_ms=EPOCH + request, cleanup_wait_ms=wait,
                              cleanup_attempts=3)
                self.assertEqual(self.analyze(rows)['native_movement_trial_complete'], complete)

    def test_cleanup_terminal_clock_and_attempts_are_strict(self):
        for key, value in (('stop_requested_at_ms', None), ('stop_requested_at_ms', EPOCH + 22001),
                           ('stop_requested_at_ms', EPOCH - 1), ('cleanup_wait_ms', None),
                           ('cleanup_wait_ms', True), ('cleanup_wait_ms', -1), ('cleanup_wait_ms', 1),
                           ('cleanup_attempts', 0), ('cleanup_attempts', True), ('cleanup_attempts', 2**32)):
            with self.subTest(key=key, value=value):
                rows = fixture(); rows[-3][key] = value
                self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_old_terminal_payload_cannot_prove_new_cleanup_contract(self):
        rows = fixture()
        for key in ('stop_requested_at_ms', 'cleanup_wait_ms', 'cleanup_attempts'):
            del rows[-3][key]
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_invalid_start_identity_does_not_crash_cleanup_clock_validation(self):
        rows = fixture(); rows[2]['ped'] = None
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_cleanup_cannot_hide_a_stale_last_sample(self):
        rows = fixture(); finish = rows[-3]
        finish.update(at_ms=EPOCH + 25000, elapsed_ms=25000,
                      stop_requested_at_ms=EPOCH + 24000, cleanup_wait_ms=1000, cleanup_attempts=2)
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_cleanup_failure_after_two_second_deadline_remains_failure(self):
        rows = fixture(); finish = rows[-3]
        finish.update(at_ms=EPOCH + 24001, elapsed_ms=24001,
                      stop_requested_at_ms=EPOCH + 22000, cleanup_wait_ms=2001, cleanup_attempts=3,
                      cleanup_passed=False, cleanup_error='MTA movement cleanup deadline exceeded')
        result = self.analyze(rows)
        self.assertFalse(result['native_movement_trial_complete'])
        self.assertTrue(result['failures'])

    def test_missing_phase(self):
        rows = fixture(); rows = [row for row in rows if not (isinstance(row, dict) and row['event'] == 'native_movement_trial_phase' and row['phase'] == 'mta_walk')]
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_phase_outside_schedule(self):
        rows = fixture(); target = samples(rows)[40]; target['phase'] = 'mta_sprint'; target['code'] = 3
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_changed_schedule(self):
        rows = fixture(); rows[2]['phases'][1]['until_ms'] = 6500
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_declared_phase_boolean_is_not_an_integer(self):
        rows = fixture(); rows[2]['phases'][1]['code'] = True
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_issue_storage_limit_does_not_accept_invalid_samples(self):
        rows = fixture()
        for sample in samples(rows):
            sample['state'] = None
            sample['provenance'] = 'wrong'
        result = self.analyze(rows)
        self.assertGreater(result['issue_count'], 128)
        self.assertEqual(result['accepted_sample_count'], 0)
        self.assertFalse(result['native_movement_trial_complete'])

    def test_metrics_schema_and_ranges(self):
        for key, value in [('bound', 2), ('active', False), ('scopedSteps', -1), ('callbackCount', 2**32), ('lastMoveState', 1001), ('failureReason', 18)]:
            with self.subTest(key=key):
                rows = fixture(); samples(rows)[2]['state'][key] = value
                self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_counter_relations(self):
        for key, value in [('callbackCount', 0), ('restoredSteps', 1000), ('neutralQueries', 9999)]:
            rows = fixture(); samples(rows)[2]['state'][key] = value
            self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_counter_regression(self):
        rows = fixture(); samples(rows)[50]['state'] = state(1)
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_native_failure_counters(self):
        for key in ('restoreFailures', 'foreignThreadCallbacks', 'nestedCallbacks', 'failureReason'):
            rows = fixture(); samples(rows)[10]['state'][key] = 1
            self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_no_scopes_or_routes(self):
        for key in ('scopedSteps', 'routedQueries'):
            rows = fixture()
            for sample in samples(rows):
                sample['state'][key] = 0
                if key == 'scopedSteps': sample['state']['restoredSteps'] = 0
                else: sample['state']['neutralQueries'] = 0
            self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_phase_requires_scoped_progress(self):
        rows = fixture(); phase_rows = [row for row in samples(rows) if row['phase'] == 'mta_sprint']
        fixed = deepcopy(phase_rows[0]['state'])
        for row in phase_rows: row['state'] = deepcopy(fixed)
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_cleanup_incomplete(self):
        for key, value in [('cleanup_passed', False), ('state', None), ('samples', 175), ('reason', 'requested')]:
            rows = fixture(); rows[-3][key] = value
            self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_cleanup_native_state(self):
        for key, value in [('active', 1), ('bound', 1), ('restoredSteps', 177), ('failureReason', 11), ('scopedSteps', 1)]:
            rows = fixture(); rows[-3]['state'][key] = value
            self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_failure_after_success_preserved(self):
        rows = fixture(); rows.append({'event': 'adapter_failed', 'error': 'late'})
        result = self.analyze(rows)
        self.assertFalse(result['native_movement_trial_complete']); self.assertTrue(result['failures'])

    def test_unrelated_reference_and_network(self):
        for event in ({'event': 'connected'}, {'event': 'native_pad_sample'}, {'event': 'remote_ped_created'}, {'event': 'game_input_written'}):
            rows = fixture(); rows.insert(5, event)
            self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_malformed_marker(self):
        rows = fixture(); rows.insert(5, '{"event":"native_movement_trial_sample",broken')
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_nonfinite_or_fake_payload(self):
        for key, value in [('heading', float('nan')), ('position', {'x': 0, 'y': 0, 'z': 0, 'fake': 1}), ('raw_velocity', {'x': 100001, 'y': 0, 'z': 0})]:
            rows = fixture(); samples(rows)[1][key] = value
            self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_unexpected_event_fields(self):
        rows = fixture(); samples(rows)[1]['native_walk_verified'] = True
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_missing_finish(self):
        rows = fixture(False); rows.pop()
        self.assertEqual(self.analyze(rows)['status'], 'incomplete')

    def test_exit_order(self):
        rows = fixture(); rows[-2:] = list(reversed(rows[-2:]))
        self.assertFalse(self.analyze(rows)['native_movement_trial_complete'])

    def test_cli_preserves_full_log_and_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            raw = Path(directory) / 'raw.log'; output = Path(directory) / 'assessment.json'
            raw.write_text(text(fixture()), encoding='utf-8')
            original = raw.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(['--log', str(raw), '--output', str(output), '--require-exit', '--source-reference', str(Path(__file__))])
            result = json.loads(output.read_text())
            self.assertEqual(code, 0); self.assertEqual(raw.read_bytes(), original)
            self.assertEqual(result['evidence_scope'], 'entire_log'); self.assertEqual(result['source_line_origin'], str(raw.resolve()))
            self.assertEqual(len(result['source_references']), 1); self.assertEqual(result['log_fingerprint']['size_bytes'], len(original))

    def test_cli_rejects_raw_output_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            raw = Path(directory) / 'raw.log'; raw.write_text(text(fixture()), encoding='utf-8'); original = raw.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()): code = reader.main(['--log', str(raw), '--output', str(raw)])
            self.assertEqual(code, 2); self.assertEqual(raw.read_bytes(), original)

    def test_cli_invalid_nan_still_saves_valid_assessment(self):
        with tempfile.TemporaryDirectory() as directory:
            raw = Path(directory) / 'raw.log'; output = Path(directory) / 'assessment.json'
            rows = fixture(); samples(rows)[1]['heading'] = float('nan')
            raw.write_text(text(rows), encoding='utf-8')
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(['--log', str(raw), '--output', str(output)])
            self.assertEqual(code, 2)
            result = json.loads(output.read_text())
            self.assertFalse(result['native_movement_trial_complete'])
            self.assertEqual(result['events'][5]['heading'], 'nan')

    def test_unbound_cancel_is_incomplete(self):
        rows = fixture(False)[:2]
        rows.append(event('native_movement_trial_finished', reference_id=None, ped=None, elapsed_ms=None,
                          reason='Ctrl+Shift+End', samples=0, cleanup_passed=True, state=None,
                          stop_requested_at_ms=EPOCH, cleanup_attempts=1, cleanup_wait_ms=0))
        result = self.analyze(rows)
        self.assertEqual(result['status'], 'incomplete')
        self.assertEqual(result['accepted_sample_count'], 0)

if __name__ == '__main__': unittest.main()
