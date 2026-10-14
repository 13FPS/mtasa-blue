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
    @classmethod
    def version5(cls, rows):
        cls.version4(rows)
        for row in rows:
            if row.get("event") in ("adapter_start", "core_render_reference_started"):
                row.update(core_api=9, observation_api=5)
            if row.get("event") == "core_render_reference_sample":
                native = row["state"]
                native[0] = 5
                native += [0] * 26
                if native[44] or native[45]:
                    native[84:92] = [2, 1, 255, 0, 1, 1, 0, 1]
                    native[92:94] = native[19:21]
                    native[104] = native[25]
                    native[108] = 9
        return rows

    def test_api5_coherent_present_and_association_remain_read_only(self):
        result = analyze(self.version5(fixture()))
        self.assertEqual(result["issue_count"], 0, result)
        self.assertEqual(result["observation_api"], 5)
        self.assertEqual(result["status"], "native_gpu_ownership_incomplete")
        self.assertTrue(result["diagnostic_lifecycle_complete"])
        self.assertTrue(result["native_renderer_callback_observed"])
        self.assertTrue(result["custom_presenter_recognized"])
        self.assertTrue(result["native_present_observed"])
        self.assertTrue(result["buffer_association_observed"])
        self.assertEqual(result["native_present_sample_count"], 2)
        self.assertEqual(result["buffer_association_sample_count"], 2)
        self.assertEqual(result["boundary_metrics"], dict(present_entries=603, rejected_entries=0, resize_epoch=0,
                                                         resize_entries=0, pre_release_entries=0, foreign_lifecycle_entries=0))
        for flag in ("gpu_ownership_verified", "gpu_frame_boundary_verified", "core_render_reference_complete",
                     "render_observation_session_complete", "visual_success_verified", "com_present_verified",
                     "de_renderer_verified", "full_mta_boot_verified", "engine_input_write_verified",
                     "remote_player_context_verified"):
            self.assertFalse(result[flag], flag)

    def test_api5_all110_fields_require_exact_unsigned_integers(self):
        for field in range(110):
            for value in (True, -1, .5, 0x100000000):
                with self.subTest(field=field, value=value):
                    rows = self.version5(fixture())
                    rows[6]["state"][field] = value
                    result = analyze(rows)
                    self.assertEqual(result["status"], "invalid_evidence")
                    self.assertFalse(result["native_present_observed"])
                    self.assertFalse(result["buffer_association_observed"])
                    self.assertEqual(result["native_present_sample_count"], 0)
                    self.assertEqual(result["buffer_association_sample_count"], 0)

    def test_api5_never_derives_new_claims_from_historical_protocols(self):
        for rows in (fixture(), self.version2(fixture()), self.version3(fixture()), self.version4(fixture())):
            result = analyze(rows)
            self.assertEqual(result["issue_count"], 0, result)
            self.assertFalse(result["native_present_observed"])
            self.assertFalse(result["buffer_association_observed"])
            self.assertEqual(result["native_present_sample_count"], 0)
            self.assertEqual(result["buffer_association_sample_count"], 0)
            self.assertIsNone(result["boundary_metrics"])
            self.assertFalse(result["gpu_frame_boundary_verified"])

    def test_api5_rejects_protocol_relabel_route_and_width_mismatches(self):
        for observation, core, route in ((4, 9, 2), (5, 8, 2), (5, 9, 1), (6, 10, 2)):
            rows = self.version5(fixture())
            rows[5].update(observation_api=observation, core_api=core, requested_route=route)
            self.assertEqual(analyze(rows)["status"], "invalid_evidence")
        for length in (40, 44, 80, 84, 109, 111):
            rows = self.version5(fixture())
            rows[7]["state"] = (rows[7]["state"] + [0])[:length]
            self.assertEqual(analyze(rows)["status"], "invalid_evidence")
        rows = self.version5(fixture())
        rows[7]["state"][0] = 4
        self.assertEqual(analyze(rows)["status"], "invalid_evidence")

    def test_api5_zero_callbacks_cannot_fabricate_boundary_tail(self):
        rows = self.version5(missing_observation_fixture())
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["diagnostic_lifecycle_complete"])
        self.assertFalse(result["native_present_observed"])
        self.assertEqual(result["native_present_sample_count"], 0)
        for field in range(84, 110):
            mutated = deepcopy(rows)
            mutated[7]["state"][field] = 1
            self.assertEqual(analyze(mutated)["status"], "invalid_evidence", field)

    def test_api5_present_association_does_not_depend_on_earlier_gpu_query_success(self):
        rows = self.version5(fixture())
        for row in rows[6:8]:
            row["state"][50:54] = [8, 0x80004002, 0, 0]
            row["state"][54:74] = [0] * 20
            row["state"][75] = 3
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["native_present_observed"])
        self.assertTrue(result["buffer_association_observed"])
        self.assertFalse(result["native_gpu_metadata_observed"])
        self.assertFalse(result["gpu_ownership_verified"])

    def test_api5_nested_forged_claims_cannot_replace_scalar_evidence(self):
        for claims in (dict(native_present_observed=True), dict(buffer_association_observed=True),
                       dict(native_present_sample_count=603), dict(buffer_association_sample_count=603),
                       dict(nativePresentObserved=True), dict(bufferAssociationObserved=True),
                       dict(gpu_frame_boundary_verified=True), dict(gpuOwnershipVerified=True),
                       dict(gpuFrameBoundaryVerified=True), dict(queueSubmissionVerified=True),
                       dict(customPresentFrameSinkVerified=True), dict(resource_pointer=0)):
            rows = self.version5(fixture())
            rows[4]["details"] = [dict(nested=claims)]
            result = analyze(rows)
            self.assertEqual(result["status"], "invalid_evidence", claims)
            self.assertFalse(result["native_present_observed"])
            self.assertFalse(result["buffer_association_observed"])
            self.assertFalse(result["gpu_ownership_verified"])

    def test_api5_rejects_contradictory_complete_association(self):
        for changes in ({84: 0}, {84: 11}, {85: 0}, {85: 2}, {86: 254}, {86: 256},
                        {87: 0x80004005}, {88: 0}, {89: 2}, {88: 16, 89: 16}, {90: 1}, {91: 0}, {91: 2},
                        {92: 0}, {92: 1, 94: 1}, {94: 604}, {95: 1}, {104: 0}, {104: 43},
                        {107: 1}, {107: 2}, {107: 4}, {108: 8}, {108: 10}, {109: 1}, {80: 3, 81: 0, 82: 0, 83: 0}):
            with self.subTest(changes=changes):
                rows = self.version5(fixture())
                for field, value in changes.items():
                    rows[7]["state"][field] = value
                result = analyze(rows)
                self.assertEqual(result["status"], "invalid_evidence", result)
                self.assertFalse(result["native_present_observed"])
                self.assertFalse(result["buffer_association_observed"])
                self.assertFalse(result["gpu_frame_boundary_verified"])

    def test_api5_sparse_coherent_sample_does_not_infer_unsampled_or_later_success(self):
        rows = self.version5(fixture())
        terminal = rows[7]["state"]
        terminal[84] = 5
        terminal[86] = 127
        terminal[94] = 600
        terminal[108:110] = [8, 0x80004003]
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["native_present_observed"])
        self.assertTrue(result["buffer_association_observed"])
        self.assertEqual(result["native_present_sample_count"], 1)
        self.assertEqual(result["buffer_association_sample_count"], 1)
        self.assertEqual(result["boundary_metrics"]["present_entries"], 603)
        self.assertEqual(result["boundary_metrics"]["rejected_entries"], 600)
        self.assertEqual(result["terminal_render_state"][84], 5)
        self.assertFalse(result["gpu_ownership_verified"])
        self.assertFalse(result["gpu_frame_boundary_verified"])

    def test_api5_entry_aggregate_without_coherent_sample_cannot_invent_observation(self):
        rows = self.version5(fixture())
        for row in rows[6:8]:
            native = row["state"]
            native[84] = 3
            native[86] = 25
            native[88:92] = [1, 1, 0, 0]
            native[94:96] = native[92:94]
            native[108:110] = [4, 1]
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["diagnostic_lifecycle_complete"])
        self.assertFalse(result["native_present_observed"])
        self.assertFalse(result["buffer_association_observed"])
        self.assertEqual(result["native_present_sample_count"], 0)
        self.assertEqual(result["buffer_association_sample_count"], 0)
        self.assertEqual(result["boundary_metrics"]["present_entries"], 603)

    def test_api5_64bit_present_counters_keep_exact_halves_across_carry(self):
        rows = self.version5(fixture())
        for row, low, high in ((rows[6], 0xffffffff, 0x200000), (rows[7], 0, 0x200001)):
            for field in (17, 19, 44, 48, 92):
                row["state"][field:field + 2] = [low, high]
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["native_present_observed"])
        self.assertEqual(result["boundary_metrics"]["present_entries"], 0x20000100000000)
        rows[7]["state"][92:94] = [0xfffffffe, 0x200000]
        self.assertEqual(analyze(rows)["status"], "invalid_evidence")

    def test_api5_supported_historical_prefix_cannot_promote_ownership_or_passed_flag(self):
        rows = self.version5(fixture())
        for row in rows[6:8]:
            native = row["state"]
            native[46:50] = [native[44], native[45], 0, 0]
            native[53] = 1
            native[74:76] = [0, 1]
            native[80:84] = [1, 0, 0, 0]
            native[84:110] = [10, 1] + [0] * 24
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["diagnostic_lifecycle_complete"])
        self.assertTrue(result["native_gpu_metadata_observed"])
        self.assertFalse(result["core_render_reference_complete"])
        self.assertFalse(result["gpu_ownership_verified"])
        self.assertFalse(result["gpu_frame_boundary_verified"])
        self.assertFalse(result["native_present_observed"])
        rows[-2]["passed"] = True
        rows.pop()
        forged = analyze(rows)
        self.assertEqual(forged["status"], "invalid_evidence")
        self.assertFalse(forged["gpu_ownership_verified"])
        self.assertFalse(forged["core_render_reference_complete"])

    def test_api5_initial_fatal_gpu_cleanup_can_precede_any_boundary_attempt(self):
        rows = self.version5(fixture())
        for row in rows[6:8]:
            row["state"][84:110] = [0] * 26
            row["state"][13] = 0
            row["state"][30] = 10
            row["state"][50:54] = [18, 0x8000ffff, 0, 0]
            row["state"][54:74] = [0] * 20
            row["state"][75] = 6
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertFalse(result["diagnostic_lifecycle_complete"])
        self.assertFalse(result["native_present_observed"])
        self.assertFalse(result["buffer_association_observed"])
        self.assertEqual(result["native_present_sample_count"], 0)
        for field, value in ((30, 0), (30, 9), (13, 1), (85, 1), (92, 1)):
            mutated = deepcopy(rows)
            mutated[6]["state"][field] = value
            self.assertEqual(analyze(mutated)["status"], "invalid_evidence", (field, value))

    def test_api5_later_fatal_cleanup_cannot_recount_preserved_coherent_boundary(self):
        rows = self.version5(fixture())
        terminal = rows[7]["state"]
        terminal[84:110] = rows[6]["state"][84:110]
        terminal[13] = 0
        terminal[30] = 10
        terminal[50:54] = [18, 0x8000ffff, 0, 0]
        terminal[54:74] = [0] * 20
        terminal[75] = 6
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertFalse(result["diagnostic_lifecycle_complete"])
        self.assertTrue(result["native_present_observed"])
        self.assertTrue(result["buffer_association_observed"])
        self.assertEqual(result["native_present_sample_count"], 1)
        self.assertEqual(result["buffer_association_sample_count"], 1)
        self.assertEqual(result["boundary_metrics"]["present_entries"], 3)
        self.assertFalse(result["gpu_ownership_verified"])
        for row in rows[6:8]:
            row["state"][13] = 0
            row["state"][30] = 10
        cached_only = analyze(rows)
        self.assertEqual(cached_only["issue_count"], 0, cached_only)
        self.assertFalse(cached_only["native_present_observed"])
        self.assertFalse(cached_only["buffer_association_observed"])
        self.assertEqual(cached_only["native_present_sample_count"], 0)

    def test_api5_noncoherent_statuses_preserve_actual_failure_stage_and_hresult(self):
        cases = (
            (1, 0, 0, 0, 0, 0, 0, 0, 0),
            (10, 1, 0, 0, 0, 0, 0, 0, 0),
            (4, 1, 0, 0, 0, 0, 0, 0, 43),
            (8, 1, 0, 0, 0, 0, 0, 0, 42),
            (5, 0, 0, 0, 1, 0x80004003, 0, 0, 0),
            (5, 1, 127, 0, 8, 0x80004003, 0, 1, 42),
            (5, 1, 255, 0, 9, 0x8000ffff, 0, 1, 42),
            (3, 1, 0, 0, 1, 1, 0, 0, 42),
            (3, 1, 9, 0, 2, 1, 0, 0, 42),
            (3, 1, 31, 0, 7, 1, 0, 0, 42),
            (3, 1, 63, 0, 7, 1, 3, 1, 42),
            (6, 1, 127, 0, 8, 1, 0, 1, 42),
            (7, 1, 0, 0, 0, 0, 0, 0, 42),
            (7, 1, 127, 0, 8, 1, 0, 1, 42),
            (7, 1, 127, 0, 9, 0, 0, 1, 42),
            (9, 1, 255, 0x887a0005, 9, 0, 0, 1, 42),
        )
        for status, profile, facts, present_hr, stage, query_hr, tracked, mode, thread in cases:
            with self.subTest(status=status, stage=stage, facts=facts):
                rows = self.version5(fixture())
                for row in rows[6:8]:
                    native = row["state"]
                    native[84:110] = [0] * 26
                    native[84:88] = [status, profile, facts, present_hr]
                    native[90:92] = [tracked, mode]
                    native[108:110] = [stage, query_hr]
                    if thread:
                        native[92:94] = native[94:96] = native[19:21]
                        native[104] = thread
                    if facts:
                        native[88:90] = [1, 1]
                    if status == 3 and stage == 2:
                        # Failed root/index comparisons preserve the raw native
                        # uint32 value without pretending it is a valid buffer.
                        native[88] = 0xffffffff
                result = analyze(rows)
                self.assertEqual(result["issue_count"], 0, result)
                self.assertTrue(result["diagnostic_lifecycle_complete"])
                self.assertFalse(result["native_present_observed"])
                self.assertFalse(result["buffer_association_observed"])
                self.assertEqual(result["native_present_sample_count"], 0)
                self.assertEqual(result["terminal_render_state"][108:110], [stage, query_hr])

    def test_api5_preentry_endpoint_change_cannot_invent_a_present(self):
        for cumulative in (0, 600):
            rows = self.version5(fixture())
            for row in rows[6:8]:
                native = row["state"]
                native[84:110] = [0] * 26
                native[84:86] = [8, 1]
                native[92] = native[94] = cumulative
            result = analyze(rows)
            self.assertEqual(result["issue_count"], 0, result)
            self.assertFalse(result["native_present_observed"])
            self.assertEqual(result["native_present_sample_count"], 0)
            self.assertFalse(result["buffer_association_observed"])
            rows[7]["state"][87] = 1
            self.assertEqual(analyze(rows)["status"], "invalid_evidence")

    def test_api5_all_lifecycle_counter_halves_are_monotonic_and_exact(self):
        rows = self.version5(fixture())
        for row, low, high in ((rows[6], 0xffffffff, 0x200000), (rows[7], 0, 0x200001)):
            native = row["state"]
            native[84] = 7
            native[86] = 127
            for field in (17, 19, 44, 48, *reader.BOUNDARY_PAIRS.values()):
                native[field:field + 2] = [low, high]
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertFalse(result["native_present_observed"])
        self.assertEqual(set(result["boundary_metrics"].values()), {0x20000100000000})
        for field in reader.BOUNDARY_PAIRS.values():
            mutated = deepcopy(rows)
            mutated[7]["state"][field:field + 2] = [0xfffffffe, 0x200000]
            self.assertEqual(analyze(mutated)["status"], "invalid_evidence", field)

    def test_api5_noncoherent_status_cannot_hide_contradictory_fields(self):
        for status, changes in ((1, {85: 1}), (1, {86: 1}), (1, {87: 1}), (1, {108: 1}), (1, {109: 1}),
                                (10, {85: 0}), (10, {104: 42}), (4, {86: 1}), (8, {108: 1}),
                                (5, {108: 0}), (5, {109: 0}), (5, {108: 9, 109: 0x80004003}),
                                (5, {108: 9, 109: 0x8000ffff, 92: 0}), (5, {108: 9, 109: 0x8000ffff, 104: 0}),
                                (5, {108: 9, 109: 0x8000ffff, 94: 0}), (5, {108: 9, 109: 0x8000ffff, 85: 0}),
                                (3, {108: 9}), (3, {109: 0x80004003}), (6, {108: 7}), (6, {109: 0}),
                                (7, {108: 7}), (7, {109: 1}), (7, {86: 255}), (9, {87: 0}), (9, {107: 1}),
                                (3, {90: 1, 91: 0}), (3, {90: 0x80000000, 91: 1})):
            with self.subTest(status=status, changes=changes):
                rows = self.version5(fixture())
                native = rows[7]["state"]
                native[84:110] = [0] * 26
                native[84] = status
                native[85] = int(status != 1)
                if status not in (1, 10):
                    native[92:94] = native[94:96] = native[19:21]
                    native[104] = native[25]
                if status in (3, 5, 6):
                    native[108:110] = [1, 0x80004003] if status == 5 else [8, 1] if status == 6 else [1, 1]
                if status == 6:
                    native[86] = 127
                if status in (7, 9):
                    native[86] = 127 if status == 7 else 255
                    native[90:92] = [0, 1]
                    native[108] = 9
                    if status == 9:
                        native[87] = 0x80004005
                self.assertTrue(reader.boundary_consistent(native), (status, native))
                for field, value in changes.items():
                    native[field] = value
                self.assertEqual(analyze(rows)["status"], "invalid_evidence")

    @classmethod
    def version4(cls, rows):
        cls.version3(rows)
        for row in rows:
            if row.get("event") in ("adapter_start", "core_render_reference_started"):
                row.update(core_api=8, observation_api=4)
            if row.get("event") == "core_render_reference_sample":
                native = row["state"]
                native[0] = 4
                native += [0] * 4
                if native[44] or native[45]:
                    native[46:50] = [0, 0, native[44], native[45]]
                    native[53] = 0
                    native[64] = native[71] = 24
                    native[74:76] = [1, 2]
                    native[80:84] = [2, 1, 127, 15]
            if row.get("event") == "core_render_reference_finished":
                row["passed"] = False
        if not any(row.get("event") == "adapter_failed" for row in rows):
            rows.append(dict(event="adapter_failed", phase="core_render_reference", error=reader.INCOMPLETE_ERROR,
                             game_adapter_active=False))
        return rows

    def test_api4_recognizes_static_code_without_promoting_gpu_or_drawing(self):
        result = analyze(self.version4(fixture()))
        self.assertEqual(result["issue_count"], 0, result)
        self.assertEqual(result["status"], "native_gpu_ownership_incomplete")
        self.assertTrue(result["diagnostic_lifecycle_complete"])
        self.assertTrue(result["native_renderer_callback_observed"])
        self.assertTrue(result["native_gpu_metadata_observed"])
        self.assertTrue(result["custom_presenter_recognized"])
        self.assertEqual(result["custom_presenter_sample_count"], 2)
        self.assertEqual(result["gpu_metrics"], dict(attempts=603, accepted=0, rejected=603))
        self.assertEqual(result["terminal_render_state"][78:], [0, 0, 2, 1, 127, 15])
        for flag in ("gpu_ownership_verified", "core_render_reference_complete", "render_observation_session_complete",
                     "visual_success_verified", "com_present_verified", "de_renderer_verified", "full_mta_boot_verified"):
            self.assertFalse(result[flag], flag)

    def test_api4_initial_missing_callbacks_keep_both_tails_zero(self):
        rows = self.version4(missing_observation_fixture())
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["diagnostic_lifecycle_complete"])
        self.assertFalse(result["custom_presenter_recognized"])
        self.assertEqual(result["custom_presenter_sample_count"], 0)
        for field in range(80, 84):
            mutated = deepcopy(rows)
            mutated[7]["state"][field] = 1
            self.assertEqual(analyze(mutated)["status"], "invalid_evidence", field)

    def test_api4_absent_presenter_preserves_independent_gpu_eligibility(self):
        rows = self.version4(fixture())
        for row in rows[6:8]:
            native = row["state"]
            native[46:50] = [native[44], native[45], 0, 0]
            native[53] = 1
            native[74:76] = [0, 1]
            native[80:84] = [1, 0, 0, 0]
        rows[-2]["passed"] = True
        rows.pop()
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["gpu_ownership_verified"])
        self.assertFalse(result["custom_presenter_recognized"])
        self.assertEqual(result["custom_presenter_sample_count"], 0)
        self.assertFalse(result["de_renderer_verified"])

    def test_api4_known_code_is_independent_of_com_failure(self):
        rows = self.version4(fixture())
        for row in rows[6:8]:
            row["state"][50:54] = [8, 0x80004002, 0, 0]
            row["state"][75] = 3
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["custom_presenter_recognized"])
        self.assertEqual(result["custom_presenter_sample_count"], 2)
        self.assertFalse(result["native_gpu_metadata_observed"])
        self.assertFalse(result["gpu_ownership_verified"])

    def test_api4_unknown_signature_device_read_and_root_failures_remain_diagnostics(self):
        for status, mask in ((3, 0), (4, 0), (4, 63), (5, 0), (5, 127), (6, 127), (7, 0)):
            with self.subTest(status=status, mask=mask):
                rows = self.version4(fixture())
                for row in rows[6:8]:
                    row["state"][80:84] = [status, 0, mask, 0]
                result = analyze(rows)
                self.assertEqual(result["issue_count"], 0, result)
                self.assertTrue(result["diagnostic_lifecycle_complete"])
                self.assertFalse(result["custom_presenter_recognized"])
                self.assertEqual(result["custom_presenter_sample_count"], 0)
                self.assertEqual(result["terminal_render_state"][80:], [status, 0, mask, 0])
                self.assertFalse(result["gpu_ownership_verified"])

    def test_api4_root_fault_can_precede_knowledge_of_custom_presence(self):
        for status in (5, 7):
            rows = self.version4(fixture())
            for row in rows[6:8]:
                row["state"][58] = 2
                row["state"][74] = 0
                row["state"][80:84] = [status, 0, 0, 0]
            result = analyze(rows)
            self.assertEqual(result["issue_count"], 0, result)
            self.assertTrue(result["diagnostic_lifecycle_complete"])
            self.assertFalse(result["custom_presenter_recognized"])

    def test_api4_classification_samples_do_not_override_later_unknown_code(self):
        rows = self.version4(fixture())
        rows[7]["state"][80:84] = [3, 0, 0, 0]
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["custom_presenter_recognized"])
        self.assertEqual(result["custom_presenter_sample_count"], 1)
        self.assertEqual(result["gpu_metrics"]["attempts"], 603)
        self.assertEqual(result["terminal_render_state"][80:], [3, 0, 0, 0])
        self.assertFalse(result["gpu_ownership_verified"])

    def test_api4_rejects_forged_or_contradictory_classification(self):
        for changes in ({80: 0}, {80: 8}, {81: 0}, {81: 2}, {82: 126}, {82: 128}, {83: 0}, {83: 16},
                        {78: 1}, {79: 1}, {80: 1, 81: 0, 82: 0, 83: 0},
                        {80: 3, 81: 0, 82: 127, 83: 0}, {80: 7, 81: 0, 82: 1, 83: 0},
                        {58: 2, 74: 0}, {58: 2, 74: 0, 80: 4, 81: 0, 82: 0, 83: 0},
                        {58: 2, 74: 0, 80: 6, 81: 0, 82: 0, 83: 0},
                        {80: 5, 81: 1, 82: 0, 83: 0}, {80: 6, 81: 0, 82: 127, 83: 15}):
            with self.subTest(changes=changes):
                rows = self.version4(fixture())
                for field, value in changes.items():
                    rows[7]["state"][field] = value
                result = analyze(rows)
                self.assertEqual(result["status"], "invalid_evidence", result)
                self.assertFalse(result["custom_presenter_recognized"])
                self.assertFalse(result["gpu_ownership_verified"])

    def test_api4_cannot_relabel_older_versions_or_mix_widths(self):
        for version, core_api, route in ((3, 8, 2), (4, 7, 2), (4, 8, 1)):
            rows = self.version4(fixture())
            rows[5].update(observation_api=version, core_api=core_api, requested_route=route)
            self.assertEqual(analyze(rows)["status"], "invalid_evidence")
        for width in (44, 80, 83, 85):
            rows = self.version4(fixture())
            rows[7]["state"] = (rows[7]["state"] + [0])[:width]
            self.assertEqual(analyze(rows)["status"], "invalid_evidence", width)

    def test_api4_derived_classification_and_native_pointer_claims_cannot_be_injected(self):
        for claims in (dict(custom_presenter_recognized=True), dict(custom_presenter_sample_count=2),
                       dict(vtable="0x12345678"), dict(custom_presenter="0x12345678"),
                       dict(gpu_frame_boundary_verified=True), dict(custom_present_frame_sink_verified=True),
                       dict(gpu_ownership_verified=True)):
            with self.subTest(claims=claims):
                rows = self.version4(fixture())
                rows[4]["details"] = [dict(nested=claims)]
                result = analyze(rows)
                self.assertEqual(result["status"], "invalid_evidence")
                self.assertFalse(result["custom_presenter_recognized"])

    def test_historical_api1_api2_api3_never_infer_custom_recognition(self):
        for rows in (fixture(), self.version2(fixture()), self.version3(fixture())):
            result = analyze(rows)
            self.assertEqual(result["issue_count"], 0, result)
            self.assertFalse(result["custom_presenter_recognized"])
            self.assertEqual(result["custom_presenter_sample_count"], 0)

    @classmethod
    def version3(cls, rows):
        cls.version2(rows)
        for row in rows:
            if row.get("event") in ("adapter_start", "core_render_reference_started"):
                row.update(core_api=7, observation_api=3)
            if row.get("event") == "core_render_reference_sample":
                native = row["state"]
                native[0] = 3
                native += [0] * 36
                if native[19] or native[20]:
                    values = {44: native[19], 45: native[20], 46: native[19], 47: native[20],
                              50: 19, 52: 1, 53: 1, 58: 1, 59: 123, 61: 7, 62: 1, 63: 3, 64: 87,
                              66: 1920, 67: 1080, 68: 1920, 70: 1080, 71: 87, 72: 1, 75: 1, 76: 1, 77: 1}
                    for field, value in values.items():
                        native[field] = value
        return rows

    def test_api3_metadata_ownership_does_not_claim_draw_queue_order_or_present(self):
        result = analyze(self.version3(fixture()))
        self.assertTrue(result["core_render_reference_complete"], result)
        self.assertTrue(result["gpu_ownership_verified"])
        self.assertTrue(result["native_renderer_callback_observed"])
        self.assertEqual(result["status"], "native_gpu_ownership_observed")
        self.assertEqual(result["gpu_metrics"], dict(attempts=603, accepted=603, rejected=0))
        self.assertTrue(result["native_gpu_metadata_observed"])
        self.assertEqual(result["gpu_metadata_sample_count"], 2)
        self.assertFalse(result["com_present_verified"])
        self.assertFalse(result["de_renderer_verified"])

    def test_api3_coherent_64bit_probe_counters_do_not_lose_precision(self):
        rows = self.version3(fixture())
        for row in rows[6:8]:
            for field in (17, 19, 44, 46):
                row["state"][field] = 0xffffffff
                row["state"][field + 1] = 0x200000
        self.assertTrue(analyze(rows)["gpu_ownership_verified"])

    def test_api3_rejects_success_with_contradictory_gpu_fields(self):
        for field, value in ((44, 2), (45, 1), (46, 2), (48, 1), (50, 0), (50, 20), (51, 1), (52, 2),
                             (53, 0), (54, 2), (57, 2), (58, 2), (61, 3), (62, 3), (63, 1), (63, 17), (63, 0xffffffff), (66, 0),
                             (68, 1919), (69, 1), (70, 1079), (71, 28), (72, 2), (73, 1), (74, 1),
                             (75, 0), (76, 2), (77, 2), (78, 1), (79, 1)):
            with self.subTest(field=field, value=value):
                rows = self.version3(fixture())
                rows[7]["state"][field] = value
                result = analyze(rows)
                self.assertFalse(result["gpu_ownership_verified"])
                self.assertEqual(result["status"], "invalid_evidence")

    def test_api3_rejects_matching_unknown_formats(self):
        rows = self.version3(fixture())
        rows[7]["state"][64] = rows[7]["state"][71] = 0
        self.assertFalse(analyze(rows)["gpu_ownership_verified"])

    def test_api3_missing_callbacks_cannot_fabricate_any_gpu_metadata(self):
        rows = self.version3(missing_observation_fixture())
        result = analyze(rows)
        self.assertTrue(result["diagnostic_lifecycle_complete"], result)
        self.assertFalse(result["gpu_ownership_verified"])
        self.assertFalse(result["native_gpu_metadata_observed"])
        self.assertEqual(result["gpu_metadata_sample_count"], 0)
        for field in range(44, 80):
            mutated = deepcopy(rows)
            mutated[7]["state"][field] = 1
            self.assertFalse(analyze(mutated)["diagnostic_lifecycle_complete"], field)

    def test_api3_failed_query_keeps_stage_hresult_and_clean_native_callback(self):
        for stage, status, valid, error in ((8, 3, 0, 0x80004002), (19, 2, 1, 0), (2, 2, 0, 0),
                                          (3, 2, 0, 0), (14, 4, 0, 0x8000ffff), (17, 5, 0, 0), (18, 6, 0, 0x8000ffff)):
            with self.subTest(stage=stage, status=status):
                rows = self.version3(fixture())
                for row in rows[6:8]:
                    native = row["state"]
                    native[46:50] = [0, 0, native[44], native[45]]
                    native[50:54] = [stage, error, valid, 0]
                    native[75] = status
                    if status == 2:
                        if stage == 2:
                            native[74] = 1
                        else:
                            native[76] = 2
                rows[-1]["passed"] = False
                rows.append(dict(event="adapter_failed", phase="core_render_reference", error=reader.INCOMPLETE_ERROR,
                                 game_adapter_active=False))
                result = analyze(rows)
                self.assertFalse(result["gpu_ownership_verified"])
                self.assertTrue(result["diagnostic_lifecycle_complete"], result)
                self.assertTrue(result["native_renderer_callback_observed"])
                self.assertEqual(result["status"], "native_gpu_ownership_incomplete")
                self.assertEqual(result["terminal_render_state"][50:54], [stage, error, valid, 0])
                self.assertEqual(result["native_gpu_metadata_observed"], stage == 19 and valid == 1)
                self.assertEqual(result["gpu_metadata_sample_count"], 2 if stage == 19 and valid == 1 else 0)

    def test_api3_custom_present_metadata_preserves_incomplete_ownership_and_historical_guard(self):
        # Queried metadata is useful evidence even though a custom presenter
        # cannot satisfy renderer admission. Earlier pre-COM guard logs must
        # retain their distinct stage and remain valid historical diagnostics.
        for fully_queried in (False, True):
            with self.subTest(fully_queried=fully_queried):
                rows = self.version3(fixture())
                for row in rows[6:8]:
                    native = row["state"]
                    native[46:50] = [0, 0, native[44], native[45]]
                    native[50:54] = [19 if fully_queried else 2, 0, int(fully_queried), 0]
                    native[74:78] = [1, 2, 1, 1]
                    if not fully_queried:
                        native[54:74] = [0] * 20
                rows[-1]["passed"] = False
                rows.append(dict(event="adapter_failed", phase="core_render_reference", error=reader.INCOMPLETE_ERROR,
                                 game_adapter_active=False))
                result = analyze(rows)
                self.assertEqual(result["issue_count"], 0, result)
                self.assertTrue(result["diagnostic_lifecycle_complete"], result)
                self.assertTrue(result["native_renderer_callback_observed"])
                self.assertFalse(result["cleanup_pending"])
                self.assertEqual(result["status"], "native_gpu_ownership_incomplete")
                self.assertEqual(result["gpu_metrics"], dict(attempts=603, accepted=0, rejected=603))
                self.assertEqual(result["terminal_render_state"][50:54], [19 if fully_queried else 2, 0, int(fully_queried), 0])
                self.assertEqual(result["terminal_render_state"][61], 7 if fully_queried else 0)
                self.assertEqual(result["terminal_render_state"][74:78], [1, 2, 1, 1])
                self.assertEqual(result["native_gpu_metadata_observed"], fully_queried)
                self.assertEqual(result["gpu_metadata_sample_count"], 2 if fully_queried else 0)
                for claim in ("gpu_ownership_verified", "core_render_reference_complete", "render_observation_session_complete",
                              "visual_success_verified", "com_present_verified", "de_renderer_verified", "full_mta_boot_verified",
                              "engine_input_write_verified", "remote_player_context_verified"):
                    self.assertFalse(result[claim], claim)

    def test_api3_metadata_count_describes_sampled_reports_without_overriding_later_query_failure(self):
        # A completed earlier query remains observed evidence, while its count
        # cannot stand in for all intervening probes or terminal GPU ownership.
        rows = self.version3(fixture())
        for row in rows[6:8]:
            native = row["state"]
            native[46:50] = [0, 0, native[44], native[45]]
            native[53] = 0
            native[74:78] = [1, 2, 1, 1]
        rows[7]["state"][50:54] = [8, 0x80004002, 0, 0]
        rows[7]["state"][54:74] = [0] * 20
        rows[7]["state"][75] = 3
        rows[-1]["passed"] = False
        rows.append(dict(event="adapter_failed", phase="core_render_reference", error=reader.INCOMPLETE_ERROR,
                         game_adapter_active=False))
        result = analyze(rows)
        self.assertEqual(result["issue_count"], 0, result)
        self.assertTrue(result["native_gpu_metadata_observed"])
        self.assertEqual(result["gpu_metadata_sample_count"], 1)
        self.assertEqual(result["sample_count"], 2)
        self.assertEqual(result["gpu_metrics"], dict(attempts=603, accepted=0, rejected=603))
        self.assertEqual(result["terminal_render_state"][50:54], [8, 0x80004002, 0, 0])
        self.assertFalse(result["gpu_ownership_verified"])
        self.assertTrue(result["diagnostic_lifecycle_complete"])

    def test_api3_cannot_relabel_api2_or_mix_protocols(self):
        for version, core_api, route in ((2, 7, 2), (3, 6, 2), (3, 7, 1)):
            rows = self.version3(fixture())
            rows[5].update(observation_api=version, core_api=core_api, requested_route=route)
            self.assertFalse(analyze(rows)["gpu_ownership_verified"])
        rows = self.version3(fixture())
        rows[7]["state"] = rows[7]["state"][:44]
        self.assertFalse(analyze(rows)["gpu_ownership_verified"])

    def test_nested_native_pointer_and_readiness_claims_never_certify_observation(self):
        for details in (dict(device="0x12345678"), dict(de_renderer_verified=True), dict(full_mta_boot_verified=True),
                        dict(native_gpu_metadata_observed=True),
                        dict(rendering_started=1), dict(network_active=True), dict(remote_player_context_verified=True)):
            with self.subTest(details=details):
                rows = self.version3(fixture())
                rows[4]["details"] = [dict(nested=details)]
                result = analyze(rows)
                self.assertFalse(result["gpu_ownership_verified"])
                self.assertFalse(result["native_gpu_metadata_observed"])
                self.assertEqual(result["status"], "invalid_evidence")
        rows = self.version3(fixture())
        rows[4]["full_mta_boot_verified"] = True
        self.assertFalse(analyze(rows)["gpu_ownership_verified"])

    def test_gpu_failure_classification_cannot_change_its_actual_stage(self):
        for stage, status, error in ((19, 3, 0), (4, 5, 0), (19, 6, 0), (8, 3, 0), (17, 4, 0x8000ffff),
                                     (2, 2, 0), (3, 2, 0), (19, 2, 0)):
            with self.subTest(stage=stage, status=status):
                rows = self.version3(fixture())
                for row in rows[6:8]:
                    native = row["state"]
                    native[46:50] = [0, 0, native[44], native[45]]
                    native[50:54] = [stage, error, int(stage == 19 and status == 2), 0]
                    native[75] = status
                rows[-1]["passed"] = False
                rows.append(dict(event="adapter_failed", phase="core_render_reference", error=reader.INCOMPLETE_ERROR,
                                 game_adapter_active=False))
                result = analyze(rows)
                self.assertFalse(result["diagnostic_lifecycle_complete"])
                self.assertEqual(result["status"], "invalid_evidence")

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
