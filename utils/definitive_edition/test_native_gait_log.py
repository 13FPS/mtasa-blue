"""Synthetic evidence tests; no game, bridge, server or UI execution."""

import contextlib
from copy import deepcopy
import io
import json
from pathlib import Path
import tempfile
import unittest

import native_gait_log as reader


EPOCH = 1000000
TASK_AT = EPOCH + 1000
BEGIN = {"x": -40, "y": 23, "z": 3.5, "heading": 270}
TARGET = {**BEGIN, "x": -32}
ANCHOR = {**BEGIN, "x": -36, "y": 20}
REFERENCE = {"reference_id": "native-gait-" + str(EPOCH), "reference_started_at_ms": EPOCH}
OWNED = {"ped": 100, "local_ped": 10, "model": 7}
TASK = {"task_id": "walk-" + str(TASK_AT), "task_requested_at_ms": TASK_AT}


def event(kind, at, **fields):
    return {"event": reader.PREFIX + kind, **REFERENCE, "sample_at_ms": at, **fields}


def fixture(duration=5000, cadence=1, speed=1.2, phase=0.2):
    records = [
        {"event": "adapter_phase", "phase": "entry", "mode": "smoke"},
        event("started", EPOCH, mode="smoke", model=7, interval_ms=100, readiness_budget_ms=10000,
              settle_ms=1000, budget_ms=10000, max_samples=100, min_arrival_age_ms=2000,
              arrival_radius=0.5, lane_radius=12, max_z_drift=2),
        event("spawned", EPOCH, **OWNED, anchor_transform=ANCHOR, start_transform=BEGIN, target_transform=TARGET),
        event("task_requested", TASK_AT, **OWNED, **TASK, move_state=4, time_limit_ms=10000,
              clip="Walk_civi", start_transform=BEGIN, target_transform=TARGET),
    ]
    for age in range(0, duration + 1, 100):
        records.append(event("sample", TASK_AT + age, **OWNED, **TASK, elapsed_ms=1000 + age,
                             task_age_ms=age, playing=True, time=(phase + age / 1000 * cadence) % 1,
                             clip="Walk_civi", observed_transform={**BEGIN, "x": BEGIN["x"] + speed * age / 1000}))
    terminal_at = TASK_AT + 10000
    terminal = {**OWNED, **TASK, "elapsed_ms": terminal_at - EPOCH,
                "task_age_ms": terminal_at - TASK_AT}
    records.extend([
        event("finished", terminal_at, **terminal, reason="time_budget", outcome="observation_only",
              samples=len(range(0, duration + 1, 100))),
        event("cleanup_deleted", terminal_at, **OWNED, **TASK),
        event("stopped", terminal_at, **terminal, reason="time_budget"),
    ])
    return deepcopy(records)


def encode(records, quoted=True):
    lines = []
    for record in records:
        message = json.dumps(record)
        lines.append('12:40:00 [INFO] ["DE Smoke"] ' + (json.dumps(message) if quoted else message))
    return "\n".join(lines) + "\n"


def samples(records):
    return [record for record in records if record.get("event") == reader.PREFIX + "sample"]


def find(records, kind):
    return next(record for record in records if record.get("event") == reader.PREFIX + kind)


class NativeReferenceTests(unittest.TestCase):
    def result(self, records):
        return reader.analyze_log(encode(records))

    def reject(self, records):
        result = self.result(records)
        self.assertFalse(result["engine_reference_measured"])
        self.assertFalse(result["foot_contact_or_no_sliding_verified"])
        self.assertFalse(result["stride_matching_verified"])
        self.assertFalse(result["bounded_animation_rate_implemented"])
        return result

    def test_golden_native_motion_uses_observed_phase_and_least_squares_rates(self):
        for quoted in (True, False):
            result = reader.analyze_log(encode(fixture(), quoted))
            self.assertTrue(result["engine_reference_measured"], result["issues"])
            fit = result["selected_fit"]
            self.assertEqual(fit["phase_wraps_observed"], 4)
            self.assertAlmostEqual(fit["cycle_period_seconds_fit"], 1)
            self.assertAlmostEqual(fit["projected_speed_units_per_second_fit"], 1.2)
            self.assertAlmostEqual(fit["projected_units_per_cycle_fit"], 1.2)
            self.assertAlmostEqual(fit["phase_fit_mean_squared_residual"], 0)
            self.assertAlmostEqual(fit["position_fit_mean_squared_residual"], 0)
            self.assertEqual(result["skipped_reason_counts"]["warmup"], 10)
            self.assertAlmostEqual(result["selected_samples"][0]["time"], 0.2)
            self.assertEqual(fit["source_lines"], [record["line"] for record in result["selected_samples"]])
            self.assertFalse(result["foot_contact_or_no_sliding_verified"])
            self.assertFalse(result["stride_matching_verified"])
            self.assertFalse(result["bounded_animation_rate_implemented"])

    def test_noninteger_cadence_and_negative_world_coordinates_fit(self):
        result = self.result(fixture(cadence=0.8, speed=1.4))
        self.assertTrue(result["engine_reference_measured"])
        self.assertAlmostEqual(result["selected_fit"]["cycle_period_seconds_fit"], 1.25)
        self.assertAlmostEqual(result["selected_fit"]["projected_units_per_cycle_fit"], 1.75)

    def test_missing_each_required_marker_cannot_reuse_an_earlier_fit(self):
        for kind in ("started", "spawned", "task_requested", "finished", "cleanup_deleted", "stopped"):
            with self.subTest(kind=kind):
                records = [record for record in fixture() if record.get("event") != reader.PREFIX + kind]
                self.reject(records)

    def test_duplicate_reference_and_restart_are_rejected(self):
        self.reject(fixture() + fixture())
        records = fixture()
        records.insert(5, deepcopy(find(records, "started")))
        self.reject(records)
        records = fixture() + [{"event": "adapter_phase", "phase": "entry", "mode": "smoke"}]
        self.reject(records)

    def test_reference_task_and_owner_mismatch_reject_without_losing_sample(self):
        for field, value in (("reference_id", "foreign"), ("reference_started_at_ms", EPOCH + 1),
                             ("task_id", "foreign"), ("task_requested_at_ms", TASK_AT + 1),
                             ("ped", 101), ("local_ped", 11), ("model", 0), ("clip", "run_civi")):
            with self.subTest(field=field):
                records = fixture()
                samples(records)[12][field] = value
                result = self.reject(records)
                self.assertEqual(result["rejected_sample_count"], 1)
                self.assertEqual(result["rejected_samples"][0][field], value)

    def test_wrong_native_walk_args_and_owned_cj_are_rejected(self):
        for kind, field, value in (("task_requested", "move_state", 6), ("task_requested", "time_limit_ms", -1),
                                   ("spawned", "ped", 10), ("spawned", "ped", True),
                                   ("started", "max_samples", 101), ("started", "mode", "online")):
            with self.subTest(kind=kind, field=field):
                records = fixture()
                find(records, kind)[field] = value
                self.reject(records)

    def test_native_clip_unavailable_is_incomplete_and_phase_is_never_fabricated(self):
        records = fixture()
        for record in samples(records):
            record.update(playing=False, time=None)
        result = self.reject(records)
        self.assertEqual(result["status"], "incomplete")
        self.assertEqual(result["skipped_reason_counts"], {"clip_unavailable": 51})
        self.assertIsNone(result["selected_fit"])

    def test_invalid_phase_and_native_condition_values_are_rejected(self):
        for field, value in (("time", float("nan")), ("time", float("inf")), ("time", -0.1),
                             ("time", 1.1), ("time", True), ("playing", 1), ("playing", None)):
            with self.subTest(field=field, value=value):
                records = fixture()
                samples(records)[12][field] = value
                self.reject(records)
        records = fixture()
        samples(records)[12].update(playing=False, time=0.4)
        self.reject(records)

    def test_clock_reversal_and_task_age_reset_are_rejected(self):
        for field, value in (("sample_at_ms", TASK_AT), ("elapsed_ms", 0),
                             ("task_age_ms", 0), ("sample_at_ms", True)):
            with self.subTest(field=field):
                records = fixture()
                samples(records)[12][field] = value
                self.reject(records)

    def test_sample_gap_segments_cannot_join_two_inadequate_phase_fragments(self):
        records = fixture(duration=3000)
        records = [record for record in records if record.get("task_age_ms") not in (1800, 1900, 2000)]
        find(records, "finished")["samples"] = len(samples(records))
        result = self.reject(records)
        self.assertTrue(any(boundary["reason"] == "sample_gap_over_250ms"
                            for boundary in result["segmentation_boundaries"]))
        self.assertIsNone(result["selected_fit"])

    def test_warmup_and_destination_stop_are_excluded_from_cycle_fit(self):
        result = self.result(fixture(duration=6000, speed=1.3))
        self.assertTrue(result["engine_reference_measured"])
        self.assertEqual(result["skipped_reason_counts"]["warmup"], 10)
        self.assertEqual(result["skipped_reason_counts"]["destination_stopping_window"], 7)
        self.assertGreaterEqual(min(record["remaining_units"] for record in result["selected_samples"]), 1)
        self.assertAlmostEqual(result["selected_fit"]["projected_speed_units_per_second_fit"], 1.3)

    def test_phase_leap_rejects_even_when_another_good_segment_exists(self):
        records = fixture()
        samples(records)[12]["time"] = 0.9
        result = self.reject(records)
        self.assertGreater(result["rejected_sample_count"], 0)
        self.assertTrue(any(boundary["reason"] == "phase_discontinuity"
                            for boundary in result["segmentation_boundaries"]))

    def test_stationary_phase_or_stationary_motion_cannot_calibrate(self):
        for frozen in ("phase", "motion"):
            records = fixture()
            for record in samples(records):
                if frozen == "phase":
                    record["time"] = 0.2
                else:
                    record["observed_transform"] = {**BEGIN}
            result = self.reject(records)
            self.assertIsNone(result["selected_fit"])

    def test_partial_cycles_are_incomplete(self):
        result = self.reject(fixture(duration=1500))
        self.assertEqual(result["status"], "incomplete")
        self.assertTrue(result["segment_fits"])
        self.assertIsNone(result["selected_fit"])

    def test_departure_or_height_change_rejected_with_provenance(self):
        for field, value in (("y", BEGIN["y"] + 0.6), ("z", BEGIN["z"] + 0.3),
                             ("x", BEGIN["x"] - 2), ("x", float("inf"))):
            with self.subTest(field=field):
                records = fixture()
                samples(records)[15]["observed_transform"][field] = value
                result = self.reject(records)
                self.assertEqual(result["rejected_sample_count"], 1)
                self.assertIn("line", result["rejected_samples"][0])

    def test_abnormal_finish_and_adapter_error_do_not_hide_observed_fit(self):
        records = fixture()
        for kind in ("finished", "stopped"):
            find(records, kind)["reason"] = "render_gap"
        result = self.reject(records)
        self.assertIsNotNone(result["selected_fit"])
        records = fixture() + [{"event": "adapter_failed", "error": "native failed"}]
        result = self.reject(records)
        self.assertEqual(result["status"], "adapter_failed")

    def test_cleanup_mismatch_and_early_stop_are_rejected(self):
        records = fixture()
        find(records, "cleanup_deleted")["ped"] = 101
        self.reject(records)
        records = fixture()
        records[-2], records[-1] = records[-1], records[-2]
        self.reject(records)
        records = fixture()
        find(records, "finished")["samples"] -= 1
        self.reject(records)

    def test_malformed_reference_json_and_unknown_events_are_rejected(self):
        result = reader.analyze_log(encode(fixture()) + reader.PREFIX + 'sample {bad json}\n')
        self.assertFalse(result["engine_reference_measured"])
        self.assertIn("Malformed", result["issues"][0]["reason"])
        self.reject(fixture() + [event("retry", TASK_AT + 6000)])

    def test_empty_evidence_is_incomplete(self):
        self.assertEqual(self.reject([])["status"], "incomplete")

    def test_malformed_origin_and_task_clocks_return_invalid_evidence(self):
        for kind in ("started", "spawned", "task_requested", "finished", "stopped"):
            for value in (None, True, "bad", -1, 10 ** 100, float("nan")):
                with self.subTest(kind=kind, value=value):
                    records = fixture()
                    find(records, kind)["sample_at_ms"] = value
                    self.reject(records)

    def test_zero_length_route_and_anchor_mismatch_are_rejected(self):
        records = fixture()
        find(records, "spawned")["target_transform"] = deepcopy(BEGIN)
        self.reject(records)
        records = fixture()
        find(records, "spawned")["anchor_transform"]["x"] += 1
        self.reject(records)

    def test_task_settling_and_readiness_budgets_are_enforced(self):
        records = fixture()
        find(records, "spawned")["sample_at_ms"] += 500
        self.reject(records)
        records = fixture()
        find(records, "task_requested")["sample_at_ms"] += 20000
        self.reject(records)

    def test_finishing_budget_reason_must_match_actual_clock_or_count(self):
        for reason, age in (("time_budget", 5100), ("sample_budget", 10000), ("arrived", 1000)):
            with self.subTest(reason=reason):
                records = fixture()
                for kind in ("finished", "stopped"):
                    record = find(records, kind)
                    record.update(reason=reason, sample_at_ms=TASK_AT + age,
                                  task_age_ms=age, elapsed_ms=1000 + age)
                find(records, "cleanup_deleted")["sample_at_ms"] = TASK_AT + age
                self.reject(records)

    def test_full_sample_cap_completes_with_observed_cycles(self):
        records = fixture(duration=9900, speed=0.7)
        for kind in ("finished", "stopped"):
            find(records, kind).update(reason="sample_budget", sample_at_ms=TASK_AT + 9900,
                                       elapsed_ms=10900, task_age_ms=9900)
        find(records, "cleanup_deleted")["sample_at_ms"] = TASK_AT + 9900
        result = self.result(records)
        self.assertTrue(result["engine_reference_measured"], result["issues"])
        self.assertEqual(result["sample_count"], 100)
        self.assertAlmostEqual(result["selected_fit"]["projected_speed_units_per_second_fit"], 0.7)

    def test_duplicate_task_and_sample_after_finish_are_rejected(self):
        records = fixture()
        records.insert(4, deepcopy(find(records, "task_requested")))
        self.reject(records)
        records = fixture()
        records.insert(-2, deepcopy(samples(records)[-1]))
        find(records, "finished")["samples"] += 1
        self.reject(records)

    def test_excess_samples_and_catchup_burst_are_rejected(self):
        records = fixture(duration=9900, speed=0.7)
        records.insert(-3, deepcopy(samples(records)[-1]))
        find(records, "finished")["samples"] += 1
        result = self.reject(records)
        self.assertTrue(any("sample cap" in item["reason"] for item in result["issues"]))
        records = fixture()
        early = samples(records)[12]
        early.update(sample_at_ms=early["sample_at_ms"] - 50,
                     elapsed_ms=early["elapsed_ms"] - 50, task_age_ms=early["task_age_ms"] - 50)
        self.reject(records)

    def test_native_event_cap_and_text_cap_are_enforced(self):
        records = fixture() + [event("sample", TASK_AT + 5000) for _ in range(reader.MAX_EVENTS)]
        result = self.reject(records)
        self.assertEqual(len(result["events"]), reader.MAX_EVENTS)
        self.assertTrue(any("event cap" in item["reason"] for item in result["issues"]))
        with self.assertRaises(ValueError):
            reader.analyze_log(" " * (reader.MAX_LOG_BYTES + 1))


class CommandTests(unittest.TestCase):
    def test_cli_preserves_raw_evidence_and_writes_full_report(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "raw.log", Path(directory) / "audit.json"
            evidence = encode(fixture())
            source.write_text(evidence, encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(output)])
            self.assertEqual(code, 0)
            self.assertEqual(source.read_text(encoding="utf-8"), evidence)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertTrue(report["engine_reference_measured"])
            self.assertEqual(report["source_line_origin"], str(source.resolve()))
            self.assertEqual(report["log_fingerprint"]["size_bytes"], len(source.read_bytes()))

    def test_incomplete_report_is_saved_and_raw_log_remains(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "raw.log", Path(directory) / "audit.json"
            source.write_text(encode(fixture()[:-3]), encoding="utf-8")
            before = source.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(output)])
            self.assertEqual(code, 2)
            self.assertEqual(source.read_bytes(), before)
            self.assertFalse(json.loads(output.read_text())["engine_reference_measured"])

    def test_output_cannot_overwrite_raw_log(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "raw.log"
            source.write_text(encode(fixture()), encoding="utf-8")
            before = source.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(source)])
            self.assertEqual(code, 2)
            self.assertEqual(source.read_bytes(), before)

    def test_read_error_writes_a_false_result_without_deleting_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "missing.log", Path(directory) / "audit.json"
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(output)])
            self.assertEqual(code, 2)
            self.assertEqual(json.loads(output.read_text())["status"], "read_error")

    def test_actual_read_is_capped_at_sixteen_mib(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "large.log", Path(directory) / "audit.json"
            with source.open("wb") as stream:
                stream.truncate(reader.MAX_LOG_BYTES + 1)
            with contextlib.redirect_stdout(io.StringIO()):
                code = reader.main(["--log", str(source), "--output", str(output)])
            self.assertEqual(code, 2)
            self.assertEqual(json.loads(output.read_text())["status"], "read_error")


if __name__ == "__main__":
    unittest.main()
