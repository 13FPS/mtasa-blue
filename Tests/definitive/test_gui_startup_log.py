"""Saved-file GUI64 framing, provenance, lifetime and owned-workload checks."""
from copy import deepcopy
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import gui_startup_log as reader


def passed_values():
    values = [0] * 64
    assignments = {0: 1, 1: 2, 2: 3, 3: 250, 4: 1, 6: 1, 8: 1, 9: 1, 10: 1, 11: 1,
                   12: 1, 14: 601, 16: 601, 18: 2, 19: 1, 20: 1, 21: 1, 24: 1,
                   25: 77, 27: 87, 28: 640, 29: 480, 30: 1, 31: 1, 32: 1, 33: 4, 35: 4,
                   37: 1, 38: 20001, 39: 1, 40: 2, 46: 2, 47: 8, 53: 1, 54: 1,
                   55: 1, 56: 1, 57: 1, 58: 1}
    for field, value in assignments.items():
        values[field] = value
    return values


def encoded(values=None, pid=71, run_ms=100, **extra):
    record = dict(schema=reader.SCHEMA, pid=pid, runMs=run_ms,
                  values=passed_values() if values is None else values)
    record.update(extra)
    return (json.dumps(record) + "\n").encode()


def assess(data=None, **kwargs):
    return reader.assess(encoded() if data is None else data,
                         expected_pid=kwargs.pop("expected_pid", 71), baseline=kwargs.pop("baseline", b""), **kwargs)


class GuiStartupLogTests(unittest.TestCase):
    def test_normal_offscreen_workload(self):
        result = assess()
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertTrue(result["own_gpu_workload_verified"])
        for field in ("visible_gta_drawing_verified", "full_mta_ready", "bridge_worker_lifecycle_verified"):
            self.assertFalse(result[field])
        self.assertEqual(result["reason"], "Completed")

    def test_explicit_pid_baseline_bytes_required(self):
        with self.assertRaises(TypeError):
            reader.assess(encoded(), expected_pid=71)
        for pid in (None, True, 0, -1, 1 << 32, 1.0):
            with self.subTest(pid=pid), self.assertRaises(ValueError):
                assess(expected_pid=pid)
        for baseline in (None, "", bytearray()):
            with self.subTest(baseline=baseline), self.assertRaises(ValueError):
                assess(baseline=baseline)

    def test_exact_baseline_append(self):
        baseline = encoded(pid=55, run_ms=90)
        result = assess(baseline + encoded(), baseline=baseline,
                        baseline_sha256=hashlib.sha256(baseline).hexdigest())
        self.assertTrue(result["own_gpu_workload_verified"])
        self.assertEqual(result["fresh_matching_records"], 1)

    def test_unchanged_baseline_cannot_supply_pass(self):
        data = encoded()
        self.assertFalse(assess(data, baseline=data)["diagnostic_valid"])

    def test_baseline_replaced_or_truncated(self):
        for baseline in (encoded(pid=72), encoded()[:-1], b"not a complete boundary"):
            with self.subTest(baseline=baseline):
                self.assertFalse(assess(encoded(), baseline=baseline)["diagnostic_valid"])

    def test_partial_baseline_even_when_prefix_retained(self):
        baseline = encoded()[:-1]
        self.assertFalse(assess(baseline + b"\n" + encoded(run_ms=101), baseline=baseline)["diagnostic_valid"])

    def test_baseline_hash_mismatch(self):
        self.assertFalse(assess(baseline_sha256="0" * 64)["diagnostic_valid"])
        for value in ("A" * 64, "g" * 64, "a", 7):
            with self.subTest(value=value), self.assertRaises(ValueError):
                assess(baseline_sha256=value)

    def test_inherited_same_pid_run_rejected(self):
        baseline = encoded()
        self.assertFalse(assess(baseline + encoded(), baseline=baseline)["diagnostic_valid"])
        self.assertTrue(assess(baseline + encoded(run_ms=101), baseline=baseline)["own_gpu_workload_verified"])

    def test_wrong_pid_and_run_never_inherited(self):
        self.assertFalse(assess(encoded(pid=72))["diagnostic_valid"])
        self.assertFalse(assess(expected_run_ms=101)["diagnostic_valid"])
        self.assertTrue(assess(expected_run_ms=100)["diagnostic_valid"])
        for run in (True, -1, 1 << 64, 1.0):
            with self.subTest(run=run), self.assertRaises(ValueError):
                assess(expected_run_ms=run)

    def test_ambiguous_runs_and_duplicate_records(self):
        for data in (encoded() * 2, encoded() + encoded(run_ms=101)):
            with self.subTest(data=data):
                self.assertFalse(assess(data)["diagnostic_valid"])
                self.assertFalse(assess(data, expected_run_ms=100)["diagnostic_valid"])

    def test_foreign_fresh_pid_is_not_matching_identity(self):
        self.assertTrue(assess(encoded(pid=72) + encoded())["own_gpu_workload_verified"])

    def test_duplicate_json_keys_at_any_depth(self):
        data = encoded().decode()
        for bad in (data.replace('"pid": 71', '"pid": 71, "pid": 71'),
                    data.replace('"runMs": 100', '"runMs": 100, "runMs": 101'),
                    data.replace('"values": [', '"values": {"x": 1, "x": 1}, "discard": [')):
            with self.subTest(data=bad):
                self.assertFalse(assess(bad.encode())["diagnostic_valid"])

    def test_malformed_truncated_and_non_utf8(self):
        for data in (b"{\n", encoded()[:-1], encoded()[:-2] + b"\n", b"\xff\n", b"null\n", b"[]\n",
                     encoded() + b"broken\n", b"\n" + encoded()):
            with self.subTest(data=data):
                self.assertFalse(assess(data)["diagnostic_valid"])

    def test_historical_invalid_line_cannot_hide_fresh_errors(self):
        baseline = b"historical unrelated malformed line\n"
        self.assertTrue(assess(baseline + encoded(), baseline=baseline)["diagnostic_valid"])
        self.assertFalse(assess(baseline + encoded() + b"broken\n", baseline=baseline)["diagnostic_valid"])

    def test_exact_schema_fields(self):
        for changes in ({"schema": "other"}, {"pid": True}, {"pid": 0}, {"runMs": True},
                        {"runMs": -1}, {"runMs": 1 << 64}, {"passed": True}, {"pointer": 123}):
            with self.subTest(changes=changes):
                self.assertFalse(assess(encoded(**changes))["diagnostic_valid"])
        missing = json.loads(encoded())
        del missing["runMs"]
        self.assertFalse(assess((json.dumps(missing) + "\n").encode())["diagnostic_valid"])

    def test_all_scalars_are_exact_uint32(self):
        for invalid in (True, False, -1, 1 << 32, 1.0, None, "1", [], {}):
            for field in (0, 3, 25, 38, 63):
                with self.subTest(invalid=invalid, field=field):
                    values = passed_values()
                    values[field] = invalid
                    self.assertFalse(assess(encoded(values))["diagnostic_valid"])
        for length in (0, 63, 65):
            self.assertFalse(assess(encoded([0] * length))["diagnostic_valid"])

    def test_known_enums_booleans_and_reserved_fields(self):
        for field, invalid in ((0, 2), (1, 0), (1, 4), (2, 0), (4, 12), (18, 1), (18, 3),
                               (23, 7), (41, 5), (42, 11), (46, 3), (47, 9), (48, 9)):
            with self.subTest(field=field, invalid=invalid):
                values = passed_values(); values[field] = invalid
                self.assertFalse(assess(encoded(values))["diagnostic_valid"])
        for field in reader.BOOLEAN_FIELDS + (60, 61, 62, 63):
            values = passed_values(); values[field] = 2 if field < 60 else 1
            self.assertFalse(assess(encoded(values))["diagnostic_valid"], field)

    def test_native_queue_and_backbuffer_writes_refused(self):
        for field in (44, 45):
            values = passed_values(); values[field] = 1
            self.assertFalse(assess(encoded(values))["diagnostic_valid"])

    def test_cleanup_cannot_claim_retained_ownership(self):
        for field in (5, 43, 49, 50, 51, 59):
            values = passed_values(); values[field] = 1
            self.assertFalse(assess(encoded(values))["diagnostic_valid"], field)
        for field in (9, 39):
            values = passed_values(); values[field] = 0
            self.assertFalse(assess(encoded(values))["diagnostic_valid"], field)

    def test_cleanup_is_separate_from_workload_success(self):
        values = passed_values()
        values[4] = 7; values[21] = 0; values[37] = 0; values[22] = 0x80004005
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["own_gpu_workload_verified"])

    def test_cancel_and_prepare_failure_cleanups(self):
        for reason in (2, 3, 4, 6, 8, 9):
            values = [0] * 64
            for field, value in {0: 1, 1: 2, 2: 1, 4: reason, 6: 1, 9: 1, 39: 1, 40: 2}.items():
                values[field] = value
            result = assess(encoded(values))
            self.assertTrue(result["diagnostic_valid"], reason)
            self.assertTrue(result["cleanup_complete"], reason)
            self.assertFalse(result["own_gpu_workload_verified"], reason)

    def test_partial_and_retained_diagnostics_do_not_pass(self):
        values = passed_values()
        values[1] = 3; values[4] = 10; values[5] = 1; values[6] = 0; values[39] = 0; values[49] = 1
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertFalse(result["cleanup_complete"])
        self.assertFalse(result["own_gpu_workload_verified"])
        values[59] = 1
        self.assertTrue(assess(encoded(values))["diagnostic_valid"])

    def test_fence_and_readback_are_required(self):
        for field, value in ((33, 0), (35, 3), (37, 0), (38, 10000), (46, 1), (53, 0), (54, 0), (55, 0), (56, 0)):
            values = passed_values(); values[field] = value
            self.assertFalse(assess(encoded(values))["own_gpu_workload_verified"], field)
        values = passed_values(); values[33] = 0xffffffff; values[34] = 1; values[35] = 0; values[36] = 2
        self.assertTrue(assess(encoded(values))["own_gpu_workload_verified"])

    def test_device_removed_fence_sentinel_is_not_completion(self):
        values = passed_values(); values[35] = values[36] = 0xffffffff
        self.assertFalse(assess(encoded(values))["diagnostic_valid"])
        values[52] = 1
        self.assertFalse(assess(encoded(values))["own_gpu_workload_verified"])

    def test_lease_and_late_native_failure_refuse_work(self):
        for field, value in ((12, 0), (14, 0), (16, 0), (18, 0), (58, 0), (42, 10)):
            values = passed_values(); values[field] = value
            self.assertFalse(assess(encoded(values))["own_gpu_workload_verified"], field)
        values = passed_values()
        values[4] = 11; values[19] = values[20] = values[21] = values[37] = values[58] = 0
        self.assertTrue(assess(encoded(values))["diagnostic_valid"])
        self.assertFalse(assess(encoded(values))["own_gpu_workload_verified"])

    def test_no_counter_inequality_assumed(self):
        values = passed_values(); values[14] = 1; values[16] = 100
        self.assertTrue(assess(encoded(values))["own_gpu_workload_verified"])

    def test_fixed_workload_contract_and_failure_stage(self):
        for field, value in ((7, 1), (22, 1), (23, 1), (24, 2), (27, 24), (28, 1920), (29, 1080),
                             (47, 7), (48, 4), (52, 1)):
            values = passed_values(); values[field] = value
            self.assertFalse(assess(encoded(values))["own_gpu_workload_verified"], field)

    def test_cleanup_deadline_remains_visible(self):
        values = passed_values(); values[4] = 10; values[7] = 1
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["own_gpu_workload_verified"])

    def test_cli_is_saved_file_only_and_preserves_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); log = root / "gui.jsonl"; baseline = root / "baseline.jsonl"; output = root / "assessment.json"
            log.write_bytes(encoded()); baseline.write_bytes(b"")
            self.assertEqual(reader.main(["--log", str(log), "--pid", "71", "--baseline", str(baseline), "--output", str(output)]), 0)
            self.assertTrue(json.loads(output.read_text())["own_gpu_workload_verified"])
            self.assertEqual(log.read_bytes(), encoded()); self.assertEqual(baseline.read_bytes(), b"")
            with self.assertRaises(SystemExit):
                reader.main(["--log", str(log), "--pid", "71", "--baseline-absent", "--output", str(log)])
            with redirect_stdout(io.StringIO()):
                self.assertEqual(reader.main(["--log", str(log), "--pid", "72", "--baseline-absent"]), 2)


if __name__ == "__main__":
    unittest.main()
