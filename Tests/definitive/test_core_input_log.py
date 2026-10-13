"""Adversarial saved-evidence checks for the Core configured-input reference."""

from copy import deepcopy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import core_input_log as reader


def fixture():
    rows = [
        dict(event="adapter_phase", phase="entry", mode="smoke"),
        dict(event="adapter_armed", mode="smoke", activate_key="Ctrl+Shift+F9", stop_key="Ctrl+Shift+End",
             bridge_loaded=False, game_adapter_active=False, network_active=False),
        dict(event="adapter_activation_requested", mode="smoke", activate_key="Ctrl+Shift+F9"),
        dict(event="adapter_activation_released", mode="smoke", activate_key="Ctrl+Shift+F9", at_ms=990,
             release_wait_ms=50, bridge_loaded=False, game_adapter_active=False, network_active=False),
        dict(event="adapter_start", mode="smoke", core_input_reference=True, provenance=reader.PROVENANCE,
             core_api=3, endpoint=None, network_active=False, input_writes=False, remote_peds_created=False),
        dict(event="core_input_reference_started", at_ms=1000, core_api=3, sample_interval_ms=100,
             budget_ms=60000, context=reader.CONTEXT.copy(), context_source="assumed_on_foot"),
    ]
    previous = [0] * 24
    for sequence, names in enumerate(((), ("forwards",), ("backwards",), ("left",), ("right",),
                                      ("forwards", "sprint"), ("forwards", "walk"), ()), 1):
        current = [0] * 24
        for name, index, value in (("forwards", 1, -128), ("backwards", 1, 128), ("left", 0, -128),
                                    ("right", 0, 128), ("sprint", 16, 255), ("walk", 21, 255)):
            if name in names:
                current[index] = value
        elapsed = 60000 if sequence == 8 else (sequence - 1) * 1000
        state = [1, 2 if sequence == 8 else 1, sequence, elapsed, 7, 6, 0, 6, 0, 0, 1, sequence,
                 sum(1 << reader.ACTIONS[name] for name in names), 0, 1 if sequence == 8 else 0,
                 int(sequence == 8)]
        state += [value & 0xffffffff for value in current + ([0] * 24 if sequence == 8 else previous)]
        rows.append(dict(event="core_input_reference_sample", at_ms=1000 + elapsed, elapsed_ms=elapsed, state=state))
        previous = current
    rows.append(dict(event="core_input_reference_finished", at_ms=61100, elapsed_ms=60100,
                     state=[1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 2, 2, 1, 0, 0, 0, 1, 0],
                     run_result=1, passed=True, cleanup_passed=True, stop_requested=False, samples=8,
                     full_core_ready=False, rendering_started=False, network_active=False,
                     input_writes=False, remote_peds_created=False))
    return rows


def analyze(rows, shutdown=True, require_exit=True):
    text = "\n".join(json.dumps(row) for row in rows)
    if shutdown:
        text += '\n[INFO] New event received: "host shut down"\n[INFO] Exit\n'
    return reader.analyze_log(text, require_exit)


class CoreInputEvidenceTests(unittest.TestCase):
    def test_partial_renderer_markers_cannot_accompany_an_earlier_input_success(self):
        rows = fixture()
        rows.append(dict(event="core_render_reference_sample", state=[0] * 40))
        self.assertFalse(analyze(rows)["core_input_reference_complete"])

    def test_valid_read_only_trace_keeps_scope_narrow(self):
        result = analyze(fixture())
        self.assertTrue(result["core_input_reference_complete"], result)
        self.assertTrue(result["configured_input_session_complete"])
        self.assertTrue(result["normal_exit_verified"])
        for flag in ("full_mta_boot_verified", "de_renderer_verified", "engine_input_write_verified", "remote_player_context_verified"):
            self.assertIs(result[flag], False)

    def test_passed_cleanup_cannot_substitute_for_human_coverage(self):
        rows = fixture()
        for row in rows[6:14]:
            row["state"][12] = 0
            row["state"][16:] = [0] * 48
        result = analyze(rows)
        self.assertTrue(result["configured_input_session_complete"])
        self.assertFalse(result["core_input_reference_complete"])
        self.assertEqual(result["status"], "movement_coverage_missing")

    def test_every_sample_field_requires_exact_unsigned_types(self):
        for field in range(64):
            for value in (-1, True, 0.5, 0x100000000):
                with self.subTest(field=field, value=value):
                    rows = fixture()
                    rows[7]["state"][field] = value
                    self.assertFalse(analyze(rows)["core_input_reference_complete"])

    def test_sample_bounds_context_sequence_and_cleanup_cannot_be_forged(self):
        for field, value in ((0, 2), (1, 3), (2, 0), (3, 62001), (4, 6), (5, 0), (7, 0), (8, 1),
                             (12, 1), (13, 1), (14, 1), (15, 1), (16, 127), (18, 1), (32, 128), (37, 128)):
            with self.subTest(field=field, value=value):
                rows = fixture(); rows[7]["state"][field] = value
                self.assertFalse(analyze(rows)["core_input_reference_complete"])
        for field, value in ((1, 1), (12, 16), (14, 2), (15, 0), (16, 128)):
            rows = fixture(); rows[13]["state"][field] = value
            self.assertFalse(analyze(rows)["core_input_reference_complete"])

    def test_focus_suppression_and_consecutive_previous_frames(self):
        rows = fixture(); rows[7]["state"][4] = 4
        self.assertFalse(analyze(rows)["core_input_reference_complete"])
        rows = fixture(); rows[8]["state"][41] = 0
        self.assertFalse(analyze(rows)["core_input_reference_complete"])
        rows = fixture(); rows[8]["state"][2] = rows[7]["state"][2]
        self.assertFalse(analyze(rows)["core_input_reference_complete"])

    def test_samples_may_skip_worker_frames(self):
        rows = fixture()
        for row in rows[7:14]:
            row["state"][2] *= 2
        self.assertTrue(analyze(rows)["core_input_reference_complete"])

    def test_terminal_clears_both_frames_even_after_a_held_key(self):
        rows = fixture()
        rows[12]["state"][12] = 1 << reader.ACTIONS["forwards"]
        rows[12]["state"][17] = 0xffffff80
        rows[12]["state"][37] = 0
        # This lifecycle variant omits walk coverage but must still prove cleanup.
        self.assertTrue(analyze(rows)["configured_input_session_complete"])
        rows[13]["state"][41] = 0xffffff80
        self.assertFalse(analyze(rows)["core_input_reference_complete"])
        rows = fixture(); rows[13]["state"][3] = 100
        self.assertFalse(analyze(rows)["core_input_reference_complete"])

    def test_wrong_schema_flags_or_missing_final_are_rejected(self):
        for index, change in ((4, {"core_api": 2}), (4, {"input_writes": True}),
                              (5, {"context_source": "verified"}), (5, {"context": {}}),
                              (5, {"budget_ms": True}), (6, {"extra": 1}),
                              (14, {"samples": 7}), (14, {"full_core_ready": True}),
                              (14, {"at_ms": 60000, "elapsed_ms": 59000})):
            rows = fixture(); rows[index].update(change)
            self.assertFalse(analyze(rows)["core_input_reference_complete"])
        rows = fixture(); rows.pop(13)
        self.assertFalse(analyze(rows)["core_input_reference_complete"])
        rows = fixture(); rows.insert(14, deepcopy(rows[6]))
        self.assertFalse(analyze(rows)["core_input_reference_complete"])

    def test_restart_cancel_failure_and_exit_rules_survive_lifecycle_reuse(self):
        rows = fixture(); rows += deepcopy(rows)
        self.assertFalse(analyze(rows)["core_input_reference_complete"])
        rows = fixture(); rows.insert(7, dict(event="core_input_reference_stop_requested", at_ms=1200, reason="Ctrl+Shift+End"))
        self.assertFalse(analyze(rows)["core_input_reference_complete"])
        rows = fixture(); rows.append(dict(event="adapter_failed", error="late failure"))
        self.assertFalse(analyze(rows)["core_input_reference_complete"])
        self.assertFalse(analyze(fixture(), shutdown=False)["core_input_reference_complete"])
        self.assertTrue(analyze(fixture(), shutdown=False, require_exit=False)["core_input_reference_complete"])

    def test_malformed_markers_are_never_skipped(self):
        rows = fixture()
        text = "\n".join(json.dumps(row) for row in rows)
        for raw in ('{"event":"core_input_reference_sample",bad}',
                    '{"event":"core_input_reference_sample","event":"core_input_reference_sample"}'):
            self.assertFalse(reader.analyze_log(text + "\n" + raw)["core_input_reference_complete"])


if __name__ == "__main__":
    unittest.main()
