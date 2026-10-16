"""Frontend256 freshness, actual neutral scope and independent retirement evidence."""
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import gui_frontend_input_log as reader


def passed_values():
    v = [0] * 256
    values = {0: 1, 1: 2, 2: 20, 3: 20050, 4: 1, 6: 1, 12: 1, 14: 2, 16: 3,
              18: 2, 24: 1, 27: 87, 28: 1280, 29: 1024, 33: 10, 35: 10,
              38: 20001, 46: 20, 47: 9, 67: 10, 68: 10, 70: 10, 81: 100, 82: 101,
              83: 1, 84: 10, 85: 20000, 96: 1, 97: 4, 98: 100, 99: 20,
              128: 1, 129: 4, 130: 100, 139: 3, 141: 1, 142: 3, 143: 2, 146: 120,
              157: 255, 158: 111, 160: 1, 163: 1, 164: 1, 165: 8,
              176: 1, 177: 4, 178: 100, 181: 10, 182: 7, 183: 3, 184: 7,
              185: 1, 187: 20, 193: 111, 212: 26, 214: 20, 216: 60, 220: 20,
              224: 4, 230: 2, 240: 50, 242: 20000}
    flags = (8, 9, 10, 11, 19, 20, 21, 30, 31, 32, 37, 39, 55, 56, 57, 58,
             60, 61, 62, 63, 64, 65, 66, 78, 79, 80, 86, 106, 107, 108, 109,
             113, 118, 121, 124, 131, 149, 152, 161, 171, 179, 180, 188, 189,
             190, 192, 232, 236, 237, 238)
    for field, value in values.items():
        v[field] = value
    for field in flags:
        v[field] = 1
    return v


def encoded(v=None, pid=71, run_ms=100):
    return (json.dumps(dict(schema=reader.SCHEMA, pid=pid, runMs=run_ms,
                           values=passed_values() if v is None else v)) + "\n").encode()


def assess(data=None, **kwargs):
    return reader.assess(encoded() if data is None else data,
                         expected_pid=kwargs.pop("expected_pid", 71),
                         baseline=kwargs.pop("baseline", b""), **kwargs)


class GuiFrontendInputLogTests(unittest.TestCase):
    def test_neutral_scope_and_console_commands_have_independent_claims(self):
        result = assess()
        for key in ("diagnostic_valid", "cleanup_complete", "own_frontend_input_workload_verified",
                    "native_scoped_neutral_verified", "bindings_neutral_producer_verified",
                    "native_input_cleanup_verified", "console_command_workload_verified"):
            self.assertTrue(result[key], key)
        for key in ("native_player_control_command_verified", "camera_capture_verified",
                    "visible_gta_drawing_verified", "bridge_worker_lifecycle_verified",
                    "gameplay_ready", "full_mta_ready"):
            self.assertFalse(result[key], key)

    def test_unknown_action_neutral_counter_is_not_all_routed_queries(self):
        for count in (0, 3, 60):
            v = passed_values()
            v[218] = count
            self.assertTrue(assess(encoded(v))["own_frontend_input_workload_verified"])

    def test_foreign_callbacks_are_forwarded_but_nested_failure_is_not_success(self):
        self.assertTrue(assess()["own_frontend_input_workload_verified"])
        v = passed_values()
        v[226] = 1
        result = assess(encoded(v))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["native_scoped_neutral_verified"])

    def test_no_scopes_or_queries_cannot_prove_native_neutral_writes(self):
        for field in (212, 214, 216):
            v = passed_values()
            v[4], v[21], v[237] = 7, 0, 0
            v[field] = 0
            if field in (212, 214):
                v[214] = v[220] = 0
            result = assess(encoded(v))
            self.assertTrue(result["diagnostic_valid"], field)
            self.assertTrue(result["cleanup_complete"], field)
            self.assertFalse(result["native_scoped_neutral_verified"], field)

    def test_actual_bindings_are_required_even_when_native_hooks_ran(self):
        for field in (179, 180, 182, 184, 187):
            v = passed_values()
            if field in (179, 180):
                v[187] = 0
            if field == 182:
                v[184] = 0
            v[field] = 0
            result = assess(encoded(v))
            self.assertTrue(result["diagnostic_valid"], field)
            self.assertFalse(result["bindings_neutral_producer_verified"], field)
            self.assertFalse(result["own_frontend_input_workload_verified"], field)

    def test_live_suppression_is_cleared_while_historical_neutral_states_remain(self):
        v = passed_values()
        self.assertEqual(v[186], 0)
        self.assertTrue(reader.bindings_neutral_verified(v))
        v[186] = 1
        self.assertFalse(assess(encoded(v))["diagnostic_valid"])

    def test_deadline_or_native_failure_does_not_conceal_clean_retirement(self):
        for field, value in ((7, 1), (228, 5), (172, 1), (241, 1)):
            v = passed_values()
            v[4], v[21], v[field] = 10, 0, value
            result = assess(encoded(v))
            self.assertTrue(result["diagnostic_valid"], field)
            self.assertTrue(result["cleanup_complete"], field)
            self.assertTrue(result["console_command_workload_verified"], field)
            self.assertFalse(result["own_frontend_input_workload_verified"], field)

    def test_retained_owners_or_unrestored_scope_cannot_claim_cleanup(self):
        for field, value in ((208, 1), (210, 1), (220, 19), (222, 1), (239, 1),
                             (243, 1), (169, 1), (170, 1), (171, 0), (191, 1),
                             (190, 0), (197, 1), (114, 1), (116, 1), (117, 1), (150, 1)):
            with self.subTest(field=field):
                v = passed_values()
                v[field] = value
                self.assertFalse(assess(encoded(v))["diagnostic_valid"])

    def test_failure_before_creation_can_be_clean_without_workload(self):
        v = [0] * 256
        for field, value in {0: 1, 1: 2, 2: 1, 4: 3, 6: 1, 9: 1, 39: 1,
                             78: 1, 79: 1, 80: 1, 160: 1, 238: 1}.items():
            v[field] = value
        result = assess(encoded(v))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["own_frontend_input_workload_verified"])
        self.assertFalse(result["console_command_workload_verified"])

    def test_quit_or_escape_can_complete_shorter_workload(self):
        for field in (103, 173):
            v = passed_values()
            v[242], v[field] = 1000, 1
            self.assertTrue(assess(encoded(v))["own_frontend_input_workload_verified"])
        v = passed_values()
        v[242] = 19999
        self.assertFalse(assess(encoded(v))["own_frontend_input_workload_verified"])

    def test_deferred_hidden_frontend_does_not_require_native_admission(self):
        v = [0] * 256
        for field, value in {0: 1, 1: 1, 2: 1, 160: 1, 161: 1, 238: 1}.items():
            v[field] = value
        self.assertTrue(reader.fields_valid(v))
        for field in (162, 165, 169, 170):
            changed = v.copy()
            changed[field] = 1
            self.assertFalse(reader.fields_valid(changed), field)

    def test_native_counters_are_uint64_but_bool_and_move_state_pairs_are_bounded(self):
        v = passed_values()
        for high in (213, 215, 217, 221):
            v[high] = 1
        self.assertTrue(assess(encoded(v))["own_frontend_input_workload_verified"])
        for field in (209, 211, 229, 231):
            v = passed_values()
            v[field] = 1
            self.assertFalse(assess(encoded(v))["diagnostic_valid"], field)

    def test_reserved_unsafe_or_noninteger_fields_are_rejected(self):
        for field in (88, 125, 159, 174, 175, 198, 207, 233, 244, 245, 246, 255,
                      120, 122, 123, 148, 153, 154, 155, 194, 195, 196):
            v = passed_values()
            v[field] = 1
            self.assertFalse(assess(encoded(v))["diagnostic_valid"], field)
        for value in (True, -1, 1.0, None, reader.UINT32_MAX + 1):
            v = passed_values()
            v[2] = value
            self.assertFalse(assess(encoded(v))["diagnostic_valid"], value)
        self.assertFalse(assess(encoded(passed_values()[:-1]))["diagnostic_valid"])

    def test_freshness_requires_exact_pid_run_and_baseline_hash(self):
        baseline = encoded(pid=11, run_ms=9)
        data = baseline + encoded()
        result = assess(data, baseline=baseline, expected_run_ms=100,
                        baseline_sha256=hashlib.sha256(baseline).hexdigest())
        self.assertTrue(result["diagnostic_valid"])
        for kwargs in (dict(expected_pid=70), dict(expected_run_ms=101),
                       dict(baseline_sha256="0" * 64), dict(baseline=b"unrelated\n")):
            self.assertFalse(assess(data, **({"baseline": baseline} | kwargs))["diagnostic_valid"])

    def test_inherited_replay_duplicates_and_fresh_malformed_records_are_rejected(self):
        old = encoded()
        for data, baseline in ((old, old), (old + old, old), (old + old, b""),
                               (old + b"{}\n", b""), (old[:-1], b""),
                               (b'{"schema":1,"schema":2}\n' + old, b""),
                               (b"\xff\n" + old, b"")):
            self.assertFalse(assess(data, baseline=baseline)["diagnostic_valid"])
        self.assertTrue(assess(encoded(pid=22) + old)["diagnostic_valid"])
        bad_old = b"old schema from another mode\n"
        self.assertTrue(assess(bad_old + old, baseline=bad_old)["diagnostic_valid"])
        self.assertFalse(assess(old, baseline=old[:-1])["diagnostic_valid"])

    def test_claims_do_not_require_console_commands(self):
        v = passed_values()
        for field in (139, 141, 142, 143, 146):
            v[field] = 0
        result = assess(encoded(v))
        self.assertTrue(result["own_frontend_input_workload_verified"])
        self.assertFalse(result["console_command_workload_verified"])

    def test_cli_requires_explicit_baseline_and_refuses_evidence_output_aliases(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "log.jsonl"
            source.write_bytes(encoded())
            output = Path(directory) / "result.json"
            with contextlib.redirect_stderr(io.StringIO()):
                for args in (["--log", str(source), "--pid", "71"],
                             ["--log", str(source), "--pid", "71", "--baseline-absent", "--output", str(source)]):
                    with self.assertRaises(SystemExit):
                        reader.main(args)
                linked = Path(directory) / "same-log.jsonl"
                os.link(source, linked)
                with self.assertRaises(SystemExit):
                    reader.main(["--log", str(source), "--pid", "71", "--baseline-absent", "--output", str(linked)])
            self.assertEqual(reader.main(["--log", str(source), "--pid", "71", "--baseline-absent", "--output", str(output)]), 0)
            failed = passed_values()
            failed[4], failed[21] = 10, 0
            source.write_bytes(encoded(failed))
            self.assertEqual(reader.main(["--log", str(source), "--pid", "71", "--baseline-absent", "--output", str(output)]), 2)
            self.assertTrue(json.loads(output.read_text())["cleanup_complete"])


if __name__ == "__main__":
    unittest.main()
