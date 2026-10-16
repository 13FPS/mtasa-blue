"""Completed Core lifetime, fresh script identity and exact terminal matching."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import gui_frontend_startup_log as reader
from test_gui_frontend_input_log import passed_values, encoded


def report():
    begin = [0] * 32
    for i, v in {0: 1, 1: 2, 2: 100, 3: 1, 4: 1, 5: 1, 7: 1, 16: 65, 23: 1}.items():
        begin[i] = v
    view = begin.copy()
    view[6], view[7], view[22] = 1, 2, 1
    stopped = view.copy()
    stopped[1], stopped[5], stopped[6], stopped[14], stopped[16] = 4, 0, 0, 1, 0
    return begin + view + stopped


def events():
    values = passed_values()
    worker = [0] * 19
    worker[8], worker[13], worker[15], worker[17] = 2, 1, values[57], 1
    return [dict(event=reader.EVENT + "started", core_api=15, trial_api=1, at_ms=1000),
            dict(event=reader.EVENT + "sample", at_ms=1010, input_values=values.copy()),
            dict(event=reader.EVENT + "finished", schema=reader.SCHEMA, core_api=15, trial_api=1,
                 started_at_ms=1000, at_ms=1020, lifecycle_values=report(), input_values=values.copy(),
                 state=worker, passed=True, cleanup_passed=True, cancel_requested=False, error=None)]


def script(records, quoted=False):
    lines = []
    for record in records:
        text = json.dumps(record)
        if quoted:
            text = '12:00:00 [INFO] ["DE Onli"] ' + json.dumps(text)
        lines.append(text)
    return ("\n".join(lines) + "\n").encode()


def assess(records=None, **kwargs):
    return reader.assess(kwargs.pop("core_data", encoded()), script(events() if records is None else records),
                         expected_pid=kwargs.pop("expected_pid", 71), core_baseline=kwargs.pop("core_baseline", b""),
                         script_baseline=kwargs.pop("script_baseline", b""), **kwargs)


class CompletedFrontendLogTests(unittest.TestCase):
    def test_complete_real_path_requires_all_three_lifetimes_and_matched_input(self):
        result = assess()
        for key in ("diagnostic_valid", "cleanup_complete", "completed_frontend_workload_verified", "lifecycle_retention_verified", "core_terminal_matches_script"):
            self.assertTrue(result[key], key)
        for key in ("script_pid_independently_verified", "module_unloading_independently_verified", "camera_capture_verified",
                    "visible_gta_drawing_verified", "gameplay_ready", "full_mta_ready"):
            self.assertFalse(result[key], key)

    def test_actual_cleo_quoted_event_representation(self):
        result = reader.assess(encoded(), script(events(), quoted=True), expected_pid=71, core_baseline=b"", script_baseline=b"")
        self.assertTrue(result["completed_frontend_workload_verified"])

    def test_owner_identity_cannot_change_between_snapshots(self):
        for offset in (32, 64):
            records = events()
            records[-1]["lifecycle_values"][offset + 2] += 1
            self.assertFalse(assess(records)["diagnostic_valid"])

    def test_coherent_other_owner_cannot_borrow_this_core_input_record(self):
        records = events()
        for field in (2, 34, 66):
            records[-1]["lifecycle_values"][field] += 1
        result = assess(records)
        self.assertFalse(result["core_terminal_matches_script"])
        self.assertFalse(result["completed_frontend_workload_verified"])

    def test_gameplay_network_full_ready_and_reserved_claims_are_rejected(self):
        for offset in (0, 32, 64):
            for field in (18, 19, 20, 24, 31):
                records = events()
                records[-1]["lifecycle_values"][offset + field] = 1
                self.assertFalse(assess(records)["diagnostic_valid"], (offset, field))

    def test_retained_core_after_view_is_required_even_with_successful_input(self):
        for field in (33, 35, 37, 38, 54, 55):
            records = events()
            records[-1]["lifecycle_values"][field] = 0
            self.assertFalse(assess(records)["completed_frontend_workload_verified"], field)

    def test_shutdown_must_retire_backend_and_keep_inspectable_core(self):
        for field, value in ((65, 2), (67, 0), (69, 1), (70, 1), (78, 0), (79, 1), (80, 65)):
            records = events()
            records[-1]["lifecycle_values"][field] = value
            self.assertFalse(assess(records)["completed_frontend_workload_verified"], field)

    def test_copied_input_must_match_fresh_core_terminal_exactly(self):
        records = events()
        records[-1]["input_values"][3] += 1
        self.assertFalse(assess(records)["core_terminal_matches_script"])
        self.assertFalse(assess(expected_pid=72)["completed_frontend_workload_verified"])
        self.assertFalse(assess(core_baseline=encoded())["completed_frontend_workload_verified"])

    def test_success_booleans_cannot_promote_missing_native_scopes(self):
        values = passed_values()
        values[212] = values[214] = values[220] = 0
        records = events()
        for record in records[1:]:
            record["input_values"] = values.copy()
        self.assertFalse(assess(records, core_data=encoded(values))["completed_frontend_workload_verified"])

    def test_old_diagnostic_worker_fields_do_not_count_as_completed_core(self):
        records = events()
        records[-1]["state"][:11] = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5]
        self.assertFalse(assess(records)["diagnostic_valid"])

    def test_worker_errors_unload_and_join_facts_cannot_be_ignored(self):
        for field, value in ((9, 4), (10, 1), (13, 0), (14, 1), (15, 0), (16, 23), (17, 0), (18, 1)):
            records = events()
            records[-1]["state"][field] = value
            self.assertFalse(assess(records)["completed_frontend_workload_verified"], field)

    def test_stopped_owner_cannot_still_owe_view_cleanup(self):
        records = events()
        records[-1]["lifecycle_values"][87] = 0
        result = assess(records)
        self.assertFalse(result["cleanup_complete"])
        self.assertFalse(result["completed_frontend_workload_verified"])

    def test_worker_reserved_masks_and_summary_must_match_lifecycle(self):
        for field, value in ((8, 5), (11, 4), (12, 4), (16, 26), (17, 2)):
            records = events()
            records[-1]["state"][field] = value
            self.assertFalse(assess(records)["diagnostic_valid"], field)

    def test_partial_wrong_image_failure_can_clean_up_without_attempting_view(self):
        records = events()[::2]
        end = records[-1]
        lifecycle = end["lifecycle_values"]
        lifecycle[38], lifecycle[39], lifecycle[54], lifecycle[86] = 0, 1, 0, 0
        lifecycle[53] = lifecycle[85] = 4
        end["input_values"] = None
        end["state"][9], end["state"][15], end["state"][16], end["state"][17] = 4, 0, 23, 0
        end["passed"] = False
        result = assess(records, core_data=b"")
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["completed_frontend_workload_verified"])
        self.assertFalse(assess(records, core_data=b"not JSON\n")["cleanup_complete"])
        self.assertFalse(assess(records, core_data=b"", core_baseline=b"old\n")["cleanup_complete"])
        lifecycle[54] = lifecycle[86] = 1
        self.assertFalse(assess(records, core_data=b"")["cleanup_complete"])

    def test_malformed_types_lengths_and_boolean_integer_confusion(self):
        for value in (None, [0] * 95, [0] * 97):
            records = events()
            records[-1]["lifecycle_values"] = value
            self.assertFalse(assess(records)["diagnostic_valid"])
        for value in (True, -1, 0x100000000):
            records = events()
            records[-1]["lifecycle_values"][7] = value
            self.assertFalse(assess(records)["diagnostic_valid"])
        for key in ("passed", "cleanup_passed", "cancel_requested"):
            records = events()
            records[-1][key] = 1
            self.assertFalse(assess(records)["diagnostic_valid"])

    def test_duplicate_replayed_and_out_of_order_events_fail_freshness(self):
        records = events()
        for candidate in (records + [copy.deepcopy(records[-1])], [records[-1], records[0]], records + [copy.deepcopy(records[0])]):
            self.assertFalse(assess(candidate)["diagnostic_valid"])
        old = script(records)
        result = reader.assess(encoded(), old + script(records), expected_pid=71, core_baseline=b"", script_baseline=old)
        self.assertFalse(result["diagnostic_valid"])
        self.assertFalse(assess(script_baseline=b"different\n")["diagnostic_valid"])

    def test_sample_sequence_and_malformed_sample_preserve_fail_closed_result(self):
        records = events()
        records.insert(2, copy.deepcopy(records[1]))
        self.assertFalse(assess(records)["diagnostic_valid"])
        for value in (None, [], [1], [False] * 256):
            records = events()
            records[1]["input_values"] = value
            self.assertFalse(assess(records)["diagnostic_valid"])

    def test_truncated_terminal_and_duplicate_json_fields_are_not_accepted(self):
        data = script(events())
        for bad in (data[:-1], data.replace(b'"core_api": 15', b'"core_api": 15, "core_api": 15')):
            result = reader.assess(encoded(), bad, expected_pid=71, core_baseline=b"", script_baseline=b"")
            self.assertFalse(result["completed_frontend_workload_verified"])


if __name__ == "__main__":
    unittest.main()
