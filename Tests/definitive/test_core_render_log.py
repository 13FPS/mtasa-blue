"""Whole-log acceptance checks for the actual Core-owned render diagnostic."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import core_render_log as reader


def state(sequence=1, elapsed=100, terminal=False):
    result = [0] * 40
    values = {0: 1, 1: 2 if terminal else 1, 2: sequence, 3: elapsed, 4: int(terminal),
              5: 200 if terminal else sequence, 6: int(terminal), 7: int(terminal), 9: 1, 11: 1,
              12: 1, 13: int(not terminal), 14: 1, 15: 1, 17: sequence * 3, 19: sequence * 3,
              25: 42, 26: 1920, 27: 1080, 28: 1, 29: 0, 31: 1, 37: 15}
    for field, value in values.items():
        result[field] = value
    return result


def fixture():
    return [
        dict(event="adapter_phase", phase="entry", mode="smoke"),
        dict(event="adapter_armed", mode="smoke", activate_key="Ctrl+Shift+F9", stop_key="Ctrl+Shift+End",
             bridge_loaded=False, game_adapter_active=False, network_active=False),
        dict(event="adapter_activation_requested", mode="smoke", activate_key="Ctrl+Shift+F9"),
        dict(event="adapter_activation_released", mode="smoke", activate_key="Ctrl+Shift+F9", at_ms=990,
             release_wait_ms=50, bridge_loaded=False, game_adapter_active=False, network_active=False),
        dict(event="adapter_start", host="sa_unreal", cleo="1.5.1", host_version="1.0", definitions="0.500",
             mode="smoke", endpoint=None, stop_key="Ctrl+Shift+End", core_render_reference=True,
             provenance=reader.PROVENANCE, core_api=5, network_active=False, input_writes=False, remote_peds_created=False),
        dict(event="core_render_reference_started", at_ms=1000, core_api=5, sample_interval_ms=100, budget_ms=10000),
        dict(event="core_render_reference_sample", at_ms=1200, elapsed_ms=200, state=state()),
        dict(event="core_render_reference_sample", at_ms=11200, elapsed_ms=10200, state=state(201, 10000, True)),
        dict(event="core_render_reference_finished", at_ms=11300, elapsed_ms=10300,
             state=[1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 2, 2, 1, 0, 1, 0, 1, 0],
             run_result=1, passed=True, cleanup_passed=True, stop_requested=False, samples=2,
             full_core_ready=False, rendering_started=False, input_writes=False, network_active=False, remote_peds_created=False),
    ]


def analyze(rows=None, shutdown=True, require_exit=True):
    text = "\n".join(json.dumps(row) if isinstance(row, dict) else row for row in (fixture() if rows is None else rows))
    if shutdown:
        text += '\n[INFO] New event received: "host shut down"\n[INFO] Exit\n'
    return reader.analyze_log(text, require_exit)


class CoreRenderEvidenceTests(unittest.TestCase):
    def test_observed_callback_and_native_false_return_do_not_claim_pixels_or_com(self):
        result = analyze()
        self.assertTrue(result["core_render_reference_complete"], result)
        self.assertEqual(result["native_metrics"]["accepted_snapshots"], 603)
        self.assertEqual(result["worker_state"][15], 1)
        for name in ("visual_success_verified", "com_present_verified", "de_renderer_verified", "full_mta_boot_verified",
                     "engine_input_write_verified", "remote_player_context_verified"):
            self.assertIs(result[name], False)

    def test_all40_fields_require_exact_unsigned_integers(self):
        for field in range(40):
            for value in (True, -1, .5, 0x100000000):
                rows = fixture()
                rows[6]["state"][field] = value
                with self.subTest(field=field, value=value):
                    self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_every_required_native_and_cleanup_field_is_authoritative(self):
        for field, value in ((1, 3), (3, 9999), (4, 2), (5, 0), (6, 0), (7, 0), (8, 1), (9, 0), (10, 1),
                             (11, 0), (12, 0), (13, 1), (14, 0), (15, 0), (17, 0), (19, 0), (21, 1), (23, 1),
                             (25, 0), (26, 0), (27, 0), (29, 2), (30, 7), (31, 0), (32, 1), (33, 1), (37, 3), (38, 1), (39, 1)):
            rows = fixture()
            rows[-2]["state"][field] = value
            with self.subTest(field=field):
                self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_counter_halves_are_preserved_and_publication_contention_is_reported(self):
        rows = fixture()
        for row in rows[6:8]:
            for high in (18, 20):
                row["state"][high] = 1
            row["state"][35] = 2
            row["state"][17] += 2
        result = analyze(rows)
        self.assertTrue(result["core_render_reference_complete"], result)
        self.assertGreater(result["native_metrics"]["native_present_callbacks"], 0xffffffff)
        self.assertEqual(result["native_metrics"]["publication_misses"], 2)

    def test_publication_misses_and_disjoint_outcomes_cannot_exceed_callbacks(self):
        for field, value in ((35, 1000000), (36, 1), (21, 604), (23, 604), (33, 604), (35, 1)):
            rows = fixture()
            rows[-2]["state"][field] = value
            self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_counter_sequence_epoch_and_clock_regressions_fail(self):
        for field, value in ((2, 1), (3, 0), (5, 0), (15, 2), (17, 1), (19, 1)):
            rows = fixture()
            rows[-2]["state"][field] = value
            self.assertFalse(analyze(rows)["core_render_reference_complete"])
        rows = fixture()
        rows[-2]["at_ms"] = 1199
        self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_a_native_failure_cannot_be_cleared_in_a_later_publication(self):
        rows = fixture()
        rows[6]["state"][30] = 7
        self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_pending_cleanup_is_retained_until_later_terminal(self):
        rows = fixture()
        pending = deepcopy(rows[-2])
        pending["state"][1] = 3
        pending["state"][2] = 200
        pending["state"][6] = 0
        pending["state"][10] = pending["state"][32] = 1
        pending["state"][17] = pending["state"][19] = 600
        rows.insert(-2, pending)
        rows[-1]["samples"] = 3
        self.assertTrue(analyze(rows)["core_render_reference_complete"])
        result = analyze(rows[:-2], shutdown=False, require_exit=False)
        self.assertFalse(result["core_render_reference_complete"])
        self.assertTrue(result["cleanup_pending"])

    def test_cleanup_deadline_miss_remains_failure_after_successful_end(self):
        rows = fixture()
        rows[-2]["state"][8] = 1
        self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_expected_backend_pin_and_actual_worker_load_error_are_not_hidden(self):
        for field, value in ((15, 0), (13, 0), (7, 0), (18, 1114)):
            rows = fixture()
            rows[-1]["state"][field] = value
            self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_missing_normal_exit_is_optional_only_when_explicitly_requested(self):
        self.assertFalse(analyze(shutdown=False)["core_render_reference_complete"])
        self.assertTrue(analyze(shutdown=False, require_exit=False)["core_render_reference_complete"])

    def test_mixed_modes_restarts_and_malformed_markers_cannot_hide_behind_success(self):
        for extra in (dict(event="core_input_reference_started"), dict(event="core_startup_reference_started"),
                      dict(event="core_movement_reference_started"), dict(event="adapter_failed", error="late failure"),
                      dict(event="adapter_phase", phase="entry", mode="smoke"),
                      '{"event":"core_render_reference_sample",broken}',
                      '{"event":"core_render_reference_started","event":"ignored"}'):
            rows = fixture()
            rows.append(extra)
            self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_native_pointers_or_pixels_and_input_claims_are_outside_scope(self):
        for key, value in (("viewport", 1234), ("swapchain", 1234), ("address", 0), ("com_present_verified", True),
                           ("visual_success_verified", True), ("input_writes", True), ("network_active", True)):
            rows = fixture()
            rows[4][key] = value
            self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_quoted_cleo_messages_preserve_same_whole_log_contract(self):
        rows = [json.dumps(json.dumps(row)) for row in fixture()]
        self.assertTrue(analyze(rows)["core_render_reference_complete"])


if __name__ == "__main__":
    unittest.main()
