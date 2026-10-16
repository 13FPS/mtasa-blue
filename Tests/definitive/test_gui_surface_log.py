"""Saved Surface96 policy, attribution, cleanup and composition proof checks."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import gui_surface_log as reader


def passed_values():
    values = [0] * 96
    assignments = {0: 1, 1: 2, 2: 3, 3: 8500, 4: 1, 6: 1, 8: 1, 9: 1, 10: 1, 11: 1,
                   12: 1, 14: 601, 16: 601, 18: 2, 19: 1, 20: 1, 21: 1, 24: 1, 25: 77,
                   27: 87, 28: 640, 29: 480, 30: 1, 31: 1, 32: 1, 33: 4, 35: 4, 37: 1,
                   38: 20001, 39: 1, 40: 2, 46: 4, 47: 9, 53: 1, 54: 1, 55: 1, 56: 1, 57: 1,
                   58: 1, 60: 1, 61: 1, 62: 1, 63: 1, 64: 1, 65: 1, 66: 1, 67: 3, 68: 3,
                   70: 3, 74: 1, 78: 1, 79: 1, 80: 1, 81: 42, 82: 43, 83: 1, 84: 4, 85: 8000, 86: 1}
    for field, value in assignments.items():
        values[field] = value
    return values


def active_readback_values():
    values = passed_values()
    for field, value in {1: 1, 2: 2, 3: 300, 4: 0, 5: 1, 6: 0, 20: 0, 21: 0, 39: 0,
                         47: 6, 49: 1, 50: 1, 51: 1, 74: 0, 75: 1, 76: 1, 77: 1,
                         78: 0, 79: 0, 80: 0}.items():
        values[field] = value
    return values


def encoded(values=None, pid=71, run_ms=100, **extra):
    record = dict(schema=reader.SCHEMA, pid=pid, runMs=run_ms,
                  values=passed_values() if values is None else values)
    record.update(extra)
    return (json.dumps(record) + "\n").encode()


def assess(data=None, **kwargs):
    return reader.assess(encoded() if data is None else data, expected_pid=kwargs.pop("expected_pid", 71),
                         baseline=kwargs.pop("baseline", b""), **kwargs)


class GuiSurfaceLogTests(unittest.TestCase):
    def test_private_surface_positive_has_no_native_or_screen_claim(self):
        result = assess()
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertTrue(result["own_surface_workload_verified"])
        self.assertTrue(result["composition_present_verified"])
        for claim in ("visible_gta_drawing_verified", "full_mta_ready", "native_queue_commands_issued",
                      "native_backbuffer_touched", "bridge_worker_lifecycle_verified"):
            self.assertFalse(result[claim])
        self.assertEqual(result["reason"], "Completed")

    def test_active_verified_readback_precedes_display_completion(self):
        values = active_readback_values()
        self.assertEqual(values[37], 1)
        self.assertEqual(values[20:22], [0, 0])
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertFalse(result["cleanup_complete"])
        self.assertFalse(result["own_surface_workload_verified"])
        self.assertFalse(result["composition_present_verified"])
        self.assertFalse(result["visible_gta_drawing_verified"])

    def test_cancelled_terminal_keeps_readback_without_completed_display(self):
        values = active_readback_values()
        for field, value in {1: 2, 2: 3, 3: 392, 4: 2, 5: 0, 6: 1, 39: 1, 47: 9,
                             49: 0, 50: 0, 51: 0, 74: 1, 75: 0, 76: 0, 77: 0,
                             78: 1, 79: 1, 80: 1}.items():
            values[field] = value
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertEqual(result["reason"], "Cancelled")
        self.assertFalse(result["own_surface_workload_verified"])
        self.assertFalse(result["composition_present_verified"])
        self.assertFalse(result["visible_gta_drawing_verified"])

    def test_readback_requires_started_owned_render_resources(self):
        for field in (19, 30, 31, 32):
            with self.subTest(missing_field=field):
                values = active_readback_values()
                values[field] = 0
                self.assertFalse(assess(encoded(values))["diagnostic_valid"])

    def test_completed_proof_still_requires_workload_flags_and_eight_seconds(self):
        for updates in ({20: 0, 21: 0}, {21: 0}, {3: 7999}):
            with self.subTest(updates=updates):
                values = passed_values()
                for field, value in updates.items():
                    values[field] = value
                result = assess(encoded(values))
                self.assertFalse(result["own_surface_workload_verified"])
                self.assertFalse(result["composition_present_verified"])
        self.assertTrue(assess()["own_surface_workload_verified"])

    def test_exact_pid_and_explicit_baseline_required(self):
        with self.assertRaises(TypeError):
            reader.assess(encoded(), expected_pid=71)
        for pid in (None, True, 0, -1, 1 << 32, 1.0):
            with self.subTest(pid=pid), self.assertRaises(ValueError):
                assess(expected_pid=pid)
        for baseline in (None, "", bytearray()):
            with self.assertRaises(ValueError):
                assess(baseline=baseline)

    def test_exact_fresh_prefix_and_hash(self):
        old = encoded(pid=55, run_ms=90)
        result = assess(old + encoded(), baseline=old, baseline_sha256=hashlib.sha256(old).hexdigest())
        self.assertTrue(result["own_surface_workload_verified"])
        self.assertEqual(result["fresh_matching_records"], 1)
        self.assertFalse(assess(encoded(), baseline=encoded())["diagnostic_valid"])
        self.assertFalse(assess(encoded(), baseline=old)["diagnostic_valid"])
        self.assertFalse(assess(old + encoded(), baseline=old, baseline_sha256="0" * 64)["diagnostic_valid"])

    def test_partial_baseline_and_bad_hash_rejected(self):
        old = encoded()[:-1]
        self.assertFalse(assess(old + b"\n" + encoded(run_ms=101), baseline=old)["diagnostic_valid"])
        for digest in ("A" * 64, "g" * 64, "a", 7):
            with self.assertRaises(ValueError):
                assess(baseline_sha256=digest)

    def test_same_pid_run_cannot_inherit_prior_success(self):
        old = encoded()
        self.assertFalse(assess(old + encoded(), baseline=old)["diagnostic_valid"])
        self.assertTrue(assess(old + encoded(run_ms=101), baseline=old)["own_surface_workload_verified"])

    def test_run_pid_duplicates_and_ambiguity(self):
        self.assertFalse(assess(encoded(pid=72))["diagnostic_valid"])
        self.assertFalse(assess(expected_run_ms=101)["diagnostic_valid"])
        self.assertTrue(assess(expected_run_ms=100)["diagnostic_valid"])
        for run in (True, -1, 1 << 64, 1.0):
            with self.assertRaises(ValueError):
                assess(expected_run_ms=run)
        for data in (encoded() * 2, encoded() + encoded(run_ms=101)):
            self.assertFalse(assess(data)["diagnostic_valid"])
            self.assertFalse(assess(data, expected_run_ms=100)["diagnostic_valid"])
        self.assertTrue(assess(encoded(pid=72) + encoded())["own_surface_workload_verified"])

    def test_json_framing_schema_and_duplicate_keys(self):
        text = encoded().decode()
        invalid = (b"{\n", encoded()[:-1], b"\xff\n", b"null\n", b"[]\n", encoded() + b"broken\n",
                   text.replace('"pid": 71', '"pid": 71, "pid": 71').encode())
        for data in invalid:
            with self.subTest(data=data):
                self.assertFalse(assess(data)["diagnostic_valid"])
        for extra in ({"schema": "mta_core_gui_startup_v1"}, {"pid": True}, {"runMs": True},
                      {"passed": True}, {"hwnd": 123}, {"own_surface_workload_verified": True}):
            self.assertFalse(assess(encoded(**extra))["diagnostic_valid"])
        old = b"old malformed unrelated line\n"
        self.assertTrue(assess(old + encoded(), baseline=old)["diagnostic_valid"])
        self.assertFalse(assess(old + encoded() + b"broken\n", baseline=old)["diagnostic_valid"])

    def test_every_field_is_uint32_not_boolean(self):
        for invalid in (True, False, -1, 1 << 32, 1.0, None, "1", [], {}):
            for field in (0, 3, 25, 38, 67, 68, 81, 95):
                with self.subTest(field=field, invalid=invalid):
                    values = passed_values(); values[field] = invalid
                    self.assertFalse(assess(encoded(values))["diagnostic_valid"])
        for length in (0, 64, 95, 97):
            self.assertFalse(assess(encoded([0] * length))["diagnostic_valid"])

    def test_enums_booleans_and_all_reserved_tail(self):
        for field, value in ((0, 2), (1, 0), (1, 4), (2, 0), (4, 12), (18, 1), (23, 7), (41, 5),
                             (42, 11), (47, 10), (48, 10)):
            values = passed_values(); values[field] = value
            self.assertFalse(assess(encoded(values))["diagnostic_valid"], field)
        for field in reader.BOOLEAN_FIELDS:
            values = passed_values(); values[field] = 2
            self.assertFalse(assess(encoded(values))["diagnostic_valid"], field)
        for field in range(88, 96):
            values = passed_values(); values[field] = 1
            self.assertFalse(assess(encoded(values))["diagnostic_valid"], field)

    def test_present_success_is_a_count_not_a_boolean(self):
        values = passed_values(); values[67] = values[68] = values[70] = 200
        self.assertTrue(assess(encoded(values))["own_surface_workload_verified"])
        self.assertNotIn(68, reader.BOOLEAN_FIELDS)

    def test_busy_present_retries_are_counted_without_extra_gui_frame(self):
        values = passed_values(); values[67] = 200; values[68] = 3; values[87] = 197; values[70] = 3
        self.assertTrue(assess(encoded(values))["own_surface_workload_verified"])
        for field, value in ((67, 199), (68, 1), (87, 196), (69, 0x887a000a), (70, 2)):
            changed = list(values); changed[field] = value
            self.assertFalse(assess(encoded(changed))["own_surface_workload_verified"], field)
        values[67] = values[68] = 0xffffffff; values[87] = 1
        self.assertFalse(assess(encoded(values))["own_surface_workload_verified"])
        self.assertNotIn(87, reader.BOOLEAN_FIELDS)

    def test_no_native_write_bits_or_retained_cleanup(self):
        for field in (44, 45, 5, 43, 49, 50, 51, 59, 72, 75, 76, 77):
            values = passed_values(); values[field] = 1
            self.assertFalse(assess(encoded(values))["diagnostic_valid"], field)
        for field in (9, 39, 78, 79, 80):
            values = passed_values(); values[field] = 0
            self.assertFalse(assess(encoded(values))["diagnostic_valid"], field)

    def test_feature_failure_may_still_cleanup(self):
        values = passed_values(); values[4] = 7; values[21] = values[37] = 0; values[22] = 0x80004005
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["own_surface_workload_verified"])
        self.assertFalse(result["composition_present_verified"])

    def test_early_refusal_cleanup_requires_no_created_window(self):
        for reason in (2, 3, 4, 6, 8, 9, 11):
            values = [0] * 96
            for field, value in {0: 1, 1: 2, 2: 1, 4: reason, 6: 1, 9: 1, 39: 1, 40: 2, 78: 1, 79: 1, 80: 1}.items():
                values[field] = value
            result = assess(encoded(values))
            self.assertTrue(result["diagnostic_valid"], reason)
            self.assertTrue(result["cleanup_complete"], reason)
            self.assertFalse(result["own_surface_workload_verified"], reason)

    def test_pending_retained_or_release_fault_never_passes(self):
        values = passed_values(); values[1] = 3; values[4] = 10; values[5] = 1; values[6] = 0
        values[39] = 0; values[49] = values[75] = 1
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertFalse(result["cleanup_complete"])
        values[59] = 1
        self.assertTrue(assess(encoded(values))["diagnostic_valid"])
        self.assertFalse(assess(encoded(values))["own_surface_workload_verified"])

    def test_fence_pair_completion_and_device_removal(self):
        for field, value in ((33, 0), (35, 3), (37, 0), (38, 0)):
            values = passed_values(); values[field] = value
            self.assertFalse(assess(encoded(values))["own_surface_workload_verified"], field)
        values = passed_values(); values[33] = 0xffffffff; values[34] = 1; values[35] = 0; values[36] = 2
        self.assertTrue(assess(encoded(values))["own_surface_workload_verified"])
        values[35] = values[36] = 0xffffffff
        self.assertFalse(assess(encoded(values))["diagnostic_valid"])
        values[52] = 1
        self.assertTrue(assess(encoded(values))["diagnostic_valid"])
        self.assertFalse(assess(encoded(values))["own_surface_workload_verified"])

    def test_missing_lease_metadata_invalid_or_unproved(self):
        for field in (12, 14, 16, 18, 58):
            values = passed_values(); values[field] = 0
            self.assertFalse(assess(encoded(values))["own_surface_workload_verified"], field)
        values = passed_values(); values[14] = 1; values[16] = 100
        self.assertTrue(assess(encoded(values))["own_surface_workload_verified"])

    def test_each_required_surface_fact_gates_success(self):
        for field in (8, 10, 11, 19, 20, 21, 30, 31, 32, 37, 53, 54, 55, 56, 58,
                      60, 61, 62, 63, 64, 65, 66, 78, 79, 80, 81, 82, 84, 86):
            values = passed_values(); values[field] = 0
            self.assertFalse(assess(encoded(values))["own_surface_workload_verified"], field)

    def test_present_count_frame_count_and_hresult(self):
        for field, value in ((67, 1), (68, 2), (69, 1), (70, 2), (22, 1), (23, 1)):
            values = passed_values(); values[field] = value
            self.assertFalse(assess(encoded(values))["own_surface_workload_verified"], field)

    def test_display_size_format_alpha_and_duration(self):
        for field, value in ((3, 7999), (24, 0), (27, 24), (28, 0), (29, 16385),
                             (47, 8), (48, 1), (73, 1), (83, 0), (85, 7999), (85, 8001)):
            values = passed_values(); values[field] = value
            self.assertFalse(assess(encoded(values))["own_surface_workload_verified"], field)
        values = passed_values(); values[28] = values[29] = 16384; values[71] = 0xffffffff
        self.assertTrue(assess(encoded(values))["own_surface_workload_verified"])

    def test_normal_shutdown_stop_flag_is_not_user_cancel(self):
        for stop in (0, 1):
            values = passed_values(); values[74] = stop
            self.assertTrue(assess(encoded(values))["own_surface_workload_verified"])
        values = passed_values(); values[4] = 2
        self.assertTrue(assess(encoded(values))["cleanup_complete"])
        self.assertFalse(assess(encoded(values))["own_surface_workload_verified"])

    def test_eventual_cleanup_after_deadline_is_not_workload_pass(self):
        values = passed_values(); values[7] = 1; values[4] = 10
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["own_surface_workload_verified"])

    def test_cli_preserves_saved_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); log = root / "surface.jsonl"; output = root / "assessment.json"
            data = encoded(); log.write_bytes(data)
            self.assertEqual(reader.main(["--log", str(log), "--pid", "71", "--baseline-absent", "--output", str(output)]), 0)
            self.assertEqual(log.read_bytes(), data)
            self.assertTrue(json.loads(output.read_text())["own_surface_workload_verified"])
            with self.assertRaises(SystemExit):
                reader.main(["--log", str(log), "--pid", "71", "--baseline-absent", "--output", str(log)])
            self.assertEqual(log.read_bytes(), data)
            log.write_bytes(encoded(passed_values()[:64]))
            self.assertEqual(reader.main(["--log", str(log), "--pid", "71", "--baseline-absent", "--output", str(output)]), 2)


if __name__ == "__main__":
    unittest.main()
