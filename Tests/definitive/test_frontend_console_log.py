"""Console32 attribution, real command counters and optional CPU ownership."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import frontend_console_log as reader


def stopped_values(**updates):
    values = [0] * 32
    for field, value in {0: 1, 1: 4, 2: 42, 3: 1, 11: 1, 13: 1, 14: 1, 15: 1,
                         18: 150, 21: 1, 24: 1, 29: 255, 30: 7}.items():
        values[field] = value
    for field, value in updates.items():
        values[int(field)] = value
    return values


def disabled_values():
    values = [0] * 32
    for field, value in {0: 1, 2: 42, 21: 1, 24: 1, 29: 255}.items():
        values[field] = value
    return values


def encoded(values=None, pid=71, run_ms=100):
    return (json.dumps(dict(schema=reader.SCHEMA, pid=pid, runMs=run_ms,
                           values=stopped_values() if values is None else values)) + "\n").encode()


def assess(data=None, **kwargs):
    return reader.assess(encoded() if data is None else data, expected_pid=kwargs.pop("expected_pid", 71),
                         baseline=kwargs.pop("baseline", b""), **kwargs)


class FrontendConsoleLogTests(unittest.TestCase):
    def test_actual_command_counts_are_separate_from_game_rendering_and_native_control(self):
        result = assess()
        for key in ("diagnostic_valid", "cleanup_complete", "console_created", "command_execution_observed",
                    "console_command_workload_verified", "configuration_read_only_declared"):
            self.assertTrue(result[key], key)
        for key in ("native_control_command_verified", "camera_capture_verified", "visible_gta_drawing_verified",
                    "gui_gpu_cleanup_verified", "gameplay_ready", "full_mta_ready", "bridge_worker_lifecycle_verified",
                    "network_started", "history_file_mutation_verified"):
            self.assertFalse(result[key], key)
        self.assertEqual(result["phase"], "Stopped")
        self.assertEqual(result["last_command"], "Help")

    def test_clear_may_finish_with_empty_output_or_a_memo_newline(self):
        for output_bytes in (0, 1):
            with self.subTest(output_bytes=output_bytes):
                values = stopped_values(**{"13": 2, "18": output_bytes})
                result = assess(encoded(values))
                self.assertTrue(result["console_command_workload_verified"])
                self.assertEqual(result["last_command"], "Clear")

    def test_help_requires_memo_output_without_claiming_its_unavailable_contents(self):
        values = stopped_values(**{"18": 0})
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertTrue(result["command_execution_observed"])
        self.assertFalse(result["console_command_workload_verified"])

    def test_never_enabled_and_clean_failed_creation_do_not_claim_command_execution(self):
        for phase in (0, 4):
            values = disabled_values()
            values[1] = phase
            result = assess(encoded(values))
            self.assertTrue(result["diagnostic_valid"])
            self.assertTrue(result["cleanup_complete"])
            self.assertFalse(result["console_created"])
            self.assertFalse(result["command_execution_observed"])
            self.assertFalse(result["console_command_workload_verified"])

    def test_disabled_console_clean_flag_does_not_release_a_live_menu_owner(self):
        values = disabled_values()
        values[6] = values[8] = 1
        result = assess(encoded(values))
        self.assertTrue(result["diagnostic_valid"])
        self.assertFalse(result["cleanup_complete"])

    def test_created_console_without_submission_is_clean_but_has_no_command_workload(self):
        values = stopped_values(**{"11": 0, "13": 0, "14": 0, "15": 0, "18": 0, "19": 12})
        result = assess(encoded(values))
        self.assertTrue(result["cleanup_complete"])
        self.assertTrue(result["console_created"])
        self.assertFalse(result["console_command_workload_verified"])

    def test_rejected_final_command_is_reported_independently_of_prior_success(self):
        values = stopped_values(**{"12": 1, "13": 3, "14": 2, "15": 2})
        result = assess(encoded(values))
        self.assertTrue(result["command_execution_observed"])
        self.assertTrue(result["command_refusals_observed"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["console_command_workload_verified"])
        self.assertEqual(result["last_command"], "Rejected")

    def test_cached_text_and_history_counts_do_not_retain_destroyed_widgets(self):
        values = stopped_values(**{"15": 1, "16": 2, "17": 1, "18": 4096, "19": 1020})
        result = assess(encoded(values))
        self.assertTrue(result["cleanup_complete"])
        self.assertTrue(result["history_selection_calls_observed"])
        self.assertTrue(result["autocomplete_observed"])

    def test_running_and_pending_snapshots_do_not_claim_terminal_cleanup(self):
        running = stopped_values(**{"1": 2, "4": 1, "5": 1, "6": 1, "7": 1, "8": 1,
                                    "9": 1, "10": 2, "21": 0, "22": 1, "23": 1})
        pending = stopped_values(**{"1": 3, "21": 0})
        failed = disabled_values()
        failed[1] = 5
        for values in (running, pending, failed):
            result = assess(encoded(values))
            self.assertTrue(result["diagnostic_valid"])
            self.assertFalse(result["cleanup_complete"])
            self.assertFalse(result["console_command_workload_verified"])

    def test_completed_cleanup_cannot_conceal_console_command_input_or_fault_owners(self):
        for field in (4, 5, 6, 7, 8, 9, 10, 22, 23, 28):
            with self.subTest(field=field):
                values = stopped_values()
                values[field] = 1
                result = assess(encoded(values))
                self.assertFalse(result["cleanup_complete"])
                self.assertFalse(result["console_command_workload_verified"])

    def test_unsupported_protocol_flags_versions_and_scalar_types_fail_diagnostics(self):
        for field, value in ((0, 2), (1, 6), (2, 0), (3, 2), (10, 3), (13, 4), (15, 129),
                             (20, 1), (24, 0), (25, 1), (26, 1), (27, 1), (29, 254), (31, 1), (19, 1021)):
            with self.subTest(field=field, value=value):
                values = stopped_values()
                values[field] = value
                self.assertFalse(assess(encoded(values))["diagnostic_valid"])
        for value in (True, -1, 2 ** 32, 1.5, None, "1"):
            values = stopped_values()
            values[30] = value
            self.assertFalse(assess(encoded(values))["diagnostic_valid"])
        for count in (0, 31, 33, 160):
            self.assertFalse(assess(encoded([0] * count))["diagnostic_valid"])

    def test_impossible_command_and_history_counters_never_create_success(self):
        for updates in ({11: 0}, {13: 0}, {13: 3}, {14: 0}, {15: 2}, {15: 0, 17: 1},
                        {3: 0}, {3: 0, 14: 0, 15: 0, 18: 0, 19: 1}):
            values = stopped_values()
            for field, value in updates.items():
                values[field] = value
            self.assertFalse(assess(encoded(values))["console_command_workload_verified"])

    def test_invalid_fresh_json_or_another_protocol_cannot_supply_a_console_pass(self):
        cases = (encoded()[:-1], b"not-json\n", encoded().replace(b'"pid": 71', b'"pid": 71, "pid": 71'),
                 encoded().replace(reader.SCHEMA.encode(), b"mta_core_gui_frontend_v1"),
                 encoded().replace(b'"pid": 71', b'"pid": true'), encoded().replace(b'"runMs": 100', b'"runMs": -1'),
                 encoded().replace(b'"schema":', b'"extra": 0, "schema":'))
        for data in cases:
            self.assertFalse(assess(data)["diagnostic_valid"])

    def test_freshness_requires_exact_baseline_boundary_hash_pid_and_optional_run(self):
        baseline = encoded(run_ms=99)
        data = baseline + encoded()
        result = assess(data, baseline=baseline, expected_run_ms=100,
                        baseline_sha256=hashlib.sha256(baseline).hexdigest())
        self.assertTrue(result["console_command_workload_verified"])
        for options in ({"baseline": data}, {"baseline": baseline[:-1]}, {"baseline": b"different\n"},
                        {"baseline": baseline, "baseline_sha256": "0" * 64}, {"expected_pid": 72}, {"expected_run_ms": 101}):
            self.assertFalse(assess(data, **options)["diagnostic_valid"])

    def test_duplicate_fresh_pid_records_and_replayed_run_identity_are_ambiguous(self):
        for data, options in ((encoded() + encoded(), {}), (encoded() + encoded(run_ms=101), {"expected_run_ms": 100}),
                              (encoded() + encoded(), {"baseline": encoded()})):
            self.assertFalse(assess(data, **options)["diagnostic_valid"])
        baseline = b"old invalid line\n"
        self.assertTrue(assess(baseline + encoded(), baseline=baseline)["console_command_workload_verified"])

    def test_caller_identity_and_baseline_contracts_are_explicit(self):
        for options in ({"expected_pid": 0}, {"expected_pid": True}, {"expected_pid": 2 ** 32},
                        {"expected_run_ms": -1}, {"expected_run_ms": True}, {"baseline_sha256": "AB" * 32},
                        {"baseline_sha256": "bad"}, {"baseline": ""}):
            with self.assertRaises(ValueError):
                assess(**options)
        with self.assertRaises(ValueError):
            assess("not bytes")

    def test_cli_requires_baseline_selection_and_preserves_evidence_when_output_aliases_it(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "gui-console.log"
            log.write_bytes(encoded())
            for arguments in (["--log", str(log), "--pid", "71"],
                              ["--log", str(log), "--pid", "71", "--baseline-absent", "--output", str(log)]):
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as failure:
                    reader.main(arguments)
                self.assertEqual(failure.exception.code, 2)
                self.assertEqual(log.read_bytes(), encoded())
            output = Path(directory) / "assessment.json"
            self.assertEqual(reader.main(["--log", str(log), "--pid", "71", "--baseline-absent", "--output", str(output)]), 0)
            self.assertTrue(json.loads(output.read_text())["console_command_workload_verified"])


if __name__ == "__main__":
    unittest.main()
