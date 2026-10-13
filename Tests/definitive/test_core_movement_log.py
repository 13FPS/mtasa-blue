"""Adversarial whole-log checks for the actual Core-owned write contract."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'utils/definitive_edition'))
import core_movement_log as reader


def state(sequence, elapsed, names=(), terminal=False):
    current = [0] * 24
    for name, field, value in (('forwards', 1, -128), ('backwards', 1, 128), ('left', 0, -128),
                                ('right', 0, 128), ('sprint', 16, 255), ('walk', 21, 255)):
        if name in names:
            current[field] = value
    held = sum(1 << reader.ACTIONS[name] for name in names)
    count = 599 if terminal else sequence
    values = [1, 2 if terminal else 1, sequence, elapsed, 0 if terminal else 3, 6, 0, 6, 0, 0, 1,
              count, held, 0, 1 if terminal else 0, int(terminal)]
    values += [value & 0xffffffff for value in current + current]
    counters = [0 if terminal else 1, 0 if terminal else 1, sequence * 3, sequence * 3,
                sequence * 12, sequence, sequence * 3, 0, 0, 0, 0, 1]
    for value in counters:
        values.extend((value & 0xffffffff, value >> 32))
    values.extend((15, 15, 1, 1, int(terminal), 0, 1, 0))
    return values


def fixture():
    rows = [dict(event='adapter_phase', phase='entry', mode='smoke'),
        dict(event='adapter_armed', mode='smoke', activate_key='Ctrl+Shift+F9', stop_key='Ctrl+Shift+End',
             bridge_loaded=False, game_adapter_active=False, network_active=False),
        dict(event='adapter_activation_requested', mode='smoke', activate_key='Ctrl+Shift+F9'),
        dict(event='adapter_activation_released', mode='smoke', activate_key='Ctrl+Shift+F9', at_ms=990,
             release_wait_ms=50, bridge_loaded=False, game_adapter_active=False, network_active=False),
        dict(event='adapter_start', host='sa_unreal', cleo='1.5.1', host_version='1.0', definitions='0.500',
             mode='smoke', endpoint=None, stop_key='Ctrl+Shift+End', core_movement_reference=True,
             provenance='mta_core_configured_movement', core_api=4, adapter_abi=6,
             network_active=False, input_writes=True, remote_peds_created=False),
        dict(event='core_movement_reference_started', at_ms=1000, core_api=4, adapter_abi=6,
             sample_interval_ms=100, budget_ms=30000, readiness_budget_ms=10000,
             cleanup_deadline_ms=2000, context_source='engine_eligibility')]
    actions = ((), ('forwards',), ('backwards',), ('left',), ('right',), ('forwards', 'sprint'), ('forwards', 'walk'), ())
    for index, names in enumerate(actions):
        elapsed = index * 3000
        rows.append(dict(event='core_movement_reference_sample', at_ms=1100 + elapsed, elapsed_ms=100 + elapsed,
                         state=state(index * 60 + 1, elapsed, names)))
    rows.append(dict(event='core_movement_reference_sample', at_ms=31100, elapsed_ms=30100,
                     state=state(600, 30000, terminal=True)))
    rows.append(dict(event='core_movement_reference_finished', at_ms=31200, elapsed_ms=30200,
                     state=[1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 2, 2, 1, 0, 1, 0, 1, 0],
                     run_result=1, passed=True, cleanup_passed=True, stop_requested=False, samples=9,
                     full_core_ready=False, rendering_started=False, input_writes=True,
                     network_active=False, remote_peds_created=False))
    return rows


def analyze(rows=None, shutdown=True, require_exit=True):
    raw = '\n'.join(json.dumps(row) if isinstance(row, dict) else row for row in (fixture() if rows is None else rows))
    if shutdown:
        raw += '\n[INFO] New event received: "host shut down"\n[INFO] Exit\n'
    return reader.analyze_log(raw, require_exit)


class CoreMovementEvidenceTests(unittest.TestCase):
    def test_complete_scoped_write_keeps_global_claims_false(self):
        result = analyze()
        self.assertTrue(result['core_movement_reference_complete'], result)
        self.assertTrue(result['configured_movement_session_complete'])
        self.assertTrue(result['normal_exit_verified'])
        self.assertEqual(result['missing_controls'], [])
        for field in ('visual_success_verified', 'full_mta_boot_verified', 'de_renderer_verified',
                      'unqualified_engine_input_write_verified', 'remote_player_context_verified'):
            self.assertIs(result[field], False)

    def test_all96_fields_require_exact_unsigned_storage(self):
        for index in range(96):
            for value in (-1, True, .5, 0x100000000):
                with self.subTest(index=index, value=value):
                    rows = fixture(); rows[7]['state'][index] = value
                    self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_eligibility_and_assumed_context_cannot_be_forged(self):
        for index, value in ((4, 7), (88, 7), (89, 7), (90, 0), (91, 0), (94, 0)):
            rows = fixture(); rows[7]['state'][index] = value
            self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_every_native_error_and_restoration_mismatch_prevents_success(self):
        for index, value in ((64, 1), (66, 1), (76, 1), (78, 1), (80, 1), (82, 1), (84, 1),
                             (92, 0), (93, 1), (94, 0), (95, 1), (15, 0), (16, 128), (40, 128),
                             (4, 3), (88, 7), (89, 7), (90, 0)):
            rows = fixture(); rows[-2]['state'][index] = value
            self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_64bit_counters_are_preserved_without_truncation(self):
        rows = fixture()
        for row in rows:
            if row['event'] == 'core_movement_reference_sample':
                for metric in (2, 3, 4, 6):
                    row['state'][65 + metric * 2] = 1
        result = analyze(rows)
        self.assertTrue(result['core_movement_reference_complete'], result)
        self.assertGreater(result['native_metrics']['scopedSteps'], 0xffffffff)

    def test_partial_coverage_is_not_lifecycle_failure_or_visual_success(self):
        rows = fixture(); rows[12]['state'] = state(361, 18000)
        result = analyze(rows)
        self.assertTrue(result['configured_movement_session_complete'])
        self.assertFalse(result['core_movement_reference_complete'])
        self.assertEqual(result['missing_controls'], ['walk'])

    def test_pending_cleanup_is_retained_and_cannot_count_as_terminal(self):
        rows = fixture(); pending = deepcopy(rows[-2]); pending['state'][1] = 3
        pending['state'][15] = 0; pending['state'][95] = 1; pending['state'][64] = 1
        rows.insert(-2, pending); rows[-2]['state'][2] += 1; rows[-1]['samples'] += 1
        self.assertTrue(analyze(rows)['core_movement_reference_complete'])
        rows = rows[:-2]
        result = analyze(rows, shutdown=False, require_exit=False)
        self.assertFalse(result['configured_movement_session_complete'])
        self.assertTrue(result['cleanup_pending'])

    def test_cleanup_deadline_miss_never_becomes_success_after_restore(self):
        rows = fixture(); rows[-2]['state'][93] = 1
        self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_ended_dll_pin_is_expected_not_silently_unloaded(self):
        rows = fixture(); rows[-1]['state'][15] = 0
        self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_missing_exit_is_optional_only_when_requested(self):
        self.assertFalse(analyze(shutdown=False)['core_movement_reference_complete'])
        self.assertTrue(analyze(shutdown=False, require_exit=False)['core_movement_reference_complete'])

    def test_mixed_modes_restarts_and_malformed_markers_are_never_hidden(self):
        for extra in (dict(event='core_input_reference_started'), dict(event='core_startup_reference_started'),
                      dict(event='adapter_phase', phase='entry', mode='smoke'),
                      dict(event='adapter_failed', error='late failure'),
                      '{"event":"core_movement_reference_sample",broken}'):
            rows = fixture(); rows.append(extra)
            self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_cancel_preserves_failed_outcome_even_if_worker_completed_concurrently(self):
        rows = fixture(); rows.insert(-1, dict(event='core_movement_reference_stop_requested', at_ms=31100, reason='Ctrl+Shift+End'))
        rows[-1]['stop_requested'] = True; rows[-1]['passed'] = False
        self.assertFalse(analyze(rows)['configured_movement_session_complete'])

    def test_sequence_clock_counter_and_mask_mismatch_are_rejected(self):
        for index, value in ((2, 0), (3, 32001), (5, 0), (7, 0), (12, 1), (13, 1), (11, 601), (70, 0), (17, 128)):
            rows = fixture(); rows[7]['state'][index] = value
            self.assertFalse(analyze(rows)['core_movement_reference_complete'])
        rows = fixture(); rows[8]['state'][2] = rows[7]['state'][2]
        self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_malformed_lifecycle_clock_types_cannot_crash_the_reader(self):
        for value in (None, True, '1000', [], {}, -1, .5):
            rows = fixture(); rows[5]['at_ms'] = value
            self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_previous_direction_coverage_is_optional_for_this_focused_write_trial(self):
        rows = fixture()
        for index in (8, 9, 10):
            old = rows[index]['state']
            rows[index]['state'] = state(old[2], old[3])
        self.assertTrue(analyze(rows)['core_movement_reference_complete'])

    def test_invented_write_activity_cannot_be_hidden_in_an_extra_event(self):
        rows = fixture(); rows.append(dict(event='other_write', input_writes=True))
        self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_load_failure_without_native_samples_preserves_diagnostics(self):
        rows = fixture()[:6]
        final = fixture()[-1]
        final.update(samples=0, run_result=0, passed=False)
        final['state'] = [0, 0xffffffff, 0, 0, 0, 0, 0, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0, 0, 1, 0, 0, 4, 0, 126]
        rows.append(final)
        result = analyze(rows)
        self.assertEqual(result['issue_count'], 0, result)
        self.assertFalse(result['configured_movement_session_complete'])
        self.assertFalse(result['scoped_local_engine_input_write_observed'])
        self.assertEqual(result['worker_state'][18], 126)

    def test_abi6_ineligible_context_and_unknown_raw_state_are_valid_failed_evidence(self):
        rows = fixture()
        rows[-2]['state'][14] = 12
        rows[-2]['state'][84] = 18
        rows[-2]['state'][86] = 0xffffffff
        rows[-1].update(passed=False, run_result=0)
        rows[-1]['state'][16:18] = [15, 0]
        result = analyze(rows)
        self.assertEqual(result['issue_count'], 0, result)
        self.assertFalse(result['configured_movement_session_complete'])
        self.assertEqual(result['native_metrics']['failureReason'], 18)
        self.assertEqual(result['native_metrics']['lastMoveState'], 0xffffffff)
        rows = fixture(); rows[-2]['state'][86] = 0xffffffff
        self.assertFalse(analyze(rows)['core_movement_reference_complete'])

    def test_raw_native_state_is32bit_and_not_a_guessed_locomotion_enum(self):
        rows = fixture(); rows[-2]['state'][86] = 1001
        self.assertTrue(analyze(rows)['configured_movement_session_complete'])
        rows = fixture(); rows[-2]['state'][87] = 1
        self.assertFalse(analyze(rows)['core_movement_reference_complete'])


if __name__ == '__main__':
    unittest.main()
