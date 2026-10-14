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


def missing_observation_fixture():
    rows = fixture()
    for row in rows[6:8]:
        native = row["state"]
        for field in (*range(17, 29), 31, *range(33, 38)):
            native[field] = 0
    rows[-1]["passed"] = False
    rows.append(dict(event="adapter_failed", phase="core_render_reference",
                     error="Error: MTA Core renderer observation incomplete; inspect its saved state", game_adapter_active=False))
    return rows


class CoreRenderEvidenceTests(unittest.TestCase):
    @staticmethod
    def version2(rows, route=2):
        for row in rows:
            if row.get("event") in ("adapter_start", "core_render_reference_started"):
                row.update(core_api=6, observation_api=2, requested_route=route)
            if row.get("event") == "core_render_reference_sample":
                native = row["state"]
                observed = bool(native[19] or native[20])
                native[0] = 2
                native[37] = (15 if route == 1 else 3) if observed else 0
                native += [route, route if observed else 0, (15 if route == 1 else 19) if observed else 0,
                           int(observed and route == 2)]
        return rows

    def test_api2_preserves_both_explicit_routes_without_com_or_pixel_claims(self):
        for route in (1, 2):
            result = analyze(self.version2(fixture(), route))
            self.assertTrue(result["core_render_reference_complete"], result)
            self.assertEqual(result["requested_route"], route)
            self.assertEqual(result["observed_route"], route)
            self.assertEqual(result["presence_known_mask"], 15 if route == 1 else 19)
            self.assertEqual(result["queue_present"], route == 2)
            self.assertFalse(result["com_present_verified"])
            self.assertFalse(result["de_renderer_verified"])

    def test_api2_no_callback_keeps_route_unknown_and_clean_lifecycle_distinct(self):
        result = analyze(self.version2(missing_observation_fixture()))
        self.assertTrue(result["diagnostic_lifecycle_complete"], result)
        self.assertEqual(result["status"], "native_observation_missing")
        self.assertEqual(result["requested_route"], 2)
        self.assertEqual(result["observed_route"], 0)
        self.assertEqual(result["presence_known_mask"], 0)
        self.assertFalse(result["core_render_reference_complete"])

    def test_api2_rejects_crossroute_missing_knowledge_and_fake_device_context(self):
        for field, value in ((0, 1), (37, 15), (40, 1), (40, 0), (41, 1), (41, 0), (42, 3), (42, 23), (43, 0), (43, 2)):
            rows = self.version2(fixture())
            rows[7]["state"][field] = value
            with self.subTest(field=field, value=value):
                result = analyze(rows)
                self.assertFalse(result["core_render_reference_complete"])
                self.assertFalse(result["diagnostic_lifecycle_complete"])
                self.assertGreater(result["issue_count"], 0)

    def test_api2_contract_and_wire_shape_cannot_mix_with_historical_api1(self):
        for event_index, field, value in ((4, "core_api", 5), (5, "core_api", 5), (5, "requested_route", 1),
                                          (5, "observation_api", 1), (5, "requested_route", True)):
            rows = self.version2(fixture())
            rows[event_index][field] = value
            self.assertFalse(analyze(rows)["core_render_reference_complete"])
        for change in (lambda native: native[:40], lambda native: native + [0]):
            rows = self.version2(fixture())
            rows[7]["state"] = change(rows[7]["state"])
            self.assertFalse(analyze(rows)["core_render_reference_complete"])

    def test_api2_unobserved_route_cannot_publish_callback_scalars_or_return(self):
        for field in (*range(25, 30), 31):
            rows = self.version2(missing_observation_fixture())
            for row in rows:
                if row.get("event") == "core_render_reference_sample":
                    row["state"][field] = 1
            result = analyze(rows)
            self.assertFalse(result["diagnostic_lifecycle_complete"])
            self.assertGreater(result["issue_count"], 0)
    def test_zero_callbacks_can_complete_diagnostic_lifecycle_without_native_observation(self):
        result = analyze(missing_observation_fixture())
        self.assertEqual(result["status"], "native_observation_missing")
        self.assertTrue(result["diagnostic_lifecycle_complete"], result)
        self.assertFalse(result["core_render_reference_complete"])
        self.assertFalse(result["native_renderer_callback_observed"])
        self.assertEqual(result["issue_count"], 0)
        self.assertFalse(result["lifecycle"]["flags"]["expected_async_result"])
        self.assertEqual(len(result["lifecycle"]["failures"]), 1)
        self.assertEqual(result["deliberate_incomplete_event"]["phase"], "core_render_reference")

    def test_only_exact_immediately_matching_deliberate_error_can_qualify_diagnostic(self):
        for field, value in (("error", "late failure"), ("phase", "other"), ("game_adapter_active", True)):
            rows = missing_observation_fixture()
            rows[-1][field] = value
            with self.subTest(field=field):
                self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        rows = missing_observation_fixture()
        rows[-1]["extra"] = False
        self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        rows = missing_observation_fixture()
        rows.insert(-1, '[INFO] unrelated late message')
        self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        rows = missing_observation_fixture()
        rows[-2]["passed"] = True
        self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        rows = missing_observation_fixture()
        rows.append(deepcopy(rows[-1]))
        self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        rows = missing_observation_fixture()
        rows.append('{"event":"adapter_failed",broken}')
        self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])

    def test_clean_diagnostic_still_requires_fulltime_worker_and_actual_end(self):
        for field, value in ((3, 9999), (4, 2), (5, 0), (6, 0), (7, 0), (8, 1), (9, 0),
                             (10, 1), (11, 0), (12, 0), (13, 1), (14, 0), (15, 0), (30, 1), (32, 1)):
            rows = missing_observation_fixture()
            rows[-3]["state"][field] = value
            with self.subTest(native_field=field):
                self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        for field, value in ((1, 0), (3, 33), (7, 0), (13, 0), (15, 0), (16, 15), (17, 0), (18, 126)):
            rows = missing_observation_fixture()
            rows[-2]["state"][field] = value
            with self.subTest(worker_field=field):
                self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])

    def test_rejected_foreign_nested_or_native_failure_preclude_clean_diagnostic(self):
        for field in (21, 23, 30, 33):
            rows = missing_observation_fixture()
            rows[-3]["state"][17] = 2
            rows[-3]["state"][field] = 1
            self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        rows = missing_observation_fixture()
        rows[-3]["state"][19] = 1
        self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        rows = missing_observation_fixture()
        rows[-3]["state"][35] = 1
        self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])

    def test_missing_observation_cannot_hide_restart_extra_failure_cancel_or_required_exit(self):
        for extra in (dict(event="adapter_failed", error="unrelated"), dict(event="cleanup_failed"),
                      dict(event="adapter_phase", phase="entry", mode="smoke")):
            rows = missing_observation_fixture()
            rows.append(extra)
            self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        rows = missing_observation_fixture()
        rows.insert(-3, dict(event="core_render_reference_stop_requested", at_ms=11000, reason="Ctrl+Shift+End"))
        rows[-2]["stop_requested"] = True
        self.assertFalse(analyze(rows)["diagnostic_lifecycle_complete"])
        self.assertFalse(analyze(missing_observation_fixture(), shutdown=False)["diagnostic_lifecycle_complete"])
        self.assertTrue(analyze(missing_observation_fixture(), shutdown=False, require_exit=False)["diagnostic_lifecycle_complete"])

    def test_observed_callback_and_native_false_return_do_not_claim_pixels_or_com(self):
        result = analyze()
        self.assertTrue(result["core_render_reference_complete"], result)
        self.assertTrue(result["diagnostic_lifecycle_complete"])
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
