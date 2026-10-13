"""Synthetic saved evidence, without game/process/DLL actions."""

from copy import deepcopy
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

import native_pad_log as reader

EPOCH = 1000000


def event(kind, at=EPOCH, **extra):
    return {"event": kind, "reference_id": "native-pad-" + str(EPOCH), "ped": 17, "at_ms": at, "elapsed_ms": at - EPOCH,
            "provenance": reader.PROVENANCE, "field_count": 24, "pad_count": 2, "previous_state": "engine_OldState",
            "previous_source": "native_OldState", "velocity_units": None,
            "network_active": False, "input_writes": False, "remote_peds_created": False, **extra}


def fixture(count=240, spacing=125, reason="time_budget", finish_at=None):
    rows = [{"event": "adapter_phase", "phase": "entry", "mode": "smoke"},
            {"event": "adapter_start", "mode": "smoke", "endpoint": None, "native_pad_reference": True,
             "provenance": reader.PROVENANCE, "network_active": False, "input_writes": False, "remote_peds_created": False},
            event("native_pad_reference_reset", reason="start"),
            event("native_pad_reference_started", duration_ms=30000, sample_interval_ms=125 if spacing != 100 else 100,
                  max_samples=300, render_gap_ms=2000, readiness_budget_ms=10000)]
    for index in range(count):
        pads = [{"current": [0] * 24, "previous": [0] * 24} for _ in range(2)]
        pads[0]["current"][0] = -32768
        pads[0]["current"][16] = index % 2
        pads[0]["previous"][0] = 32767
        pads[1]["current"][23] = -index
        pads[1]["previous"][16] = -5
        rows.append(event("native_pad_sample", EPOCH + index * spacing, pads=pads, position={"x": index * 0.1, "y": 2, "z": 3},
                          heading=270, raw_velocity={"x": 0.1, "y": -0.2, "z": 0}))
    at = finish_at if finish_at is not None else EPOCH + (30000 if reason == "time_budget" else max(0, count - 1) * spacing)
    rows.extend([event("native_pad_reference_finished", at, reason=reason, outcome="observation_only", samples=count),
                 event("native_pad_reference_reset", at, reason="cleanup"), event("native_pad_reference_stopped", at, reason=reason, samples=count)])
    return rows


def encode(rows, quoted=True):
    return "\n".join('12:40:00 [INFO] ["DE Smoke"] ' + (json.dumps(json.dumps(row)) if quoted else json.dumps(row)) for row in rows)


def samples(rows):
    return [row for row in rows if row["event"] == "native_pad_sample"]


def with_device_states(rows):
    for row in rows:
        if row["event"].startswith("native_pad_reference_") or row["event"] == "native_pad_sample":
            row.update(capture_version=2, device_state_source="native_provider_source_states",
                       device_state_count=127, controller_layout_verified=False)
        if row["event"] == "native_pad_sample":
            row["device_states"] = [0] * 127
    return rows


class NativePadLogTests(unittest.TestCase):
    def report(self, rows):
        return reader.analyze_log(encode(rows))

    def reject(self, rows):
        report = self.report(rows)
        self.assertFalse(report["native_pad_reference_complete"])
        self.assertFalse(report["physical_input_mapping_verified"])
        self.assertFalse(report["velocity_units_verified"])
        self.assertFalse(report["engine_input_write_verified"])
        return report

    def test_normal240_plain_quoted_and_activity(self):
        for quoted in (False, True):
            report = reader.analyze_log(encode(fixture(), quoted))
            self.assertTrue(report["native_pad_reference_complete"], report["issues"])
            self.assertEqual(report["accepted_sample_count"], 240)
            self.assertEqual(report["observations"]["pads"][0]["current"]["button_16_nonzero_samples"], 120)
            self.assertEqual(report["observations"]["pads"][1]["previous"]["button_16_nonzero_samples"], 240)
            self.assertEqual(report["observations"]["raw_velocity_nonzero_samples"], 240)
            self.assertEqual(report["accepted_samples"][0]["line"], 5)
            self.assertFalse(report["physical_input_mapping_verified"])

    def test_zero_data_is_valid_but_no_activity_or_mapping(self):
        rows = fixture()
        for row in samples(rows):
            row["pads"] = [{"current": [0] * 24, "previous": [0] * 24} for _ in range(2)]
            row["raw_velocity"] = {"x": 0, "y": 0, "z": 0}
        report = self.report(rows)
        self.assertTrue(report["native_pad_reference_complete"])
        self.assertFalse(report["physical_input_mapping_verified"])
        self.assertEqual(report["observations"]["pads"][0]["current"]["nonzero_samples"], 0)

    def test_native_oldstate_need_not_equal_preceding_probe_sample(self):
        rows = fixture(4, reason="Ctrl+Shift+End")
        samples(rows)[1]["pads"][0]["previous"] = [-123] * 24
        self.assertTrue(self.report(rows)["native_pad_reference_complete"])

    def test_device_source_states_preserve_activity_without_mapping_claims(self):
        rows = with_device_states(fixture(4, reason="Ctrl+Shift+End"))
        samples(rows)[1]["device_states"][17] = 2
        samples(rows)[2]["device_states"][17] = 3
        samples(rows)[3]["device_states"][126] = -2147483648
        report = reader.analyze_log(encode(rows), True, True)
        self.assertTrue(report["native_pad_reference_complete"], report["issues"])
        self.assertFalse(report["physical_input_mapping_verified"])
        states = report["observations"]["device_states"]
        self.assertEqual(states["changed_indices"], [17, 126])
        self.assertEqual(states["active_indices"], [17])
        self.assertEqual(states["sources"][17]["engine_active_samples"], 2)
        self.assertEqual(states["sources"][126]["observed_values"], [-2147483648, 0])

    def test_legacy_capture_cannot_satisfy_required_device_trial(self):
        report = reader.analyze_log(encode(fixture()), True, True)
        self.assertFalse(report["native_pad_reference_complete"])
        self.assertFalse(report["flags"]["required_device_states_present"])
        self.assertEqual(report["capture_version"], 1)

    def test_device_state_dimensions_ranges_and_types(self):
        for bad in (None, [], [0]*126, [0]*128, [True]+[0]*126, [0.0]*127,
                    [-2147483649]+[0]*126, [2147483648]+[0]*126):
            rows = with_device_states(fixture(4, reason="Ctrl+Shift+End"))
            samples(rows)[1]["device_states"] = bad
            self.assertEqual(self.reject(rows)["rejected_sample_count"], 1)

    def test_device_contract_missing_changed_or_overclaiming(self):
        for key, value in (("capture_version", 1), ("device_state_source", "physical_keys"),
                           ("device_state_count", True), ("controller_layout_verified", True)):
            rows = with_device_states(fixture(4, reason="Ctrl+Shift+End"))
            samples(rows)[1][key] = value
            self.reject(rows)
        rows = with_device_states(fixture(4, reason="Ctrl+Shift+End"))
        del samples(rows)[1]["device_states"]
        self.reject(rows)

    def test_device_cli_requires_the_new_capture_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            log, output = Path(directory)/"raw.log", Path(directory)/"report.json"
            for rows, expected in ((fixture(), 2), (with_device_states(fixture()), 0)):
                log.write_text(encode(rows), encoding="utf-8")
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(reader.main(["--log", str(log), "--output", str(output), "--require-device-states"]), expected)

    def test_full_minute_device_reference_and_cleanup(self):
        rows = with_device_states(fixture(480, finish_at=EPOCH+60000))
        rows[3].update(duration_ms=60000, max_samples=600)
        report = reader.analyze_log(encode(rows), True, True)
        self.assertTrue(report["native_pad_reference_complete"], report["issues"])
        self.assertEqual(report["accepted_sample_count"], 480)

    def test_six_hundred_capture_budget_remains_bounded(self):
        rows = with_device_states(fixture(600, 100, "sample_budget"))
        rows[3].update(duration_ms=60000, max_samples=600)
        self.assertTrue(self.report(rows)["native_pad_reference_complete"])
        rows[3]["max_samples"] = 601
        self.reject(rows)

    def test_longer_reference_still_rejects_deadline_and_capture_gaps(self):
        rows = with_device_states(fixture(480, finish_at=EPOCH+60000))
        rows[3].update(duration_ms=60001, max_samples=600)
        self.reject(rows)
        rows[3]["duration_ms"] = 60000
        samples(rows)[200]["at_ms"] += 2001
        self.reject(rows)

    def test_missing_cleanup_never_completes_and_preserves_data(self):
        rows = [row for row in fixture() if not (row["event"] == "native_pad_reference_reset" and row["reason"] == "cleanup")]
        report = self.reject(rows)
        self.assertEqual(report["accepted_sample_count"], 240)

    def test_truncated_after_last_sample_is_incomplete(self):
        report = self.reject(fixture()[:-3])
        self.assertEqual(report["status"], "incomplete")
        self.assertEqual(report["accepted_sample_count"], 240)

    def test_render_gap_cleanup_is_preserved_without_normal_completion(self):
        report = self.reject(fixture(4, reason="render_gap", finish_at=EPOCH + 2500))
        self.assertEqual(report["status"], "incomplete")
        self.assertTrue(report["flags"]["one_cleanup_reset"])

    def test_three_hundred_sample_budget(self):
        report = self.report(fixture(300, 100, "sample_budget"))
        self.assertTrue(report["native_pad_reference_complete"], report["issues"])

    def test_bad_pad_state_shapes_and_ranges(self):
        for bad in (None, {}, [], [{"current": [0]*24, "previous": [0]*24}],
                    [{"current": [0]*23, "previous": [0]*24}]*2,
                    [{"current": [32768]+[0]*23, "previous": [0]*24}]*2,
                    [{"current": [True]+[0]*23, "previous": [0]*24}]*2,
                    [{"current": [0]*24, "previous": [0.0]*24}]*2):
            rows = fixture(4, reason="Ctrl+Shift+End"); samples(rows)[1]["pads"] = bad
            self.assertEqual(self.reject(rows)["rejected_sample_count"], 1)

    def test_bad_movement_values(self):
        for field, bad in (("heading", float("nan")), ("heading", 10**1000), ("raw_velocity", {"x": True, "y": 0, "z": 0}), ("position", {"x": 1, "y": 2})):
            rows = fixture(4, reason="Ctrl+Shift+End"); samples(rows)[1][field] = bad
            self.assertEqual(self.reject(rows)["rejected_sample_count"], 1)

    def test_identity_source_and_clock_mismatch(self):
        for field, bad in (("ped", 18), ("provenance", "bridge_copy"), ("previous_source", "previous_probe_sample"),
                           ("previous_state", "previous_sample"), ("field_count", 20), ("elapsed_ms", 1), ("input_writes", True)):
            rows = fixture(4, reason="Ctrl+Shift+End"); samples(rows)[1][field] = bad
            self.reject(rows)

    def test_duplicate_early_reversed_or_large_gap(self):
        for delta in (0, 100, -1, 2001):
            rows = fixture(4, reason="Ctrl+Shift+End"); samples(rows)[1].update(at_ms=EPOCH+delta, elapsed_ms=delta)
            self.reject(rows)

    def test_restart_and_duplicate_start(self):
        rows = fixture(4, reason="Ctrl+Shift+End")
        self.reject(rows + deepcopy(rows))
        self.reject(rows[:4] + [deepcopy(rows[3])] + rows[4:])

    def test_wrong_completion_count_and_premature_deadline(self):
        for field, bad in (("samples", 239), ("elapsed_ms", 29999), ("outcome", "verified")):
            rows = fixture(); rows[-3][field] = bad
            self.reject(rows)

    def test_adapter_failures_and_cleanup_failure(self):
        for kind in ("adapter_failed", "cleanup_failed"):
            rows = fixture(); rows.append({"event": kind, "error": "failed"})
            self.assertEqual(self.reject(rows)["status"], "adapter_failed")

    def test_malformed_options_or_finish_clocks_do_not_crash_analysis(self):
        for key, bad in (("duration_ms", "30000"), ("sample_interval_ms", None), ("max_samples", True), ("render_gap_ms", -1)):
            rows = fixture(); rows[3][key] = bad; self.reject(rows)
        for bad in (None, "30000", True, -1):
            rows = fixture(); rows[-3]["at_ms"] = bad; self.reject(rows)
        rows = fixture(); rows[-3]["reason"] = []; self.reject(rows)

    def test_enclosing_mode_network_and_endpoint(self):
        for change in ({"mode": "online"}, {"endpoint": "127.0.0.1:7788"}, {"native_pad_reference": False}):
            rows = fixture(); rows[1].update(change); self.reject(rows)

    def test_malformed_marker_and_bounded_input(self):
        report = reader.analyze_log(encode(fixture()) + '\n{"event":"native_pad_sample",broken')
        self.assertFalse(report["native_pad_reference_complete"])
        with self.assertRaises(ValueError):
            reader.analyze_log(" " * (reader.MAX_LOG_BYTES + 1))
        self.reject(fixture() + [{"event": "unrelated"}] * reader.MAX_EVENTS)

    def test_cli_complete_and_incomplete_policy_and_fingerprint(self):
        with tempfile.TemporaryDirectory() as directory:
            log, output = Path(directory)/"raw.log", Path(directory)/"report.json"
            log.write_text(encode(fixture()), encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(reader.main(["--log", str(log), "--output", str(output), "--require-complete"]), 0)
            report = json.loads(output.read_text())
            self.assertEqual(report["log_fingerprint"]["size_bytes"], log.stat().st_size)
            log.write_text(encode(fixture()[:-3]), encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(reader.main(["--log", str(log), "--output", str(output)]), 0)
                self.assertEqual(reader.main(["--log", str(log), "--output", str(output), "--require-complete"]), 2)
            self.assertFalse(json.loads(output.read_text())["native_pad_reference_complete"])

    def test_cli_never_overwrites_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory)/"raw.log"; original = encode(fixture()); log.write_text(original, encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(reader.main(["--log", str(log), "--output", str(log)]), 2)
            self.assertEqual(log.read_text(), original)


if __name__ == "__main__":
    unittest.main()
