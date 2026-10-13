"""Fixture-only remote lifecycle checks; no game, DLL, or server execution."""

import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

import remote_session_log as reader


ENTRY = {"event": "adapter_phase", "phase": "entry", "mode": "online"}
JOIN = {"event": "joined", "player_id": 1}
CREATE = {"event": "remote_ped_created", "player_id": 2, "ped": 20}
FIRST = {"event": "remote_transform_applied", "player_id": 2, "ped": 20,
         "x": 100.0, "y": -1707.0, "z": 13.5, "heading": 0.0}
SECOND = {**FIRST, "x": 101.0, "heading": 20.0}
DELETE = {"event": "remote_ped_deleted", "player_id": 2}
STOP = {"event": "adapter_stopped", "reason": "Ctrl+Shift+End"}
SHUTDOWN = '[INFO] New event received: "host shut down"'
EXIT = "[INFO] Exit"


def log(events, stamp="13:40:00", quoted=True):
    lines = []
    for event in events:
        if isinstance(event, str):
            message = event
        else:
            message = json.dumps(event)
            if quoted:
                message = json.dumps(message)
            message = '[INFO] ["DE Onli"] ' + message
        lines.append(stamp + " " + message)
    return "\n".join(lines) + "\n"


def valid_events():
    return [ENTRY, JOIN, CREATE, FIRST, SECOND, DELETE, STOP, SHUTDOWN, EXIT]


class LifecycleTests(unittest.TestCase):
    def result(self, events):
        return reader.analyze_log(log(events))

    def assert_rejected(self, events):
        result = self.result(events)
        self.assertFalse(result["remote_command_sequence_passed"])
        self.assertFalse(result["rendering_verified"])
        self.assertFalse(result["multiplayer_verified"])
        return result

    def test_quoted_and_plain_cleo_json_pass_only_command_sequence(self):
        for quoted in (True, False):
            result = reader.analyze_log(log(valid_events(), quoted=quoted))
            self.assertTrue(result["remote_command_sequence_passed"])
            self.assertTrue(all(result["flags"].values()))
            self.assertFalse(result["rendering_verified"])
            self.assertFalse(result["multiplayer_verified"])
            self.assertIn("already absent", result["notice"])

    def test_latest_entry_discards_previous_pass(self):
        result = self.assert_rejected(valid_events() + [ENTRY])
        self.assertEqual(result["status"], "incomplete")
        self.assertEqual(result["completed_lifecycle_count"], 0)

    def test_wrong_mode_cannot_pass(self):
        self.assert_rejected([{**ENTRY, "mode": "smoke"}] + valid_events()[1:])

    def test_startup_only_is_incomplete(self):
        self.assertEqual(self.assert_rejected([ENTRY])["status"], "incomplete")

    def test_synthetic_peer_without_adapter_entry_is_not_game_evidence(self):
        self.assert_rejected([{"event": "joined", "synthetic": True, "player_id": 2},
                              CREATE, FIRST, SECOND, DELETE, STOP, SHUTDOWN, EXIT])

    def test_creation_requires_join(self):
        self.assert_rejected([ENTRY] + valid_events()[2:])

    def test_own_player_cannot_be_a_remote(self):
        self.assert_rejected([ENTRY, {**JOIN, "player_id": 2}] + valid_events()[2:])

    def test_invalid_integer_identities_reject(self):
        for field in ("player_id", "ped"):
            for value in (True, False, 0, -1, 1.5, "2", None, 0x100000000):
                with self.subTest(field=field, value=value):
                    self.assert_rejected([ENTRY, JOIN, {**CREATE, field: value}] + valid_events()[3:])

    def test_invalid_join_rejects(self):
        for value in (True, 0, -1, "1", None, 0x100000000):
            self.assert_rejected([ENTRY, {**JOIN, "player_id": value}] + valid_events()[2:])

    def test_transform_before_creation_rejects(self):
        self.assert_rejected([ENTRY, JOIN, FIRST, CREATE, FIRST, SECOND, DELETE, STOP, SHUTDOWN, EXIT])

    def test_transform_identity_cannot_change(self):
        for change in ({"player_id": 3}, {"ped": 21}, {"player_id": True}, {"ped": True}):
            self.assert_rejected([ENTRY, JOIN, CREATE, FIRST, {**SECOND, **change}, DELETE, STOP, SHUTDOWN, EXIT])

    def test_transform_components_are_finite_numeric_and_bounded(self):
        for component in reader.COMPONENTS:
            for value in (float("nan"), float("inf"), -float("inf"), 100001, -100001, True, "1", None):
                with self.subTest(component=component, value=value):
                    self.assert_rejected([ENTRY, JOIN, CREATE, FIRST, {**SECOND, component: value},
                                          DELETE, STOP, SHUTDOWN, EXIT])
            missing = dict(SECOND)
            del missing[component]
            self.assert_rejected([ENTRY, JOIN, CREATE, FIRST, missing, DELETE, STOP, SHUTDOWN, EXIT])

    def test_single_transform_does_not_prove_movement(self):
        self.assert_rejected([ENTRY, JOIN, CREATE, FIRST, DELETE, STOP, SHUTDOWN, EXIT])

    def test_arbitrarily_large_integer_is_rejected_without_overflow(self):
        self.assert_rejected([ENTRY, JOIN, CREATE, FIRST, {**SECOND, "x": 10 ** 400},
                              DELETE, STOP, SHUTDOWN, EXIT])

    def test_repeated_identical_transform_does_not_prove_movement(self):
        self.assert_rejected([ENTRY, JOIN, CREATE, FIRST, FIRST, DELETE, STOP, SHUTDOWN, EXIT])

    def test_heading_change_alone_is_movement_evidence(self):
        self.assertTrue(self.result([ENTRY, JOIN, CREATE, FIRST, {**FIRST, "heading": 30},
                                     DELETE, STOP, SHUTDOWN, EXIT])["remote_command_sequence_passed"])

    def test_wrong_or_duplicate_deletion_rejects(self):
        for deletion in ({**DELETE, "player_id": 3}, {**DELETE, "ped": 21}, {**DELETE, "ped": True}):
            self.assert_rejected(valid_events()[:5] + [deletion] + valid_events()[6:])
        self.assert_rejected(valid_events()[:6] + [DELETE] + valid_events()[6:])

    def test_optional_matching_deletion_ped_is_accepted(self):
        self.assertTrue(self.result(valid_events()[:5] + [{**DELETE, "ped": 20}] +
                                     valid_events()[6:])["remote_command_sequence_passed"])

    def test_stop_before_cleanup_rejects(self):
        self.assert_rejected(valid_events()[:5] + [STOP, DELETE, SHUTDOWN, EXIT])

    def test_stop_shortcut_must_match(self):
        self.assert_rejected(valid_events()[:6] + [{**STOP, "reason": "F11"}, SHUTDOWN, EXIT])

    def test_shutdown_and_exit_are_ordered_after_stop(self):
        for tail in ([SHUTDOWN, STOP, EXIT], [STOP, EXIT, SHUTDOWN], [STOP, EXIT], [STOP, SHUTDOWN]):
            self.assert_rejected(valid_events()[:6] + tail)

    def test_duplicates_reject(self):
        for position, duplicate in ((2, JOIN), (3, CREATE), (7, STOP), (8, SHUTDOWN), (9, EXIT)):
            events = valid_events()
            events.insert(position, duplicate)
            self.assert_rejected(events)

    def test_adapter_and_cleanup_failures_gate_acceptance(self):
        for kind in ("adapter_failed", "cleanup_failed"):
            result = self.assert_rejected(valid_events() + [{"event": kind, "error": "fixture"}])
            self.assertEqual(result["status"], "adapter_failed")

    def test_old_failure_does_not_taint_new_complete_entry(self):
        self.assertTrue(self.result([ENTRY, {"event": "cleanup_failed"}] +
                                     valid_events())["remote_command_sequence_passed"])

    def test_separate_ped_lifecycles_cannot_combine_transforms(self):
        events = [ENTRY, JOIN, CREATE, FIRST, DELETE, {**CREATE, "ped": 21},
                  {**SECOND, "ped": 21}, DELETE, STOP, SHUTDOWN, EXIT]
        self.assert_rejected(events)

    def test_complete_separate_lifecycles_can_pass(self):
        cycle = [CREATE, FIRST, SECOND, DELETE]
        self.assertTrue(self.result([ENTRY, JOIN] + cycle + cycle +
                                     [STOP, SHUTDOWN, EXIT])["remote_command_sequence_passed"])

    def test_reconnections_cannot_combine_transforms(self):
        self.assert_rejected([ENTRY, JOIN, CREATE, FIRST, DELETE, {"event": "transport_lost"},
                              JOIN, CREATE, SECOND, DELETE, STOP, SHUTDOWN, EXIT])

    def test_reconnect_with_live_ped_rejects(self):
        for kind in ("connecting", "transport_lost"):
            self.assert_rejected(valid_events()[:5] + [{"event": kind}] + valid_events()[5:])

    def test_complete_reconnected_lifecycles_pass(self):
        cycle = [CREATE, FIRST, SECOND, DELETE]
        self.assertTrue(self.result([ENTRY, JOIN] + cycle + [{"event": "transport_lost"}, JOIN] +
                                     cycle + [STOP, SHUTDOWN, EXIT])["remote_command_sequence_passed"])

    def test_two_player_ids_cannot_own_same_ped(self):
        self.assert_rejected([ENTRY, JOIN, CREATE, {**CREATE, "player_id": 3}] + valid_events()[3:])


class FileTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / "cleo.log"
        self.baseline = Path(self.temporary.name) / "baseline.log"

    def cli(self, arguments):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream):
            code = reader.main(arguments)
        return code, json.loads(stream.getvalue())

    def test_cli_pass_and_fingerprint(self):
        self.path.write_text(log(valid_events()), encoding="utf-8")
        code, result = self.cli(["--log", str(self.path)])
        self.assertEqual(code, 0)
        self.assertEqual(len(result["log_fingerprint"]["sha256"]), 64)

    def test_baseline_excludes_old_pass_after_noise_append(self):
        old = log(valid_events())
        self.baseline.write_text(old, encoding="utf-8")
        self.path.write_text(old + "13:41:00 [INFO] unrelated\n", encoding="utf-8")
        code, result = self.cli(["--log", str(self.path), "--baseline", str(self.baseline)])
        self.assertEqual(code, 2)
        self.assertEqual(result["evidence_scope"], "fresh_content")

    def test_baseline_allows_fresh_complete_session(self):
        old = log(valid_events())
        self.baseline.write_text(old, encoding="utf-8")
        self.path.write_text(old + log(valid_events(), "13:41:00"), encoding="utf-8")
        self.assertEqual(self.cli(["--log", str(self.path), "--baseline", str(self.baseline)])[0], 0)

    def test_rotated_log_cannot_reuse_unchanged_old_markers(self):
        old = log(valid_events())
        self.baseline.write_text("old header\n" + old, encoding="utf-8")
        self.path.write_text("new header\n" + old, encoding="utf-8")
        self.assertEqual(self.cli(["--log", str(self.path), "--baseline", str(self.baseline)])[0], 2)

    def test_rotated_log_with_fresh_session_passes(self):
        self.baseline.write_text(log(valid_events()), encoding="utf-8")
        self.path.write_text(log(valid_events(), "13:41:00"), encoding="utf-8")
        self.assertEqual(self.cli(["--log", str(self.path), "--baseline", str(self.baseline)])[0], 0)

    def test_entry_present_before_baseline_cannot_be_completed_later(self):
        self.baseline.write_text(log([ENTRY]), encoding="utf-8")
        self.path.write_text(log(valid_events()), encoding="utf-8")
        self.assertEqual(self.cli(["--log", str(self.path), "--baseline", str(self.baseline)])[0], 2)

    def test_missing_log_or_baseline_returns_read_error(self):
        for arguments in (["--log", str(self.path)], ["--log", str(self.path), "--baseline", str(self.baseline)]):
            code, result = self.cli(arguments)
            self.assertEqual(code, 2)
            self.assertEqual(result["status"], "read_error")

    def test_oversized_log_is_rejected(self):
        with self.path.open("wb") as stream:
            stream.truncate(16 * 1024 * 1024 + 1)
        code, result = self.cli(["--log", str(self.path)])
        self.assertEqual(code, 2)
        self.assertEqual(result["status"], "read_error")


if __name__ == "__main__":
    unittest.main()
