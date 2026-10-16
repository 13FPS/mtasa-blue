"""Frontend160 attribution, control restoration and retained-owner evidence."""
import hashlib
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import gui_frontend_log as reader


def passed_values():
    v = [0] * 160
    assignments = {0: 1, 1: 2, 2: 20, 3: 31000, 4: 1, 6: 1, 8: 1, 9: 1, 10: 1, 11: 1,
        12: 1, 14: 601, 16: 601, 18: 2, 19: 1, 20: 1, 21: 1, 24: 1, 27: 87,
        28: 1280, 29: 1024, 30: 1, 31: 1, 32: 1, 33: 60, 35: 60, 37: 1, 38: 20001,
        39: 1, 40: 2, 46: 120, 47: 9, 55: 1, 56: 1, 57: 1, 58: 1, 60: 1, 61: 1,
        62: 1, 63: 1, 64: 1, 65: 1, 66: 1, 67: 60, 68: 60, 70: 60, 74: 1,
        78: 1, 79: 1, 80: 1, 81: 42, 82: 43, 83: 1, 84: 60, 85: 30000, 86: 1,
        96: 1, 97: 4, 98: 42, 99: 600, 103: 1, 105: 1, 106: 1, 107: 1, 108: 1,
        109: 1, 113: 1, 118: 1, 121: 1, 124: 1, 128: 1, 129: 1, 131: 1,
        133: 7, 139: 1, 142: 8, 143: 1, 144: 1, 146: 1, 147: 1, 148: 1,
        151: 1000, 153: 30000, 154: 1, 155: 1}
    for i, value in assignments.items():
        v[i] = value
    return v


def encoded(v=None, pid=71, run_ms=100):
    return (json.dumps(dict(schema=reader.SCHEMA, pid=pid, runMs=run_ms,
                           values=passed_values() if v is None else v)) + "\n").encode()


def assess(data=None, **kwargs):
    return reader.assess(encoded() if data is None else data, expected_pid=kwargs.pop("expected_pid", 71),
                         baseline=kwargs.pop("baseline", b""), **kwargs)


class GuiFrontendLogTests(unittest.TestCase):
    def test_complete_menu_and_control_handshake_have_separate_runtime_claims(self):
        result = assess()
        for key in ("diagnostic_valid", "cleanup_complete", "own_frontend_workload_verified",
                    "composition_present_verified", "control_restore_acknowledged", "menu_callbacks_observed"):
            self.assertTrue(result[key], key)
        for key in ("native_control_command_verified", "camera_capture_verified", "credits_close_verified",
                    "visible_gta_drawing_verified", "full_mta_ready", "bridge_worker_lifecycle_verified"):
            self.assertFalse(result[key], key)

    def test_quit_or_escape_may_complete_before_duration(self):
        for field in (103, 141):
            v = passed_values()
            v[103] = 0
            v[field] = 1
            v[3] = v[153] = 2500
            self.assertTrue(assess(encoded(v))["own_frontend_workload_verified"])

    def test_early_automatic_stop_is_not_a_full_workload(self):
        v = passed_values()
        v[103] = v[141] = 0
        v[153] = 29999
        self.assertFalse(assess(encoded(v))["own_frontend_workload_verified"])

    def test_rendering_does_not_imply_callbacks(self):
        for field in (105, 131, 133):
            v = passed_values()
            v[field] = 0
            result = assess(encoded(v))
            self.assertTrue(result["own_frontend_workload_verified"])
            self.assertFalse(result["menu_callbacks_observed"])

    def test_expired_or_faulted_lease_can_be_restored_without_workload_success(self):
        v = passed_values()
        v[4], v[21], v[149] = 7, 0, 1
        result = assess(encoded(v))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertTrue(result["control_restore_acknowledged"])
        self.assertFalse(result["own_frontend_workload_verified"])

    def test_historical_input_failure_or_late_drain_does_not_conceal_retirement(self):
        for field in (7, 140):
            v = passed_values()
            v[4], v[21], v[field] = 10, 0, 1
            result = assess(encoded(v))
            self.assertTrue(result["diagnostic_valid"])
            self.assertTrue(result["cleanup_complete"])
            self.assertTrue(result["control_restore_acknowledged"])
            self.assertFalse(result["own_frontend_workload_verified"])

    def test_clean_early_failure_never_claims_menu_or_engine_control(self):
        v = [0] * 160
        for i, value in {0: 1, 1: 2, 2: 1, 4: 3, 6: 1, 9: 1, 39: 1, 78: 1, 79: 1, 80: 1, 128: 1}.items():
            v[i] = value
        result = assess(encoded(v))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["own_frontend_workload_verified"])
        self.assertFalse(result["control_restore_acknowledged"])

    def test_creation_refusal_can_stop_without_publishing_a_cpu_owner(self):
        v = passed_values()
        v[96:128] = [0] * 32
        v[4], v[21], v[154], v[155] = 7, 0, 0, 1
        result = assess(encoded(v))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["own_frontend_workload_verified"])

    def test_unknown_input_protocol_is_not_promoted(self):
        for version in (0, 2, reader.UINT32_MAX):
            v = passed_values()
            v[128] = version
            self.assertFalse(assess(encoded(v))["diagnostic_valid"])

    def test_cleanup_cannot_conceal_any_native_gui_control_or_cpu_owner(self):
        for field, value in ((5, 1), (39, 0), (41, 1), (43, 1), (49, 1), (50, 1), (51, 1),
                             (59, 1), (72, 1), (75, 1), (76, 1), (77, 1), (78, 0), (79, 0),
                             (80, 0), (102, 1), (112, 1), (113, 0), (115, 1), (116, 1),
                             (117, 1), (137, 1), (138, 1), (139, 0), (147, 0), (148, 0)):
            with self.subTest(field=field):
                v = passed_values()
                v[field] = value
                result = assess(encoded(v))
                self.assertFalse(result["cleanup_complete"])
                self.assertFalse(result["own_frontend_workload_verified"])

    def test_unacknowledged_control_request_cannot_admit_menu(self):
        v = passed_values()
        v[144] = 0
        self.assertFalse(assess(encoded(v))["diagnostic_valid"])

    def test_unsubmitted_or_fixture_readback_is_not_menu_proof(self):
        for field, value in ((19, 0), (30, 0), (31, 0), (32, 0), (38, 10000), (53, 1), (54, 1), (55, 0)):
            v = passed_values()
            v[field] = value
            self.assertFalse(assess(encoded(v))["own_frontend_workload_verified"])

    def test_pointer_reserved_full_game_and_foreign_protocol_claims_rejected(self):
        for field in (44, 45, 88, 95, 110, 119, 120, 122, 123, 125, 127, 156, 157, 158, 159):
            v = passed_values()
            v[field] = 1
            self.assertFalse(assess(encoded(v))["diagnostic_valid"])
        for count in (32, 64, 96, 128, 159, 161):
            self.assertFalse(assess(encoded(([0] * count)))["diagnostic_valid"])

    def test_missing_fences_or_present_accounting_never_pass(self):
        for updates in ({33: 0}, {35: 59}, {67: 61}, {68: 1}, {69: 1}, {70: 59}, {84: 0}, {85: 8000}):
            v = passed_values()
            for i, value in updates.items():
                v[i] = value
            self.assertFalse(assess(encoded(v))["own_frontend_workload_verified"])

    def test_bad_scalars_duplicate_keys_truncation_and_wrong_schema_rejected(self):
        for value in (True, -1, 2 ** 32, 1.5, None, "1"):
            v = passed_values()
            v[2] = value
            self.assertFalse(assess(encoded(v))["diagnostic_valid"])
        for data in (encoded()[:-1], encoded().replace(b'"pid": 71', b'"pid": 71, "pid": 71'),
                     encoded().replace(reader.SCHEMA.encode(), b'mta_core_gui_surface_v1'), b'not-json\n'):
            self.assertFalse(assess(data)["diagnostic_valid"])

    def test_absent_or_changed_baseline_and_wrong_pid_do_not_inherit_a_pass(self):
        baseline = encoded(run_ms=99)
        self.assertFalse(assess(baseline, baseline=baseline)["diagnostic_valid"])
        self.assertFalse(assess(encoded(), baseline=baseline)["diagnostic_valid"])
        self.assertFalse(assess(encoded(), expected_pid=72)["diagnostic_valid"])
        self.assertFalse(assess(encoded(), expected_run_ms=101)["diagnostic_valid"])
        result = assess(baseline + encoded(), baseline=baseline,
                        baseline_sha256=hashlib.sha256(baseline).hexdigest(), expected_run_ms=100)
        self.assertTrue(result["own_frontend_workload_verified"])

    def test_duplicate_fresh_or_replayed_run_identity_rejected(self):
        self.assertFalse(assess(encoded() + encoded())["diagnostic_valid"])
        self.assertFalse(assess(encoded() + encoded(), baseline=encoded())["diagnostic_valid"])


if __name__ == "__main__":
    unittest.main()
