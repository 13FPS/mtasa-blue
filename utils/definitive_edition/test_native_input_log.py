"""Saved synthetic input evidence only; no game, DLL, process or input actions."""

import contextlib
from copy import deepcopy
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest

import native_input_log as reader


EPOCH = 1000000
PED = 17
MASK = 0xFFFFF
UNKNOWN = ["m_bChatIndicated", "m_bPedWalk", "m_bVehicleMouseLook", "m_bRadioTrackSkip"]


def event(kind, at=EPOCH, **fields):
    return {"event": kind, "reference_id": "input-" + str(EPOCH) + "-" + str(PED),
            "ped": PED, "at_ms": at, "elapsed_ms": at - EPOCH, **fields}


def fixture(count=4, spacing=100, reason="Ctrl+Shift+End", finish_at=None):
    records = [{"event": "adapter_phase", "phase": "entry", "mode": "smoke"},
               event("native_input_reference_reset", reason="start", current_mask=0, previous_mask=0),
               event("native_input_reference_started", sample_interval_ms=100, budget_ms=30000,
                     max_samples=300, known_field_count=20, unknown_fields=UNKNOWN[:], velocity_units=None,
                     network_active=False, input_writes=False, remote_peds_created=False)]
    previous = None
    for index in range(count):
        current = [0] * 20
        if index:
            current[1] = 200
        if index >= 2:
            current[16] = 1
        records.append(event("local_controller_sample", EPOCH + index * spacing,
                             current=current, previous=previous, current_mask=MASK,
                             previous_mask=0 if previous is None else MASK,
                             previous_source="previous_probe_sample",
                             previous_at_ms=None if previous is None else EPOCH + (index - 1) * spacing,
                             unknown_fields=UNKNOWN[:], velocity_raw={"x": 0, "y": 0.003 if index else 0, "z": 0},
                             velocity_units=None, position={"x": -40 + index * 0.1, "y": 23, "z": 3.5, "heading": 270}))
        previous = current[:]
    at = finish_at if finish_at is not None else EPOCH + max(0, count - 1) * spacing
    records.extend([event("native_input_reference_finished", at, reason=reason, outcome="observation_only", samples=count),
                    event("native_input_reference_reset", at, reason="cleanup", current_mask=0, previous_mask=0),
                    event("native_input_reference_stopped", at, reason=reason, samples=count,
                          network_active=False, input_writes=False)])
    return deepcopy(records)


def encode(records, quoted=True, exit_markers=False):
    lines = []
    for record in records:
        message = json.dumps(record)
        lines.append('12:40:00 [INFO] ["DE Smoke"] ' + (json.dumps(message) if quoted else message))
    if exit_markers:
        lines.extend(['12:41:00 [INFO] New event received: "host shut down"', '12:41:00 [INFO] Exit'])
    return "\n".join(lines) + "\n"


def samples(records):
    return [entry for entry in records if entry["event"] == "local_controller_sample"]


def find(records, kind):
    return next(entry for entry in records if entry["event"] == "native_input_reference_" + kind)


class NativeInputReaderTests(unittest.TestCase):
    def result(self, records, require_exit=False, exit_markers=False):
        return reader.analyze_log(encode(records, exit_markers=exit_markers), require_exit=require_exit)

    def reject(self, records):
        result = self.result(records)
        self.assertFalse(result["controller_reference_observed"])
        self.assertFalse(result["native_walk_jog_states_verified"])
        self.assertFalse(result["velocity_units_verified"])
        self.assertFalse(result["full_backend_verified"])
        return result

    def test_golden_quoted_and_plain_logs_preserve_storage_and_probe_provenance(self):
        for quoted in (True, False):
            report = reader.analyze_log(encode(fixture(), quoted))
            self.assertTrue(report["controller_reference_observed"], report["issues"])
            self.assertEqual(report["accepted_sample_count"], 4)
            self.assertEqual(report["accepted_samples"][1]["current"][1], 200)
            self.assertEqual(report["accepted_samples"][2]["previous_at_ms"], EPOCH + 100)
            self.assertTrue(report["observations"]["analog_nonzero_observed"])
            self.assertTrue(report["observations"]["engine_velocity_nonzero_observed"])
            self.assertTrue(report["observations"]["sprint_button_16_nonzero_observed"])
            self.assertFalse(report["native_walk_jog_states_verified"])
            self.assertEqual(report["observations"]["sprint_button_index"], 16)

    def test_idle_data_is_valid_without_inventing_movement_or_sprint(self):
        records = fixture()
        for entry in samples(records):
            entry.update(current=[0] * 20, previous=None if entry["previous"] is None else [0] * 20,
                         velocity_raw={"x": 0, "y": 0, "z": 0})
        report = self.result(records)
        self.assertTrue(report["controller_reference_observed"])
        for key in ("analog_nonzero_observed", "engine_velocity_nonzero_observed", "sprint_button_16_nonzero_observed"):
            self.assertFalse(report["observations"][key])

    def test_signed_int16_endpoints_are_storage_values_not_assumed_pad_ranges(self):
        records = fixture()
        for index, entry in enumerate(samples(records)):
            entry["current"] = [-32768, 32767] + [0] * 18
            entry["previous"] = None if index == 0 else samples(records)[index - 1]["current"][:]
        self.assertTrue(self.result(records)["controller_reference_observed"])

    def test_bad_current_storage_types_lengths_and_out_of_range_are_rejected(self):
        for value in (None, {}, [0] * 19, [0] * 21, [True] + [0] * 19, [0.0] + [0] * 19,
                      ["0"] + [0] * 19, [-32769] + [0] * 19, [32768] + [0] * 19):
            with self.subTest(value=value):
                records = fixture(); samples(records)[1]["current"] = value
                self.reject(records)

    def test_current_mask_requires_exact_20_known_bits_and_integer_type(self):
        for value in (0, MASK - 1, MASK | (1 << 20), -1, True, float(MASK), "0xfffff"):
            records = fixture(); samples(records)[1]["current_mask"] = value
            self.reject(records)

    def test_first_previous_requires_null_values_null_clock_and_zero_mask(self):
        for field, value in (("previous", [0] * 20), ("previous_mask", MASK), ("previous_mask", False),
                             ("previous_at_ms", EPOCH), ("previous_at_ms", False)):
            records = fixture(); samples(records)[0][field] = value
            self.reject(records)

    def test_later_previous_must_equal_immediately_prior_probe_sample(self):
        for field, value in (("previous", None), ("previous", [1] * 20), ("previous_mask", 0),
                             ("previous_mask", float(MASK)), ("previous_at_ms", EPOCH),
                             ("previous_at_ms", EPOCH + 101), ("previous_source", "previous_game_frame")):
            records = fixture(); samples(records)[2][field] = value
            self.reject(records)

    def test_previous_storage_types_cannot_hide_behind_numeric_equality(self):
        for value in (True, 0.0, "0", 32768, -32769):
            records = fixture(); samples(records)[1]["previous"][0] = value
            self.reject(records)

    def test_previous_source_is_required_on_the_first_sample_too(self):
        records = fixture(); samples(records)[0].pop("previous_source")
        self.reject(records)

    def test_exact_unknown_logical_fields_stay_unpublished(self):
        for unknown in ([], UNKNOWN[:-1], UNKNOWN + ["m_bPedSprint"], UNKNOWN[:-1] + [UNKNOWN[0]],
                        {name: False for name in UNKNOWN}, "unknown"):
            for target in ("started", "sample"):
                records = fixture()
                entry = find(records, target) if target != "sample" else samples(records)[1]
                entry["unknown_fields"] = unknown
                self.reject(records)

    def test_fake_unknown_field_values_and_gait_classifications_are_rejected(self):
        for field, value in (("m_bPedWalk", False), ("m_bChatIndicated", None), ("native_walk_state", "jog")):
            records = fixture(); samples(records)[1][field] = value
            self.reject(records)

    def test_velocity_units_must_remain_explicitly_null(self):
        for target in (find, samples):
            for value in ("units_per_second", "engine_ticks", 1, False):
                records = fixture()
                entry = find(records, "started") if target is find else samples(records)[1]
                entry["velocity_units"] = value
                self.reject(records)

    def test_velocity_and_position_require_finite_bounded_numeric_components(self):
        for name in ("velocity_raw", "position"):
            # Stay below Python's JSON digit cap while exceeding float range;
            # this exercises the reader's numeric bound rather than the encoder.
            for value in (float("nan"), float("inf"), True, "1", 100001, -(1 << 10000)):
                records = fixture(); samples(records)[1][name]["x"] = value
                self.reject(records)
            records = fixture(); samples(records)[1][name].pop("z")
            self.reject(records)
        records = fixture(); samples(records)[1]["position"]["heading"] = float("nan")
        self.reject(records)

    def test_sample_fields_cannot_be_missing_or_smuggled_as_fake_metadata(self):
        for field in ("current", "previous", "unknown_fields", "velocity_units", "position", "previous_at_ms"):
            records = fixture(); samples(records)[1].pop(field)
            self.reject(records)

    def test_reference_and_local_ped_ownership_cannot_change(self):
        for field, value in (("reference_id", "foreign"), ("ped", PED + 1), ("ped", True),
                             ("ped", 0), ("ped", 0x100000000)):
            records = fixture(); samples(records)[1][field] = value
            self.reject(records)
        records = fixture(); find(records, "started")["reference_id"] = "input-forged"
        self.reject(records)

    def test_mask_zero_start_reset_is_required_before_first_publication(self):
        records = fixture(); records.pop(1)
        self.reject(records)
        records = fixture(); records[1], records[2] = records[2], records[1]
        self.reject(records)
        for index in (1, -2):
            for field in ("current_mask", "previous_mask"):
                records = fixture(); records[index][field] = MASK
                self.reject(records)

    def test_cleanup_reset_must_follow_finish_and_precede_stop(self):
        records = fixture(); records.pop(-2)
        self.reject(records)
        records = fixture(); records[-3], records[-2] = records[-2], records[-3]
        self.reject(records)
        records = fixture(); records.append(records.pop(-2))
        self.reject(records)

    def test_first_sample_clock_equals_the_started_probe_clock(self):
        records = fixture(); samples(records)[0].update(at_ms=EPOCH + 1, elapsed_ms=1)
        self.reject(records)

    def test_clock_storage_elapsed_identity_and_sample_gap_are_strict(self):
        for field, value in (("at_ms", True), ("at_ms", EPOCH + 100.0), ("at_ms", -1),
                             ("at_ms", 9007199254740992), ("elapsed_ms", 101), ("elapsed_ms", True)):
            records = fixture(); samples(records)[1][field] = value
            self.reject(records)
        self.reject(fixture(spacing=99))
        self.reject(fixture(spacing=251))
        self.assertTrue(self.result(fixture(spacing=250))["controller_reference_observed"])

    def test_duplicate_sample_and_events_after_finish_are_rejected(self):
        records = fixture(); records.insert(4, deepcopy(samples(records)[0]))
        self.reject(records)
        records = fixture(); records.insert(-2, deepcopy(samples(records)[-1]))
        self.reject(records)
        records = fixture(); records.append(deepcopy(samples(records)[-1]))
        self.reject(records)

    def test_started_policy_is_fixed_and_read_only(self):
        for field, value in (("sample_interval_ms", 50), ("budget_ms", 30001), ("max_samples", 301),
                             ("known_field_count", 19), ("network_active", True), ("input_writes", True),
                             ("remote_peds_created", True), ("network_active", 0)):
            records = fixture(); find(records, "started")[field] = value
            self.reject(records)

    def test_online_transport_npc_and_input_write_activity_is_disallowed(self):
        for kind in ("connecting", "joined", "online_transform", "remote_ped_created", "remote_animation_requested"):
            records = fixture(); records.insert(3, {"event": kind})
            self.reject(records)
        for flag in ("network_active", "input_writes", "remote_peds_created", "game_input_sent_by_tools"):
            records = fixture(); records.insert(3, {"event": "adapter_start", "mode": "smoke", flag: True})
            self.reject(records)

    def test_adapter_start_when_present_must_identify_the_isolated_input_probe(self):
        start = {"event": "adapter_start", "mode": "smoke", "endpoint": None,
                 "native_input_reference": True, "network_active": False,
                 "input_writes": False, "remote_peds_created": False}
        records = fixture(); records.insert(1, start)
        self.assertTrue(self.result(records)["controller_reference_observed"])
        for field, value in (("endpoint", "127.0.0.1:22040"), ("native_input_reference", False),
                             ("mode", "diagnostic"), ("network_active", 0)):
            records = fixture(); records.insert(1, {**start, field: value})
            self.reject(records)
        records = fixture(); records.insert(1, start); records.insert(2, start)
        self.reject(records)

    def test_one_smoke_entry_required_and_no_hidden_restart(self):
        records = fixture(); records[0]["mode"] = "online"
        self.reject(records)
        self.reject(fixture()[1:])
        records = fixture(); records.append(deepcopy(records[0]))
        self.reject(records)

    def test_missing_required_lifecycle_stays_incomplete(self):
        for kind in ("started", "finished", "stopped"):
            records = [entry for entry in fixture() if entry["event"] != "native_input_reference_" + kind]
            self.reject(records)
        self.reject(fixture(count=0))

    def test_null_identity_cleanup_before_ready_is_incomplete_not_a_valid_reference(self):
        records = [fixture()[0], {"event": "native_input_reference_reset", "reference_id": None, "ped": None,
                                  "at_ms": EPOCH, "elapsed_ms": None, "reason": "cleanup", "current_mask": 0, "previous_mask": 0},
                   {"event": "native_input_reference_stopped", "reference_id": None, "ped": None, "at_ms": EPOCH,
                    "elapsed_ms": None, "reason": "loading", "samples": 0, "network_active": False, "input_writes": False}]
        self.assertEqual(self.reject(records)["status"], "incomplete")

    def test_finish_counts_reason_and_observation_only_outcome_are_verified(self):
        for kind in ("finished", "stopped"):
            records = fixture(); find(records, kind)["samples"] += 1
            self.reject(records)
        records = fixture(); find(records, "stopped")["reason"] = "time_budget"
        self.reject(records)
        records = fixture(); find(records, "finished")["outcome"] = "game_backend_verified"
        self.reject(records)

    def test_normal_time_and_sample_budgets_are_bounded(self):
        self.assertTrue(self.result(fixture(count=300, reason="sample_budget"))["controller_reference_observed"])
        self.assertTrue(self.result(fixture(count=200, spacing=150, reason="time_budget", finish_at=EPOCH + 30000))["controller_reference_observed"])
        self.reject(fixture(reason="sample_budget"))
        self.reject(fixture(reason="time_budget"))
        self.reject(fixture(count=301, reason="sample_budget"))
        records = fixture(); samples(records)[-1].update(at_ms=EPOCH + 30000, elapsed_ms=30000)
        self.reject(records)

    def test_abnormal_completion_never_becomes_successful_movement_reference(self):
        for reason in ("loading", "local_ped_missing", "local_ped_changed", "clock_reversed", "render_gap", "error", "script_exit"):
            result = self.reject(fixture(reason=reason))
            self.assertIn(result["status"], ("incomplete", "adapter_failed"))

    def test_adapter_or_cleanup_failure_cannot_be_hidden_by_later_stop(self):
        for kind in ("adapter_failed", "cleanup_failed"):
            records = fixture(); records.insert(-1, event(kind, EPOCH + 300, error="readback failed"))
            report = self.reject(records)
            self.assertEqual(report["status"], "adapter_failed")
            self.assertTrue(report["failures"])

    def test_optional_exit_requires_ordered_host_shutdown_then_exit_after_stop(self):
        self.assertTrue(self.result(fixture())["controller_reference_observed"])
        self.assertFalse(self.result(fixture(), require_exit=True)["controller_reference_observed"])
        self.assertTrue(self.result(fixture(), require_exit=True, exit_markers=True)["controller_reference_observed"])
        text = encode(fixture()) + '12:41:00 [INFO] Exit\n12:41:00 [INFO] New event received: "host shut down"\n'
        self.assertFalse(reader.analyze_log(text, require_exit=True)["controller_reference_observed"])

    def test_malformed_unknown_and_capped_reference_events_do_not_get_ignored(self):
        text = encode(fixture()) + '{"event":"local_controller_sample"\n'
        self.assertFalse(reader.analyze_log(text)["controller_reference_observed"])
        records = fixture(); records.insert(-1, event("native_input_reference_mystery", EPOCH + 300))
        self.reject(records)
        records = fixture() + [deepcopy(samples(fixture())[0]) for _ in range(reader.MAX_EVENTS)]
        report = self.reject(records)
        self.assertEqual(len(report["events"]), reader.MAX_EVENTS)
        with self.assertRaises(ValueError):
            reader.analyze_log(" " * (reader.MAX_LOG_BYTES + 1))


class NativeInputCommandTests(unittest.TestCase):
    def test_cli_preserves_entire_saved_log_and_sha256_with_optional_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "game.log", Path(directory) / "assessment.json"
            source.write_text(encode(fixture(), exit_markers=True), encoding="utf-8")
            before = source.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(output), "--require-exit"])
            self.assertEqual(code, 0)
            self.assertEqual(source.read_bytes(), before)
            report = json.loads(output.read_text())
            self.assertEqual(report["evidence_scope"], "entire_log")
            self.assertEqual(report["log_fingerprint"]["sha256"], hashlib.sha256(before).hexdigest())
            self.assertEqual(report["source_line_origin"], str(source.resolve()))

    def test_cli_rejects_raw_overwrite_by_same_path_or_hard_link(self):
        with tempfile.TemporaryDirectory() as directory:
            source, alias = Path(directory) / "game.log", Path(directory) / "alias.log"
            source.write_text(encode(fixture()), encoding="utf-8")
            os.link(source, alias)
            before = source.read_bytes()
            for target in (source, alias):
                with contextlib.redirect_stdout(io.StringIO()):
                    code = reader.main(["--log", str(source), "--output", str(target)])
                self.assertEqual(code, 2)
                self.assertEqual(source.read_bytes(), before)

    def test_missing_and_oversized_saved_log_emit_read_error(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "game.log", Path(directory) / "assessment.json"
            for oversized in (False, True):
                if oversized:
                    with source.open("wb") as stream:
                        stream.truncate(reader.MAX_LOG_BYTES + 1)
                with contextlib.redirect_stdout(io.StringIO()):
                    code = reader.main(["--log", str(source), "--output", str(output)])
                self.assertEqual(code, 2)
                self.assertEqual(json.loads(output.read_text())["status"], "read_error")


if __name__ == "__main__":
    unittest.main()
