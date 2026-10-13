"""Synthetic rate/cadence evidence; no native calls, game, server or UI."""

import contextlib
from copy import deepcopy
import io
import json
from pathlib import Path
import tempfile
import unittest

import gait_rate_log as reader


EPOCH = 1000000
PLAYER, PED = 6, 100
COMMON = {"player_id": PLAYER, "ped": PED}
TASKS = [(0, "idle"), (2000, "walk"), (14400, "idle"), (16000, "walk"),
         (22400, "idle"), (24000, "walk"), (28400, "idle")]
WALKS = {2000: (0.5, 12000), 16000: (-1.0, 6000), 24000: (1.5, 4000)}


def value(kind, **fields):
    return {"event": kind, **COMMON, **fields}


def position(elapsed):
    if elapsed < 2000:
        x = -3
    elif elapsed <= 14000:
        x = -3 + (elapsed - 2000) * 0.0005
    elif elapsed < 16000:
        x = 3
    elif elapsed <= 22000:
        x = 3 - (elapsed - 16000) * 0.001
    elif elapsed < 24000:
        x = -3
    elif elapsed <= 28000:
        x = -3 + (elapsed - 24000) * 0.0015
    else:
        x = 3
    return {"x": x, "y": 23, "z": 3.5, "heading": 90 if 16000 <= elapsed < 24000 else 270}


def application(task_at, at, speed):
    rate = max(reader.RATE_MIN, min(reader.RATE_MAX, speed / reader.REFERENCE_SPEED))
    return value("remote_animation_rate_applied", sample_at_ms=EPOCH + at,
                 task_requested_at_ms=EPOCH + task_at, task_age_ms=at - task_at,
                 clip="Walk_civi", state="walk", reference_speed=reader.REFERENCE_SPEED,
                 measured_speed=speed, requested_rate=rate, rate_min=0.25, rate_max=2,
                 setter_interval_ms=250, min_rate_delta=0.05, speed_window_ms=300,
                 min_speed_window_ms=250, max_speed_window_ms=500,
                 speed_window_age_ms=0, max_speed_window_age_ms=250,
                 speed_window_started_at_ms=EPOCH + at - 300,
                 speed_window_finished_at_ms=EPOCH + at, speed_sample_count=7,
                 speed_path_distance=speed * 0.3, source="received_target_xy")


def fixture(adapted=True):
    records = [{"event": "adapter_phase", "phase": "entry", "mode": "online"},
               {"event": "joined", "player_id": 5}, value("remote_ped_created", model=7)]
    task_at, state = 0, "idle"
    for elapsed in range(0, 30000, 100):
        if (elapsed, "idle") in TASKS or (elapsed, "walk") in TASKS:
            task_at, state = next(task for task in TASKS if task[0] == elapsed)
            records.append(value("remote_animation_requested", state=state,
                                 animation="Walk_civi" if state == "walk" else "idle_stance",
                                 animation_file="PED", requested_at_ms=EPOCH + elapsed))
        if elapsed == 0:
            records.append(value("remote_gait_probe_started", sample_at_ms=EPOCH,
                                 interval_ms=100, budget_ms=30000, max_samples=300))
        rate = None
        if state == "walk":
            speed, _ = WALKS[task_at]
            rate = abs(speed) / reader.REFERENCE_SPEED if elapsed - task_at >= 300 else None
            if elapsed - task_at == 300:
                records.append(application(task_at, elapsed, abs(speed)))
            age = (elapsed - task_at) / 1000
            phase = 0.2 + reader.BASELINE_CADENCE * (min(age, 0.3) + max(age - 0.3, 0) * (rate if adapted and rate else 1))
        else:
            phase = 0.2 + (elapsed - task_at) / 1500
        pose = position(elapsed)
        records.append(value("remote_gait_sample", sample_at_ms=EPOCH + elapsed,
                             probe_elapsed_ms=elapsed, task_requested_at_ms=EPOCH + task_at,
                             task_age_ms=elapsed - task_at, state=state, animation_file="PED",
                             clip="Walk_civi" if state == "walk" else "idle_stance", playing=True,
                             time=phase % 1, requested_animation_rate=rate,
                             requested_transform=dict(pose), observed_transform=dict(pose), network_target=dict(pose)))
    records.extend([value("remote_gait_probe_finished", sample_at_ms=EPOCH + 30000,
                          probe_elapsed_ms=30000, samples=300, reason="time_budget"),
                    value("remote_ped_deleted"), {"event": "adapter_stopped", "reason": "Ctrl+Shift+End"}])
    return deepcopy(records)


def encode(records, quoted=True, exit_markers=False):
    lines = []
    for record in records:
        message = json.dumps(record)
        lines.append('12:40:00 [INFO] ["DE Onli"] ' + (json.dumps(message) if quoted else message))
    if exit_markers:
        lines.extend(['12:40:01 [INFO] New event received: "host shut down"', '12:40:01 [INFO] Exit'])
    return "\n".join(lines) + "\n"


def matching(records, kind):
    return [record for record in records if record["event"] == kind]


def first_walk_sample(records, age=1500):
    return next(record for record in matching(records, "remote_gait_sample")
                if record["task_requested_at_ms"] == EPOCH + 2000 and record["task_age_ms"] == age)


class RateLogTests(unittest.TestCase):
    def result(self, records, **options):
        return reader.analyze_log(encode(records), **options)

    def reject(self, records):
        result = self.result(records)
        self.assertFalse(result["animation_rate_runtime_verified"])
        self.assertFalse(result["foot_contact_or_no_sliding_verified"])
        self.assertFalse(result["stride_matching_verified"])
        self.assertFalse(result["human_visual_observation_inferred"])
        return result

    def test_golden_three_actual_rates_pass_with_declared_tolerance(self):
        for quoted in (True, False):
            result = reader.analyze_log(encode(fixture(), quoted=quoted))
            self.assertTrue(result["animation_rate_runtime_verified"], result["issues"])
            self.assertEqual(len(result["walking_groups"]), 3)
            for group, speed in zip(result["walking_groups"], (0.5, -1.0, 1.5)):
                fit = group["selected_fit"]
                self.assertGreaterEqual(fit["phase_wraps_observed"], 2)
                expected_rate = abs(speed) / reader.REFERENCE_SPEED
                self.assertAlmostEqual(fit["mean_requested_animation_rate"], expected_rate)
                self.assertAlmostEqual(fit["actual_phase_cycles_per_second_fit"], reader.BASELINE_CADENCE * expected_rate)
                self.assertAlmostEqual(fit["signed_network_x_speed_units_per_second_fit"], speed)
                self.assertTrue(group["actual_cadence_verified"])
            self.assertEqual(result["policy"]["cadence_relative_tolerance"], 0.1)
            self.assertFalse(result["normal_exit_verified"])
            self.assertFalse(result["foot_contact_or_no_sliding_verified"])
            self.assertFalse(result["stride_matching_verified"])

    def test_setter_requests_without_actual_cadence_change_are_not_proof(self):
        result = self.reject(fixture(adapted=False))
        self.assertTrue(result["flags"]["all_three_walk_rates_applied"])
        self.assertFalse(result["flags"]["all_three_actual_cadences_verified"])
        self.assertTrue(any(group["selected_fit"] and not group["selected_fit"]["cadence_within_tolerance"]
                            for group in result["walking_groups"]))

    def test_exit_is_separate_optional_gate(self):
        records = fixture()
        result = reader.analyze_log(encode(records), require_exit=True)
        self.assertFalse(result["animation_rate_runtime_verified"])
        self.assertFalse(result["normal_exit_verified"])
        result = reader.analyze_log(encode(records, exit_markers=True), require_exit=True)
        self.assertTrue(result["animation_rate_runtime_verified"], result["issues"])
        self.assertTrue(result["normal_exit_verified"])

    def test_endpoint_hold_and_task_warmup_do_not_contribute_phase_wraps(self):
        result = self.result(fixture())
        self.assertTrue(result["animation_rate_runtime_verified"])
        self.assertGreater(result["skipped_reason_counts"]["stationary_endpoint_or_pause"], 0)
        for sample in result["selected_samples"]:
            self.assertGreaterEqual(sample["task_age_ms"], 1000)
            self.assertLessEqual(sample["task_age_ms"], WALKS[sample["task_requested_at_ms"] - EPOCH][1])

    def test_missing_task_applied_probe_finish_or_deletion_prevents_acceptance(self):
        for kind in ("remote_animation_requested", "remote_animation_rate_applied", "remote_gait_probe_started",
                     "remote_gait_probe_finished", "remote_ped_deleted"):
            with self.subTest(kind=kind):
                records = fixture()
                removed = matching(records, kind)[0]
                records.remove(removed)
                self.reject(records)

    def test_sample_linkage_and_latest_applied_rate_are_strict(self):
        for field, replacement in (("ped", 101), ("player_id", 7), ("task_requested_at_ms", EPOCH),
                                   ("requested_animation_rate", 1), ("task_age_ms", 1), ("clip", "run_civi")):
            with self.subTest(field=field):
                records = fixture()
                first_walk_sample(records)[field] = replacement
                result = self.reject(records)
                self.assertGreater(result["rejected_sample_count"], 0)

    def test_rate_metadata_must_recompute_path_speed_and_clamped_multiplier(self):
        for field, replacement in (("requested_rate", 1), ("measured_speed", 1), ("speed_path_distance", 5),
                                   ("reference_speed", 1), ("source", "displayed_transform"),
                                   ("speed_window_ms", 100), ("max_speed_window_ms", 30000), ("rate_max", 3)):
            with self.subTest(field=field):
                records = fixture()
                matching(records, "remote_animation_rate_applied")[0][field] = replacement
                self.reject(records)

    def test_new_setter_rate_cannot_be_reused_across_a_different_walk_task(self):
        records = fixture()
        matching(records, "remote_animation_rate_applied")[1]["task_requested_at_ms"] = EPOCH + 2000
        self.reject(records)

    def test_applied_setter_bursts_are_rejected(self):
        records = fixture()
        first = matching(records, "remote_animation_rate_applied")[0]
        duplicate = deepcopy(first)
        duplicate.update(sample_at_ms=first["sample_at_ms"] + 100, task_age_ms=400,
                         speed_window_started_at_ms=first["speed_window_started_at_ms"] + 100,
                         speed_window_finished_at_ms=first["speed_window_finished_at_ms"] + 100)
        records.insert(records.index(first) + 2, duplicate)
        self.reject(records)

    def test_gap_cannot_join_insufficient_fast_cycle_fragments(self):
        records = fixture()
        records = [record for record in records if record["event"] != "remote_gait_sample"
                   or record["sample_at_ms"] not in (EPOCH + 26700, EPOCH + 26800, EPOCH + 26900)]
        matching(records, "remote_gait_probe_finished")[0]["samples"] -= 3
        result = self.reject(records)
        self.assertIsNone(result["walking_groups"][2]["selected_fit"])
        self.assertTrue(any(boundary["reason"] == "sample_gap_over_250ms"
                            for boundary in result["segmentation_boundaries"]))

    def test_insufficient_fast_observed_wraps_remain_incomplete(self):
        records = fixture()
        for sample in matching(records, "remote_gait_sample"):
            if sample["task_requested_at_ms"] == EPOCH + 24000 and sample["task_age_ms"] >= 2200:
                sample.update(playing=False, time=None)
        result = self.reject(records)
        self.assertFalse(result["walking_groups"][2]["actual_cadence_verified"])
        self.assertIsNone(result["walking_groups"][2]["selected_fit"])

    def test_phase_reset_reverse_or_jump_are_rejected_and_retained(self):
        for phase in (0, 0.4, float("nan"), float("inf"), True):
            with self.subTest(phase=phase):
                records = fixture()
                first_walk_sample(records)["time"] = phase
                result = self.reject(records)
                self.assertGreater(result["rejected_sample_count"], 0)

    def test_native_clip_absence_is_not_rate_verification(self):
        records = fixture()
        for sample in matching(records, "remote_gait_sample"):
            if sample["state"] == "walk":
                sample.update(playing=False, time=None)
        result = self.reject(records)
        self.assertEqual(result["skipped_reason_counts"]["clip_unavailable"], 232)

    def test_remote_readback_or_lane_departure_is_rejected(self):
        for field, amount in (("observed_transform", 0.3), ("network_target", 1)):
            records = fixture()
            first_walk_sample(records)[field]["y"] += amount
            self.reject(records)

    def test_restarts_clock_resets_and_own_player_creation_are_rejected(self):
        self.reject(fixture() + fixture())
        records = fixture()
        records.insert(20, value("remote_animation_clock_reset"))
        self.reject(records)
        records = fixture()
        matching(records, "remote_ped_created")[0]["player_id"] = 5
        self.reject(records)

    def test_empty_or_previous_non_opt_in_logs_do_not_pass(self):
        result = self.reject([])
        self.assertIsNone(result["walking_groups"][0]["selected_fit"] if result["walking_groups"] else None)
        records = fixture()
        for record in records:
            record.pop("task_requested_at_ms", None)
            record.pop("requested_at_ms", None)
        self.reject(records)

    def test_cleanup_count_failures_and_missing_shutdown_are_rejected(self):
        records = fixture()
        matching(records, "remote_gait_probe_finished")[0]["samples"] = 299
        self.reject(records)
        records = fixture() + [{"event": "cleanup_failed", "error": "native deletion failed"}]
        result = self.reject(records)
        self.assertEqual(result["status"], "adapter_failed")
        result = reader.analyze_log(encode(fixture()) + '12:40:01 [INFO] Exit\n')
        self.assertFalse(result["animation_rate_runtime_verified"])

    def test_malformed_remote_json_is_not_silently_ignored(self):
        result = reader.analyze_log(encode(fixture()) + 'remote_gait_sample { malformed }\n')
        self.assertFalse(result["animation_rate_runtime_verified"])
        self.assertTrue(any("Malformed" in issue["reason"] for issue in result["issues"]))

    def test_malformed_applied_and_task_clocks_return_invalid_reports(self):
        for field in ("sample_at_ms", "task_requested_at_ms", "speed_window_ms",
                      "speed_window_started_at_ms", "speed_window_finished_at_ms"):
            for replacement in (None, True, "bad", -1, 10 ** 100, float("nan")):
                with self.subTest(field=field, replacement=replacement):
                    records = fixture()
                    matching(records, "remote_animation_rate_applied")[0][field] = replacement
                    self.reject(records)

    def test_invalid_previous_transform_never_crashes_the_next_sample(self):
        for field in ("network_target", "requested_transform", "observed_transform"):
            records = fixture()
            first_walk_sample(records)[field] = None
            result = self.reject(records)
            self.assertEqual(result["rejected_sample_count"], 1)

    def test_monotone_task_and_sample_clocks_are_required(self):
        records = fixture()
        matching(records, "remote_animation_requested")[3]["requested_at_ms"] = EPOCH + 1000
        self.reject(records)
        records = fixture()
        first_walk_sample(records)["sample_at_ms"] = EPOCH + 2000
        self.reject(records)

    def test_rate_window_can_be_recent_without_sharing_the_setter_tick(self):
        records = fixture()
        applied = matching(records, "remote_animation_rate_applied")[0]
        # The most recent changed endpoint can precede a same-task setter query
        # when a later poll repeats the cached target. Preserve its real clock.
        applied["speed_window_started_at_ms"] -= 50
        applied["speed_window_finished_at_ms"] -= 50
        # This synthetic earlier endpoint would precede the task, so use a
        # smaller valid 250 ms window ending 50 ms before this setter instead.
        applied.update(speed_window_ms=250, speed_window_started_at_ms=EPOCH + 2000,
                       speed_window_finished_at_ms=EPOCH + 2250, speed_path_distance=0.125,
                       speed_window_age_ms=50)
        self.assertTrue(self.result(records)["animation_rate_runtime_verified"])
        applied["sample_at_ms"] += 300
        self.reject(records)

    def test_identical_rate_reapplication_does_not_satisfy_minimum_delta(self):
        records = fixture()
        applied = matching(records, "remote_animation_rate_applied")[0]
        duplicate = deepcopy(applied)
        duplicate.update(sample_at_ms=applied["sample_at_ms"] + 300, task_age_ms=600,
                         speed_window_started_at_ms=applied["speed_window_started_at_ms"] + 300,
                         speed_window_finished_at_ms=applied["speed_window_finished_at_ms"] + 300)
        insert_at = records.index(first_walk_sample(records, age=600))
        records.insert(insert_at, duplicate)
        self.reject(records)

    def test_observations_before_entry_and_after_deletion_are_rejected(self):
        records = fixture()
        records[0], records[1] = records[1], records[0]
        self.reject(records)
        records = fixture()
        records.insert(-1, deepcopy(first_walk_sample(records)))
        self.reject(records)

    def test_gradual_height_drift_cannot_pass_as_a_level_lane(self):
        records = fixture()
        for sample in matching(records, "remote_gait_sample"):
            sample["network_target"]["z"] += sample["probe_elapsed_ms"] * 0.0001
        self.reject(records)

    def test_event_and_in_memory_input_caps_remain_finite(self):
        records = fixture() + [value("remote_gait_sample") for _ in range(reader.MAX_EVENTS)]
        result = self.reject(records)
        self.assertEqual(len(result["events"]), reader.MAX_EVENTS)
        with self.assertRaises(ValueError):
            reader.analyze_log(" " * (reader.MAX_LOG_BYTES + 1))


class CommandTests(unittest.TestCase):
    def test_cli_writes_measurement_and_preserves_raw_log(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "game.log", Path(directory) / "rate.json"
            source.write_text(encode(fixture(), exit_markers=True), encoding="utf-8")
            before = source.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(output), "--require-exit"])
            self.assertEqual(code, 0)
            self.assertEqual(source.read_bytes(), before)
            report = json.loads(output.read_text())
            self.assertTrue(report["animation_rate_runtime_verified"])
            self.assertEqual(report["log_fingerprint"]["size_bytes"], len(before))
            self.assertEqual(report["source_line_origin"], str(source.resolve()))

    def test_false_measurement_is_saved_without_changing_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "game.log", Path(directory) / "rate.json"
            source.write_text(encode(fixture(adapted=False)), encoding="utf-8")
            before = source.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(output)])
            self.assertEqual(code, 2)
            self.assertEqual(source.read_bytes(), before)
            self.assertFalse(json.loads(output.read_text())["animation_rate_runtime_verified"])

    def test_raw_evidence_cannot_be_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "game.log"
            source.write_text(encode(fixture()), encoding="utf-8")
            before = source.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(source)])
            self.assertEqual(code, 2)
            self.assertEqual(source.read_bytes(), before)

    def test_read_errors_and_oversize_input_return_false_report(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "game.log", Path(directory) / "rate.json"
            for oversize in (False, True):
                if oversize:
                    with source.open("wb") as stream:
                        stream.truncate(reader.MAX_LOG_BYTES + 1)
                with contextlib.redirect_stdout(io.StringIO()):
                    code = reader.main(["--log", str(source), "--output", str(output)])
                self.assertEqual(code, 2)
                self.assertEqual(json.loads(output.read_text())["status"], "read_error")


if __name__ == "__main__":
    unittest.main()
