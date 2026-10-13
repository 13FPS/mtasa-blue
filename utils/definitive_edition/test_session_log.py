"""Fixtures only: no game execution, process inspection, or installed-file writes."""

import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

import session_log


ENTRY = {"event": "adapter_phase", "phase": "entry", "mode": "diagnostic", "host": "sa_unreal"}
COMPLETE = {"event": "adapter_diagnostic_complete", "phase": "diagnostic_complete", "mode": "diagnostic",
            "bridge_loaded": False, "game_adapter_active": False, "network_active": False}


def line(event, stamp="06:35:00", quoted=True):
    encoded = json.dumps(event)
    if quoted:
        encoded = json.dumps(encoded)
    return f'{stamp} [INFO] ["DE Onli"] {encoded}\n'


def valid_log(stamp="06:35:00"):
    return line(ENTRY, stamp) + line(COMPLETE, stamp)


class ParserTests(unittest.TestCase):
    def test_actual_cleo_quoted_json_and_plain_json(self):
        for quoted in (True, False):
            with self.subTest(quoted=quoted):
                result = session_log.analyze_log(line(ENTRY, quoted=quoted) + line(COMPLETE, quoted=quoted))
                self.assertEqual(result["status"], "diagnostic_complete")
                self.assertTrue(result["diagnostic_markers"]["inactive_flags_valid"])
                self.assertFalse(result["shutdown_evidence"]["runtime_exit"])

    def test_completion_before_entry_is_incomplete(self):
        result = session_log.analyze_log(line(COMPLETE) + line(ENTRY))
        self.assertEqual(result["status"], "incomplete")

    def test_completion_alone_cannot_pass(self):
        self.assertFalse(session_log.analyze_log(line(COMPLETE))["diagnostic_complete"])

    def test_inactive_flags_require_literal_false(self):
        for flag in session_log.INACTIVE_FLAGS:
            for value in (0, "false", None, True):
                with self.subTest(flag=flag, value=value):
                    invalid = {**COMPLETE, flag: value}
                    result = session_log.analyze_log(line(ENTRY) + line(invalid))
                    expected = "unexpected_diagnostic_activity" if value is True else "invalid_diagnostic_completion"
                    self.assertEqual(result["status"], expected)
                    self.assertFalse(result["diagnostic_complete"])
            missing = dict(COMPLETE)
            del missing[flag]
            self.assertFalse(session_log.analyze_log(line(ENTRY) + line(missing))["diagnostic_complete"])

    def test_completion_mode_and_phase_must_match(self):
        for change in ({"mode": "online"}, {"phase": "controller_run"}):
            self.assertFalse(session_log.analyze_log(line(ENTRY) + line({**COMPLETE, **change}))["diagnostic_complete"])

    def test_good_marker_cannot_hide_numeric_false_in_later_marker(self):
        # Python dictionary equality treats zero as equal to False; validation
        # must preserve the JSON boolean type for every completion marker.
        result = session_log.analyze_log(valid_log() + line({**COMPLETE, "network_active": 0}))
        self.assertEqual(result["status"], "invalid_diagnostic_completion")

    def test_latest_entry_replaces_old_success(self):
        result = session_log.analyze_log(valid_log() + line(ENTRY, "06:36:00"))
        self.assertEqual(result["status"], "incomplete")
        self.assertFalse(result["diagnostic_markers"]["completion"])

    def test_failure_after_completion_wins(self):
        failed = {"event": "adapter_failed", "phase": "entry", "error": "fixture failure"}
        result = session_log.analyze_log(valid_log() + line(failed))
        self.assertEqual(result["status"], "adapter_failed")
        self.assertEqual(result["failures"][0]["error"], "fixture failure")

    def test_bridge_load_is_reported_and_rejects_diagnostic_success(self):
        bridge = {"event": "adapter_phase", "phase": "bridge_load", "mode": "diagnostic"}
        result = session_log.analyze_log(line(ENTRY) + line(bridge) + line(COMPLETE))
        self.assertEqual(result["status"], "unexpected_diagnostic_activity")
        self.assertEqual(result["bridge_evidence"][0]["phase"], "bridge_load")

    def test_online_entry_reports_its_own_bridge_path(self):
        result = session_log.analyze_log(valid_log() + line({**ENTRY, "mode": "online"}) +
                                         line({"event": "adapter_phase", "phase": "bridge_loaded", "mode": "online"}))
        self.assertEqual(result["status"], "different_mode")
        self.assertTrue(result["bridge_evidence"])

    def test_literal_dylib_load_message_rejects_diagnostic_success(self):
        result = session_log.analyze_log(line(ENTRY) + "06:35:00 [INFO] Loading dynamic library ./de_client_bridge.dll\n" +
                                         line(COMPLETE))
        self.assertEqual(result["status"], "unexpected_diagnostic_activity")
        self.assertEqual(result["bridge_evidence"][0]["event"], "dynamic_library_load")

    def test_old_literal_dylib_message_does_not_taint_later_diagnostic(self):
        result = session_log.analyze_log("06:34:00 [INFO] Loading dynamic library ./de_client_bridge.dll\n" + valid_log())
        self.assertTrue(result["diagnostic_complete"])
        self.assertFalse(result["bridge_evidence"])

    def test_active_gameplay_evidence_rejects_diagnostic(self):
        result = session_log.analyze_log(valid_log() + line({"event": "local_transform", "game_adapter_active": True}))
        self.assertEqual(result["status"], "unexpected_diagnostic_activity")

    def test_shutdown_evidence_is_optional_and_scoped_to_latest_entry(self):
        shutdown = '06:36:00 [INFO] New event received: "host shut down"\n06:36:00 [INFO] Exit\n'
        result = session_log.analyze_log(valid_log() + shutdown)
        self.assertEqual(result["shutdown_evidence"], {"host_shutdown": True, "runtime_exit": True})
        self.assertTrue(result["diagnostic_complete"])
        later = session_log.analyze_log(valid_log() + shutdown + line(ENTRY))
        self.assertEqual(later["shutdown_evidence"], {"host_shutdown": False, "runtime_exit": False})

    def test_malformed_and_unrelated_json_are_ignored(self):
        noise = '06:35:00 [INFO] {broken\n["DE Onli"] "{\\\"event\\\":\n' + line({"other": "value"})
        self.assertEqual(session_log.analyze_log(noise)["status"], "incomplete")

    def test_log_fields_cannot_forge_line_order_or_crash_phase_check(self):
        result = session_log.analyze_log(line(ENTRY) + line({"event": "adapter_phase", "phase": []}) +
                                         line({**COMPLETE, "line": "forged"}))
        self.assertTrue(result["diagnostic_complete"])


class FileAndWatchTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "cleo_redux.log"

    def write(self, text):
        self.path.write_text(text, encoding="utf-8")

    def watch(self, update=None, seconds=2):
        reports = []
        elapsed = [0.0]
        def sleep(duration):
            elapsed[0] += duration
            if update is not None and elapsed[0] == 1:
                update()
        code = session_log.watch_log(self.path, seconds, emit=reports.append,
                                     clock=lambda: elapsed[0], sleep=sleep)
        return code, reports

    def test_one_shot_reports_success_without_freshness_claim(self):
        self.write(valid_log())
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(session_log.main(["--log", str(self.path)]), 0)
        report = json.loads(output.getvalue())
        self.assertEqual(report["evidence_scope"], "entire_log")
        self.assertIn("do not establish", report["notice"])

    def test_missing_log_returns_nonzero_json(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(session_log.main(["--log", str(self.path)]), 2)
        self.assertEqual(json.loads(output.getvalue())["status"], "read_error")

    def test_log_read_cap(self):
        with self.path.open("wb") as stream:
            stream.truncate(session_log.MAX_LOG_BYTES + 1)
        with self.assertRaisesRegex(ValueError, "16 MiB"):
            session_log.read_snapshot(self.path)

    def test_stale_success_never_passes_watch(self):
        self.write(valid_log())
        code, reports = self.watch()
        self.assertEqual(code, 2)
        self.assertEqual(reports[-1]["status"], "watch_timeout")
        self.assertFalse(any(report["diagnostic_complete"] for report in reports))

    def test_unrelated_append_cannot_reactivate_old_markers(self):
        old = valid_log()
        self.write(old)
        code, reports = self.watch(lambda: self.write(old + "06:36:00 [INFO] Still running\n"))
        self.assertEqual(code, 2)
        self.assertFalse(any(report["diagnostic_complete"] for report in reports))

    def test_fresh_entry_and_completion_after_baseline_pass(self):
        old = valid_log()
        self.write(old)
        code, reports = self.watch(lambda: self.write(old + valid_log("06:36:00")))
        self.assertEqual(code, 0)
        self.assertEqual(reports[-1]["evidence_scope"], "fresh_content")

    def test_entry_before_watch_and_completion_after_watch_cannot_pass(self):
        old = line(ENTRY)
        self.write(old)
        code, reports = self.watch(lambda: self.write(old + line(COMPLETE)))
        self.assertEqual(code, 2)
        self.assertFalse(any(report["diagnostic_complete"] for report in reports))

    def test_log_replacement_with_new_timestamps_passes(self):
        self.write(valid_log())
        code, reports = self.watch(lambda: self.write(valid_log("06:36:00")))
        self.assertEqual(code, 0)
        self.assertTrue(reports[-1]["diagnostic_complete"])

    def test_rewritten_noise_cannot_reactivate_unchanged_old_markers(self):
        old = valid_log()
        self.write("old header\n" + old)
        code, reports = self.watch(lambda: self.write("new header\n" + old))
        self.assertEqual(code, 2)
        self.assertFalse(any(report["diagnostic_complete"] for report in reports))

    def test_missing_at_baseline_new_log_can_pass(self):
        code, reports = self.watch(lambda: self.write(valid_log()))
        self.assertEqual(code, 0)
        self.assertIsNone(reports[0]["baseline_fingerprint"])

    def test_fresh_failure_stops_watch(self):
        self.write("")
        code, reports = self.watch(lambda: self.write(line(ENTRY) + line({"event": "adapter_failed", "phase": "entry"})))
        self.assertEqual(code, 2)
        self.assertEqual(reports[-1]["status"], "adapter_failed")

    def test_partial_line_at_baseline_is_not_a_fresh_entry(self):
        old = line(ENTRY).rstrip("\n")
        self.write(old)
        code, reports = self.watch(lambda: self.write(old + "\n" + line(COMPLETE)))
        self.assertEqual(code, 2)
        self.assertFalse(any(report["diagnostic_complete"] for report in reports))

    def test_watch_argument_is_bounded(self):
        self.assertEqual(session_log.bounded_seconds("1"), 1)
        self.assertEqual(session_log.bounded_seconds("600"), 600)
        for value in ("0", "601", "3.5", "bad"):
            with self.subTest(value=value), self.assertRaises(Exception):
                session_log.bounded_seconds(value)


if __name__ == "__main__":
    unittest.main()
