"""Persistent frontend freshness, two leases and independent lifetime evidence."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import persistent_frontend_log as reader
from test_gui_frontend_input_log import passed_values
from test_gui_frontend_startup_log import report


def values():
    v = passed_values() + [0] * 32
    for field, value in {3: 45200, 85: 0, 103: 1, 242: 45000, 256: 1, 257: 4, 259: 1,
                         260: 17, 263: 1, 264: 5, 265: 1000, 266: 500, 267: 850, 269: 1}.items():
        v[field] = value
    return v


def encoded(v=None, pid=71, run_ms=100):
    return (json.dumps(dict(schema=reader.SCHEMA, pid=pid, runMs=run_ms, values=values() if v is None else v)) + "\n").encode()


def events(v=None):
    v = values() if v is None else v
    worker = [0] * 19
    worker[8], worker[13], worker[15], worker[17] = 2, 1, v[57], 1
    return [dict(event=reader.EVENT + "started", schema=reader.SCHEMA, core_api=16, trial_api=1, at_ms=1000,
                 fields=288, input_fields=288, lifecycle_fields=96, maximum_duration_ms=0, display_duration_ms=0,
                 host_heartbeat_budget_ms=1000, native_heartbeat_budget_ms=500),
            dict(event=reader.EVENT + "generation", at_ms=1000, generation=4),
            dict(event=reader.EVENT + "sample", at_ms=46200, elapsed_ms=45200, input_values=v.copy()),
            dict(event=reader.EVENT + "finished", schema=reader.SCHEMA, core_api=16, trial_api=1,
                 started_at_ms=1000, at_ms=46300, elapsed_ms=45200, lifecycle_values=report(), input_values=v.copy(),
                 generation=4, state=worker, passed=True, cleanup_passed=True, cancel_requested=False, host_lease_lost=False,
                 host_heartbeat_rejected=False, error=None, samples=1, network_active=False, gameplay_ready=False,
                 full_core_ready=False, full_mta_ready=False, camera_capture_verified=False)]


def script(records, quoted=False):
    lines = []
    for record in records:
        line = json.dumps(record)
        if quoted:
            line = '12:00:00 [INFO] ["DE Online"] ' + json.dumps(line)
        lines.append(line)
    return ("\n".join(lines) + "\n").encode()


def assess(records=None, **kwargs):
    return reader.assess(kwargs.pop("core_data", encoded()), kwargs.pop("script_data", script(events() if records is None else records)),
                         expected_pid=kwargs.pop("expected_pid", 71), core_baseline=kwargs.pop("core_baseline", b""),
                         script_baseline=kwargs.pop("script_baseline", b""), **kwargs)


class PersistentFrontendLogTests(unittest.TestCase):
    def test_full_new_lifetime_and_short_leases_are_required(self):
        result = assess()
        for key in ("diagnostic_valid", "cleanup_complete", "persistent_frontend_workload_verified", "lifecycle_retention_verified", "core_terminal_matches_script"):
            self.assertTrue(result[key], key)
        for key in ("script_pid_independently_verified", "module_unloading_independently_verified", "native_player_control_command_verified",
                    "camera_capture_verified", "visible_gta_drawing_verified", "network_started", "gameplay_ready", "full_mta_ready"):
            self.assertFalse(result[key], key)

    def test_actual_cleo_quoted_representation(self):
        self.assertTrue(assess(script_data=script(events(), quoted=True))["persistent_frontend_workload_verified"])

    def test_legacy_schema_and_twenty_second_duration_cannot_impersonate_persistence(self):
        for field, value in ((85, 20000), (256, 0), (265, 35000), (266, 2000)):
            v = values()
            v[field] = value
            self.assertFalse(assess(events(v), core_data=encoded(v))["persistent_frontend_workload_verified"], field)
        data = encoded().replace(reader.SCHEMA.encode(), b"mta_core_gui_frontend_input_v1")
        self.assertFalse(assess(core_data=data)["cleanup_complete"])

    def test_duration_must_extend_beyond_old_native_limit(self):
        for duration in (20000, 34999, 35000):
            v = values()
            v[242] = duration
            result = assess(events(v), core_data=encoded(v))
            self.assertTrue(result["cleanup_complete"])
            self.assertFalse(result["persistent_frontend_workload_verified"], duration)

    def test_script_elapsed_must_also_extend_beyond_old_limit(self):
        records = events()
        records[2]["at_ms"], records[2]["elapsed_ms"] = 35000, 34000
        records[-1]["at_ms"], records[-1]["elapsed_ms"] = 35100, 34100
        self.assertFalse(assess(records)["persistent_frontend_workload_verified"])

    def test_gui_close_and_fresh_host_are_required_for_positive_result(self):
        for field, value in ((103, 0), (259, 0), (264, 1), (267, 0), (269, 5)):
            v = values()
            v[field] = value
            result = assess(events(v), core_data=encoded(v))
            self.assertTrue(result["cleanup_complete"], field)
            self.assertFalse(result["persistent_frontend_workload_verified"], field)

    def test_orderly_cancel_host_expiry_and_focus_loss_can_prove_cleanup(self):
        for stop in (2, 3, 4):
            v = values()
            v[4], v[21], v[103], v[269] = 2 if stop == 2 else 7, 0, 0, stop
            v[259] = 0 if stop == 3 else 1
            if stop == 4:
                v[264], v[228] = 3, 7
            records = events(v)
            end = records[-1]
            end["state"][9], end["state"][17] = 5, 0
            end["state"][16] = 12 if stop == 2 else 0
            end["passed"] = False
            end["cancel_requested"] = stop == 2
            end["host_lease_lost"] = stop == 3
            result = assess(records, core_data=encoded(v))
            self.assertTrue(result["diagnostic_valid"], stop)
            self.assertTrue(result["cleanup_complete"], stop)
            self.assertFalse(result["persistent_frontend_workload_verified"], stop)

    def test_native_worker_expiry_retains_failure_but_proves_full_retirement(self):
        v = values()
        v[4], v[21], v[103], v[228], v[264], v[269] = 7, 0, 0, 7, 1, 5
        records = events(v)
        records[-1]["state"][9], records[-1]["state"][16], records[-1]["state"][17] = 4, 23, 0
        records[-1]["passed"] = False
        result = assess(records, core_data=encoded(v))
        self.assertTrue(result["diagnostic_valid"])
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["persistent_frontend_workload_verified"])
        self.assertEqual(result["values"][228], 7, "raw failure evidence must never be zeroed")
        self.assertEqual(result["terminal"]["input_values"], v)

    def test_focus_failure_and_nested_callback_history_are_separate_from_cleanup(self):
        for reason in range(1, 19):
            v = values()
            v[4], v[21], v[103], v[226], v[228], v[264], v[269] = 7, 0, 0, 1, reason, 3, 4
            records = events(v)
            records[-1]["state"][9], records[-1]["state"][17], records[-1]["passed"] = 5, 0, False
            result = assess(records, core_data=encoded(v))
            self.assertTrue(result["diagnostic_valid"], reason)
            self.assertTrue(result["cleanup_complete"], reason)
            self.assertFalse(result["persistent_frontend_workload_verified"], reason)
            self.assertEqual(result["values"][226], 1)
            self.assertEqual(result["values"][228], reason)

    def test_historical_failures_never_hide_live_ownership_or_restore_failure(self):
        for field, value in ((208, 1), (210, 1), (220, 19), (222, 1), (223, 1), (239, 1), (243, 1),
                             (49, 1), (102, 1), (169, 1)):
            v = values()
            v[4], v[21], v[226], v[228], v[264], v[269] = 7, 0, 1, 7, 1, 5
            v[field] = value
            result = assess(events(v), core_data=encoded(v))
            self.assertFalse(result["cleanup_complete"], field)
            self.assertFalse(result["persistent_frontend_workload_verified"], field)

    def test_failure_reason_bounds_and_success_counter_requirements_remain_strict(self):
        for field, value in ((228, 19), (229, 1)):
            v = values()
            v[field] = value
            self.assertFalse(assess(events(v), core_data=encoded(v))["cleanup_complete"], field)
        for field in (226, 227, 228):
            v = values()
            v[field] = 1
            result = assess(events(v), core_data=encoded(v))
            self.assertTrue(result["cleanup_complete"], field)
            self.assertFalse(result["persistent_frontend_workload_verified"], field)

    def test_heartbeat_rejection_racing_quit_does_not_invent_expiry(self):
        records = events()
        records[-1]["host_heartbeat_rejected"] = True
        records.insert(2, dict(event=reader.EVENT + "host_heartbeat_rejected", at_ms=46000))
        self.assertTrue(assess(records)["persistent_frontend_workload_verified"])

    def test_incorrect_host_expiry_claim_is_rejected(self):
        records = events()
        records[-1]["host_lease_lost"] = True
        self.assertFalse(assess(records)["cleanup_complete"])

    def test_active_unrevoked_or_wrong_native_token_cannot_claim_cleanup(self):
        for field, value in ((262, 1), (263, 0), (264, 0), (260, 0), (238, 0), (220, 19), (243, 1)):
            v = values()
            v[field] = value
            self.assertFalse(assess(events(v), core_data=encoded(v))["cleanup_complete"], field)

    def test_reserved_fields_and_boolean_uint_confusion_are_rejected(self):
        for field in (256, 257, 259, 260, 263, 264, 265, 266, 267, 269, 270, 287):
            v = values()
            v[field] = True
            self.assertFalse(assess(events(v), core_data=encoded(v))["cleanup_complete"], field)
        for field in (270, 287):
            v = values()
            v[field] = 1
            self.assertFalse(assess(events(v), core_data=encoded(v))["cleanup_complete"], field)

    def test_generations_cannot_drift_between_events_and_core(self):
        for record_index, key in ((1, "generation"), (-1, "generation")):
            records = events()
            records[record_index][key] += 1
            self.assertFalse(assess(records)["persistent_frontend_workload_verified"])
        v = values()
        v[257] += 1
        self.assertFalse(assess(events(v), core_data=encoded(v))["core_terminal_matches_script"])

    def test_other_coherent_core_owner_cannot_supply_lifecycle_for_this_view(self):
        records = events()
        for field in (2, 34, 66):
            records[-1]["lifecycle_values"][field] += 1
        self.assertFalse(assess(records)["core_terminal_matches_script"])
        for field in (34, 66):
            records = events()
            records[-1]["lifecycle_values"][field] += 1
            self.assertFalse(assess(records)["diagnostic_valid"])

    def test_all_input_component_owners_must_match_lifecycle_owner(self):
        for field in (81, 98, 130, 178):
            v = values()
            v[field] += 1
            self.assertFalse(assess(events(v), core_data=encoded(v))["core_terminal_matches_script"], field)

    def test_core_lives_after_view_and_stops_only_after_its_cleanup(self):
        for field, value in ((33, 3), (37, 0), (38, 0), (54, 0), (55, 0), (65, 2), (67, 0), (78, 0), (87, 0)):
            records = events()
            records[-1]["lifecycle_values"][field] = value
            self.assertFalse(assess(records)["persistent_frontend_workload_verified"], field)

    def test_worker_flags_and_unloading_cannot_be_assumed(self):
        for field, value in ((0, 1), (9, 4), (10, 1), (11, 4), (12, 1), (13, 0), (15, 0), (16, 23), (17, 0), (18, 1)):
            records = events()
            records[-1]["state"][field] = value
            self.assertFalse(assess(records)["persistent_frontend_workload_verified"], field)
        records = events()
        records[-1]["state"][14] = 1
        self.assertTrue(assess(records)["persistent_frontend_workload_verified"], "prior Game DE load is a separate loader fact")

    def test_exact_terminal_bytes_pid_and_run_identity_are_required(self):
        records = events()
        records[-1]["input_values"][3] += 1
        self.assertFalse(assess(records)["core_terminal_matches_script"])
        self.assertFalse(assess(expected_pid=72)["cleanup_complete"])
        self.assertFalse(assess(expected_run_ms=101)["cleanup_complete"])
        self.assertFalse(assess(core_data=encoded() + encoded(run_ms=101))["cleanup_complete"])

    def test_reset_cleo_or_core_log_never_discards_saved_baseline(self):
        for key in ("core_baseline", "script_baseline"):
            self.assertFalse(assess(**{key: b"old run\n"})["cleanup_complete"], key)
        self.assertFalse(assess(core_baseline=encoded())["cleanup_complete"])
        old = script(events())
        self.assertFalse(assess(script_data=old + script(events()), script_baseline=old)["diagnostic_valid"])

    def test_hash_pinned_baselines_and_complete_boundaries_are_checked(self):
        for key in ("core_baseline_sha256", "script_baseline_sha256"):
            self.assertTrue(assess(**{key: hashlib.sha256(b"").hexdigest()})["persistent_frontend_workload_verified"])
            self.assertFalse(assess(**{key: "0" * 64})["cleanup_complete"])
        self.assertFalse(assess(core_data=b"x" + encoded(), core_baseline=b"x")["cleanup_complete"])

    def test_replayed_core_run_and_script_token_are_rejected(self):
        old = encoded()
        self.assertFalse(assess(core_data=old + encoded(), core_baseline=old)["cleanup_complete"])
        old_events = events()
        old_events[0]["at_ms"] = 1
        old_events[1]["at_ms"] = 1
        old_events[-1]["started_at_ms"] = 1
        baseline = script(old_events)
        self.assertFalse(assess(script_data=baseline + script(events()), script_baseline=baseline)["diagnostic_valid"])

    def test_missing_duplicate_or_reordered_events_and_samples_fail_closed(self):
        records = events()
        for candidate in (records[1:], records[:1] + records[2:], records + [copy.deepcopy(records[-1])],
                          [records[-1]] + records[:-1], records[:2] + [copy.deepcopy(records[2])] + records[2:]):
            self.assertFalse(assess(candidate)["diagnostic_valid"])

    def test_sample_counters_time_and_native_generation_never_regress(self):
        for field in (2, 3, 99, 139, 142, 143, 165, 187, 212, 214, 216, 220, 242, 260, 267):
            records = events()
            previous = copy.deepcopy(records[2])
            previous["at_ms"], previous["elapsed_ms"] = 46000, 45000
            previous["input_values"][2] -= 1
            previous["input_values"][field] += 1
            records.insert(2, previous)
            records[-1]["samples"] = 2
            self.assertFalse(assess(records)["diagnostic_valid"], field)

    def test_full_network_or_camera_claims_are_rejected(self):
        for field in (18, 19, 20, 24, 31):
            records = events()
            records[-1]["lifecycle_values"][field] = 1
            self.assertFalse(assess(records)["diagnostic_valid"], field)
        for key in ("network_active", "full_core_ready", "gameplay_ready", "full_mta_ready", "camera_capture_verified"):
            records = events()
            records[-1][key] = True
            self.assertFalse(assess(records)["diagnostic_valid"], key)

    def test_optional_console_matches_same_pid_run_and_embedded_values(self):
        console = (json.dumps(dict(schema="mta_core_frontend_console_v1", pid=71, runMs=100, values=values()[128:160])) + "\n").encode()
        self.assertTrue(assess(console_data=console, console_baseline=b"")["optional_console_matches_input"])
        self.assertFalse(assess(console_data=console.replace(b'"pid": 71', b'"pid": 72'), console_baseline=b"")["cleanup_complete"])
        with self.assertRaises(ValueError):
            assess(console_data=console)

    def test_truncated_and_duplicate_json_keys_cannot_claim_success(self):
        for data in (script(events())[:-1], script(events()).replace(b'"core_api": 16', b'"core_api": 16, "core_api": 16')):
            self.assertFalse(assess(script_data=data)["cleanup_complete"])
        self.assertFalse(assess(core_data=encoded()[:-1])["cleanup_complete"])

    def test_malformed_types_and_lengths_fail_closed(self):
        for v in (None, [0] * 287, [0] * 289, [False] * 288):
            records = events()
            records[-1]["input_values"] = v
            self.assertFalse(assess(records)["cleanup_complete"])
        for key in ("passed", "cleanup_passed", "cancel_requested", "host_lease_lost"):
            records = events()
            records[-1][key] = 1
            self.assertFalse(assess(records)["diagnostic_valid"])
        for key in ("trial_api", "maximum_duration_ms", "display_duration_ms"):
            records = events()
            records[0][key] = bool(records[0][key])
            self.assertFalse(assess(records)["diagnostic_valid"], key)
        records = events()
        records[-1]["samples"] = True
        self.assertFalse(assess(records)["diagnostic_valid"])

    def test_wrong_image_no_view_refusal_can_still_prove_owner_cleanup(self):
        records = events()
        records.pop(2)
        end = records[-1]
        lifecycle = end["lifecycle_values"]
        lifecycle[38], lifecycle[39], lifecycle[54], lifecycle[86] = 0, 1, 0, 0
        lifecycle[53] = lifecycle[85] = 4
        end["input_values"] = None
        end["state"][9], end["state"][15], end["state"][16], end["state"][17] = 4, 0, 23, 0
        end["passed"], end["samples"] = False, 0
        result = assess(records, core_data=b"")
        self.assertTrue(result["cleanup_complete"])
        self.assertFalse(result["persistent_frontend_workload_verified"])
        self.assertFalse(assess(records, core_data=b"not JSON\n")["cleanup_complete"])
        self.assertFalse(assess(records, core_data=b"", core_baseline=b"old\n")["cleanup_complete"])


if __name__ == "__main__":
    unittest.main()
