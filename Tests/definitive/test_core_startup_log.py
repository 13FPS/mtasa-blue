"""Synthetic Core startup evidence checks; no DLL, game or process operations."""

from copy import deepcopy
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import core_startup_log as reader


def fixture(exit=True, before=0, after=0, elapsed=123):
    rows = [
        {"event": "adapter_phase", "phase": "entry", "mode": "smoke"},
        {"event": "adapter_armed", "mode": "smoke", "activate_key": "Ctrl+Shift+F9", "stop_key": "Ctrl+Shift+End",
         "bridge_loaded": False, "game_adapter_active": False, "network_active": False},
        {"event": "adapter_activation_requested", "mode": "smoke", "activate_key": "Ctrl+Shift+F9"},
        {"event": "adapter_activation_released", "mode": "smoke", "activate_key": "Ctrl+Shift+F9", "at_ms": 990,
         "release_wait_ms": 50, "bridge_loaded": False, "game_adapter_active": False, "network_active": False},
        {"event": "adapter_start", "mode": "smoke", "host": "sa_unreal", "cleo": "1.5.1", "host_version": "1.0", "definitions": "0.5",
         "core_startup_reference": True, "provenance": "mta_core_de_startup", "core_api": 2, "endpoint": None,
         "network_active": False, "input_writes": False, "remote_peds_created": False},
        {"event": "core_startup_reference_started", "at_ms": 1000, "core_api": 2},
        {"event": "core_startup_reference_finished", "at_ms": 1000 + elapsed, "elapsed_ms": elapsed,
         "run_result": 1, "state": [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, before, after, 1, 0, 0, 0, 1, 0],
         "passed": True, "cleanup_passed": True, "stop_requested": False, "full_core_ready": False, "rendering_started": False,
         "network_active": False, "input_writes": False, "remote_peds_created": False},
    ]
    if exit:
        rows += ['[INFO] New event received: "host shut down"', '[INFO] Exit']
    return rows


def text(rows):
    return "\n".join(row if isinstance(row, str) else "[INFO] " + json.dumps(row) for row in rows)


class CoreStartupLogTests(unittest.TestCase):
    def analyze(self, rows=None, require_exit=False):
        return reader.analyze_log(text(fixture() if rows is None else rows), require_exit)

    def test_success_is_limited_to_saved_read_only_startup(self):
        result = self.analyze(require_exit=True)
        self.assertTrue(result["core_startup_reference_complete"])
        self.assertTrue(result["normal_exit_verified"])
        self.assertEqual(result["state"]["phase"], 6)
        for claim in ("full_mta_boot_verified", "de_renderer_verified", "engine_input_write_verified", "remote_player_context_verified"):
            self.assertIs(result[claim], False)

    def test_existing_renderer_baseline_is_permitted_but_new_bits_are_not(self):
        for before in range(4):
            for after in range(4):
                with self.subTest(before=before, after=after):
                    self.assertEqual(self.analyze(fixture(before=before, after=after))["core_startup_reference_complete"], after & ~before == 0)

    def test_yielding_startup_has_no_invented_small_time_limit(self):
        self.assertTrue(self.analyze(fixture(elapsed=2**40))["core_startup_reference_complete"])

    def test_legacy_failed_snapshot_remains_readable_without_certifying_old_success(self):
        rows = fixture()
        rows[4]["core_api"] = rows[5]["core_api"] = 1
        rows[6].pop("stop_requested")
        rows[6]["state"].pop()
        self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])
        rows[6].update(run_result=0, passed=False, cleanup_passed=False)
        rows[6]["state"] = [0] * 8 + [0xffffffff] * 3 + [2, 2, 1, 0, 0, 4, 0]
        rows.insert(7, {"event": "adapter_failed", "error": "old synchronous load failure"})
        result = self.analyze(rows)
        self.assertEqual(result["status"], "adapter_failed")
        self.assertEqual(result["issue_count"], 0)
        self.assertEqual(result["state"]["error_reason"], 4)
        self.assertNotIn("win32_load_error", result["state"])
        self.assertFalse(result["core_startup_reference_complete"])

    def test_worker_load_error_is_preserved_as_failure(self):
        rows = fixture()
        rows[6].update(run_result=0, passed=False, cleanup_passed=False)
        rows[6]["state"] = [0] * 8 + [0xffffffff] * 3 + [2, 2, 1, 0, 0, 4, 0, 126]
        rows.insert(7, {"event": "adapter_failed", "error": "worker load failed"})
        result = self.analyze(rows)
        self.assertEqual(result["issue_count"], 0)
        self.assertEqual(result["state"]["win32_load_error"], 126)
        self.assertFalse(result["core_startup_reference_complete"])

    def test_cancelled_worker_cleanup_and_racing_success_are_never_success_evidence(self):
        for reason in ("Ctrl+Shift+End", "loading"):
            for raced_success in (False, True):
                with self.subTest(reason=reason, raced_success=raced_success):
                    rows = fixture()
                    rows[6].update(stop_requested=True, passed=False, run_result=int(raced_success))
                    if not raced_success:
                        rows[6]["state"][16:18] = [12, 0]
                    rows.insert(6, {"event": "core_startup_reference_stop_requested", "at_ms": 1050, "reason": reason})
                    rows.insert(8, {"event": "adapter_failed", "error": "cooperative cancellation"})
                    result = self.analyze(rows)
                    self.assertEqual(result["issue_count"], 0)
                    self.assertFalse(result["core_startup_reference_complete"])
                    self.assertTrue(result["normal_exit_verified"])

    def test_stop_request_schema_order_and_terminal_flag_are_strict(self):
        stop = {"event": "core_startup_reference_stop_requested", "at_ms": 1050, "reason": "loading"}
        for change in ({"at_ms": 999}, {"at_ms": 1124}, {"at_ms": True}, {"reason": "other"},
                       {"reason": []}, {"extra": 1}):
            rows = fixture(); rows[6].update(stop_requested=True, passed=False)
            rows.insert(6, {**stop, **change})
            with self.subTest(change=change):
                self.assertFalse(self.analyze(rows)["flags"]["valid_payloads"])
        rows = fixture(); rows[6].update(stop_requested=True, passed=False)
        rows.insert(6, stop); rows.insert(6, stop)
        self.assertFalse(self.analyze(rows)["flags"]["valid_payloads"])
        for at in (5, 7):
            rows = fixture(); rows[6].update(stop_requested=True, passed=False)
            rows.insert(at, stop)
            self.assertFalse(self.analyze(rows)["flags"]["valid_payloads"])
        rows = fixture(); rows[6]["passed"] = False; rows.insert(6, stop)
        self.assertFalse(self.analyze(rows)["flags"]["valid_payloads"])

    def test_worker_run_and_cleanup_flags_must_match_its_terminal_snapshot(self):
        for fields in ({"run_result": 0}, {"cleanup_passed": False}):
            rows = fixture(); rows[6].update(fields)
            self.assertFalse(self.analyze(rows)["flags"]["valid_payloads"])

    def test_quoted_cleo_messages(self):
        raw = "\n".join(row if isinstance(row, str) else "[INFO] " + json.dumps(json.dumps(row)) for row in fixture())
        self.assertTrue(reader.analyze_log(raw)["core_startup_reference_complete"])

    def test_incomplete_streams_and_missing_activation(self):
        for index in range(7):
            with self.subTest(index=index):
                rows = fixture(); del rows[index]
                self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_every_uint32_field_is_typed_and_required(self):
        for index in range(19):
            for value in (-1, 2**32, True, 1.0, None):
                with self.subTest(index=index, value=value):
                    rows = fixture(); rows[6]["state"][index] = value
                    self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])
        for state in (None, {}, [], fixture()[6]["state"][:-1], fixture()[6]["state"] + [0]):
            rows = fixture(); rows[6]["state"] = state
            self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_every_gate_and_cleanup_field_is_checked(self):
        for index in (*range(11), *range(13, 19)):
            with self.subTest(index=index):
                rows = fixture(); rows[6]["state"][index] += 1
                self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])
        for index in (11, 12):
            rows = fixture(); rows[6]["state"][index] = 4
            self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_adapter_contract_and_activity_flags(self):
        for key, value in (("core_startup_reference", False), ("provenance", "other"), ("core_api", True),
                           ("core_api", 1), ("core_api", 3), ("mode", "online"), ("endpoint", "localhost"),
                           ("network_active", True), ("input_writes", True), ("remote_peds_created", True),
                           ("native_movement_trial", True)):
            with self.subTest(key=key):
                rows = fixture(); rows[4][key] = value
                self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_terminal_result_flags_are_strict(self):
        for key, value in (("run_result", 0), ("run_result", True), ("passed", False), ("cleanup_passed", False),
                           ("full_core_ready", True), ("rendering_started", True), ("input_writes", True),
                           ("network_active", True), ("remote_peds_created", True), ("stop_requested", True),
                           ("stop_requested", 0)):
            with self.subTest(key=key):
                rows = fixture(); rows[6][key] = value
                self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_clock_types_origin_and_order(self):
        for row, key, value in ((5, "at_ms", True), (5, "at_ms", 989), (5, "core_api", True),
                                (6, "at_ms", -1), (6, "at_ms", 2**53), (6, "elapsed_ms", -1),
                                (6, "elapsed_ms", True), (6, "elapsed_ms", 124), (3, "release_wait_ms", 10000)):
            with self.subTest(row=row, key=key):
                rows = fixture(); rows[row][key] = value
                self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_activation_order_and_chord(self):
        rows = fixture(); rows[1], rows[2] = rows[2], rows[1]
        self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])
        for row in (1, 2, 3):
            rows = fixture(); rows[row]["activate_key"] = "Ctrl+Shift+Home"
            self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_duplicate_attempt_or_later_failure_cannot_hide_behind_success(self):
        for extra in (fixture()[0], fixture()[4], fixture()[5], fixture()[6],
                      {"event": "adapter_failed", "error": "later failure"}, {"event": "cleanup_failed"}):
            rows = fixture(); rows.insert(7, deepcopy(extra))
            self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_player_movement_network_and_unexpected_payloads_are_refused(self):
        for extra in ({"event": "native_movement_trial_sample"}, {"event": "connected"}, {"event": "unrelated", "ped": 1},
                      {"event": "core_startup_reference_surprise"}, {"event": "unrelated", "input_writes": True}):
            rows = fixture(); rows.insert(7, extra)
            self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])
        rows = fixture(); rows[6]["position"] = {"x": 0, "y": 0, "z": 0}
        self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])
        for value in (0, 1, "false", None):
            rows = fixture(); rows.insert(7, {"event": "unrelated", "input_writes": value})
            self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_malformed_and_duplicate_json_markers_are_not_skipped(self):
        for marker in ('{"event":"core_startup_reference_finished",broken',
                       '{"event":"core_startup_reference_finished","event":"adapter_phase"}',
                       '{"event":"core_startup_reference_finished","state":NaN}'):
            rows = fixture(); rows.insert(7, "[INFO] " + marker)
            self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_event_and_input_byte_limits(self):
        rows = fixture(); rows[7:7] = [{"event": "unrelated"}] * reader.MAX_EVENTS
        self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])
        with self.assertRaises(ValueError):
            reader.analyze_log("x" * (reader.MAX_LOG_BYTES + 1))

    def test_exit_is_optional_and_requires_post_cleanup_order(self):
        self.assertTrue(self.analyze(fixture(False))["core_startup_reference_complete"])
        self.assertFalse(self.analyze(fixture(False), True)["core_startup_reference_complete"])
        rows = fixture(); rows[6], rows[7] = rows[7], rows[6]
        self.assertFalse(self.analyze(rows)["core_startup_reference_complete"])

    def test_cli_preserves_evidence_fingerprint_and_refuses_same_file(self):
        with tempfile.TemporaryDirectory() as directory:
            log, output = Path(directory) / "input.log", Path(directory) / "assessment.json"
            original = text(fixture()).encode()
            log.write_bytes(original)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(reader.main(["--log", str(log), "--output", str(output), "--require-exit"]), 0)
            result = json.loads(output.read_text())
            self.assertEqual(log.read_bytes(), original)
            self.assertEqual(result["log_fingerprint"]["size_bytes"], len(original))
            self.assertEqual(result["evidence_scope"], "entire_log")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(reader.main(["--log", str(log), "--output", str(log)]), 2)
            self.assertEqual(log.read_bytes(), original)

    def test_cli_records_invalid_nonfinite_payload_without_overclaiming(self):
        with tempfile.TemporaryDirectory() as directory:
            log, output = Path(directory) / "input.log", Path(directory) / "assessment.json"
            rows = fixture(); rows[6]["state"][0] = float("inf")
            log.write_text(text(rows))
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(reader.main(["--log", str(log), "--output", str(output)]), 2)
            self.assertFalse(json.loads(output.read_text())["core_startup_reference_complete"])


if __name__ == "__main__":
    unittest.main()
