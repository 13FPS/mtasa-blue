"""Meaningful saved roots-diagnostic framing, attribution and non-admission checks."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils/definitive_edition"))
import render_roots_log as reader


def fields(accepted=12, dispatch=11):
    return [1, 1, 0, accepted, 0, dispatch, 0, 1, 22, 2, 0, 3, 1, 0]


def line(phase=0, values=None, pid=71, run=None, **extra):
    return (json.dumps(dict(event="de_render_roots", pid=pid, run_ms=run or [100, 0], phase=phase,
                            fields=fields() if values is None else values, **extra)) + "\n").encode()


def fixture():
    return line() + line(1, fields(13, 12)) + line(2, fields(14, 13))


def analyze(data=None, **kwargs):
    return reader.analyze_log(fixture() if data is None else data, expected_pid=kwargs.pop("expected_pid", 71), **kwargs)


class RootsLogTests(unittest.TestCase):
    def test_refusal_is_diagnostic_only(self):
        report = analyze()
        self.assertTrue(report["diagnosticObserved"])
        self.assertEqual(report["sampleCount"], 3)
        run = report["matchingNewRuns"][0]
        self.assertTrue(run["complete"])
        self.assertEqual((run["queryPhaseName"], run["checkpointName"], run["refusalReasonName"]),
                         ("NativeRoots", "Texture", "RequiredNull"))
        for key in ("rootsAdmissionVerified", "rootsAdmissionGrantedByDiagnostic", "associationEvidenceFromDiagnostic",
                    "nativePresentObserved", "nativeDispatchObserved", "bufferAssociationObserved",
                    "nativeDispatchBufferAssociationObserved", "gpuOwnershipVerified", "gpuFrameBoundaryVerified",
                    "gpuCommandsIssued", "visualRenderingVerified", "fullMtaReady", "earlierTrialRootCauseEstablished"):
            self.assertFalse(report[key], key)

    def test_known_zero_and_unread_scalar_are_distinct(self):
        value = fields()
        value[11:14] = [3, 0, 0]
        run = analyze(line(values=value))["matchingNewRuns"][0]
        self.assertEqual((run["contextNodeMask"], run["nativeBufferIndex"]), (0, 0))
        value[8:14] = [2, 1, 0, 0, 0, 0]
        run = analyze(line(values=value))["matchingNewRuns"][0]
        self.assertIsNone(run["contextNodeMask"])
        self.assertIsNone(run["nativeBufferIndex"])

    def test_version_and_scalar_semantics(self):
        for offset, value in ((0, 0), (0, 2), (7, 4), (8, 32), (9, 6), (11, 4), (10, 0xc0000005),
                              (11, 0), (12, True), (13, -1), (3, 0x100000000)):
            with self.subTest(offset=offset, value=value):
                bad = fields()
                bad[offset] = value
                self.assertFalse(analyze(line(values=bad))["observed"])
        bad = fields()
        bad[9:11] = [5, 0]
        self.assertTrue(analyze(line(values=bad))["observed"])
        good = fields()
        good[9:11] = [5, 0xc0000005]
        self.assertTrue(analyze(line(values=good))["observed"])

    def test_no_query_after_accepted_callback(self):
        value = fields(dispatch=0)
        value[7:] = [0] * 7
        self.assertTrue(analyze(line(values=value))["observed"])
        value[8] = 1
        self.assertFalse(analyze(line(values=value))["observed"])

    def test_refusal_checkpoint_contract_and_nonzero_correlation(self):
        allowed = {0: {29, 31}, 1: set(range(1, 29)), 2: {2, 3, 4, 6, 7, 11, 12, 17, 22, 24, 25, 27},
                   3: {1, 5, 26}, 4: {23}, 5: set(range(1, 32))}
        for reason in range(6):
            for checkpoint in range(32):
                value = fields()
                value[8:11] = [checkpoint, reason, 0xc0000005 if reason == 5 else 0]
                value[11:14] = [0, 0, 0] if checkpoint <= 15 else [1, 1, 0] if checkpoint <= 19 else [3, 1, 0]
                with self.subTest(reason=reason, checkpoint=checkpoint):
                    self.assertEqual(analyze(line(values=value))["observed"], checkpoint in allowed[reason])
        for at, changed in ((1, 0), (3, 0), (5, 0)):
            value = fields()
            value[at] = changed
            self.assertFalse(analyze(line(values=value))["observed"])
        value = fields()
        value[8:11] = [29, 0, 0]
        value[11:14] = [0, 0, 0]
        self.assertFalse(analyze(line(values=value))["observed"])
        # Counter relationships across two callbacks are evidence, not a frame
        # admission rule. Preserve greater dispatch counts without inventing a bound.
        self.assertTrue(analyze(line(values=fields(dispatch=20)))["observed"])
        value = fields()
        value[8] = 0
        self.assertFalse(analyze(line(values=value))["observed"])

    def test_exact_width_unsigned_and_no_extra_claims(self):
        for value in (fields()[:-1], fields() + [0], [True] + fields()[1:], "not scalars", None):
            raw = line(values=value) if value is not None else line().replace(b'"fields": [', b'"fields": null, "unused": [')
            self.assertFalse(analyze(raw)["observed"])
        for claim in ("native_pointer", "address", "nativeDispatchObserved", "fullMtaReady"):
            self.assertFalse(analyze(line(**{claim: True}))["observed"])

    def test_duplicate_keys_and_malformed_bytes(self):
        for raw in (line().replace(b'"pid": 71', b'"pid": 71, "pid": 71'), b'\xff\n', b'[]\n', b'{bad}\n',
                    line().replace(b'"phase": 0', b'"phase": true')):
            self.assertFalse(analyze(raw)["observed"])

    def test_pid_run_generation_selection(self):
        for pid in (None, 72):
            self.assertFalse(analyze(expected_pid=pid)["observed"])
        self.assertTrue(analyze(expected_run=[100, 0], expected_generation=[1, 0])["observed"])
        self.assertFalse(analyze(expected_run=[101, 0])["observed"])
        self.assertFalse(analyze(expected_generation=[2, 0])["observed"])
        for bad in (True, 0, -1, 0x100000000):
            with self.assertRaises(ValueError):
                analyze(expected_pid=bad)
        for bad in ([True, 0], [1], [0, -1]):
            with self.assertRaises(ValueError):
                analyze(expected_run=bad)

    def test_baseline_never_supplies_old_run(self):
        old = line(pid=72, run=[99, 0]) + line(2, pid=72, run=[99, 0])
        result = analyze(old + fixture(), baseline=old, baseline_existed=True)
        self.assertTrue(result["observed"])
        self.assertEqual(len(result["allValidRuns"]), 2)
        self.assertEqual(result["sampleCount"], 3)
        for data in (old, fixture()):
            self.assertFalse(analyze(data, baseline=old, baseline_existed=True)["observed"])
        prior = line()
        self.assertFalse(analyze(prior + line(2), baseline=prior, baseline_existed=True)["observed"])
        self.assertFalse(analyze(prior + line(2), baseline=prior[:-1], baseline_existed=True)["observed"])

    def test_historical_malformed_record_does_not_hide_fresh_evidence(self):
        old = b'not-json\n'
        result = analyze(old + fixture(), baseline=old, baseline_existed=True)
        self.assertTrue(result["observed"])
        self.assertEqual((result["issueCount"], result["newIssueCount"]), (1, 0))
        self.assertFalse(analyze(fixture() + b'not-json\n')["observed"])

    def test_run_order_bound_and_counter_regression(self):
        for raw in (line(2) + line(1), line() + line(), line(2) + line(2),
                    line() + line(1, fields(11, 10)), line() + line(1, fields(13, 10)),
                    line() + line(1, [1, 2, 0] + fields()[3:]), line() * 33):
            self.assertFalse(analyze(raw)["observed"])
        raw = line() + b''.join(line(1, fields(13 + n, 12 + n)) for n in range(30)) + line(2, fields(50, 49))
        self.assertTrue(analyze(raw)["observed"])
        self.assertFalse(analyze(raw.replace(line(2, fields(50, 49)), line(1, fields(49, 48)) + line(2, fields(50, 49))))["observed"])

    def test_split64_rollover_and_generation(self):
        before, after = fields(), fields()
        before[1:7] = [0xffffffff, 2, 0xffffffff, 3, 0xfffffffe, 3]
        after[1:7] = [0xffffffff, 2, 0, 4, 0xffffffff, 3]
        run = analyze(line(values=before) + line(2, after))["matchingNewRuns"][0]
        self.assertEqual(run["acceptedSnapshots"], 4 << 32)
        self.assertEqual(run["generation"], [0xffffffff, 2])

    def test_end_only_is_explicitly_incomplete_and_multiple_runs_ambiguous(self):
        result = analyze(line(2))
        self.assertTrue(result["observed"])
        self.assertFalse(result["matchingNewRuns"][0]["complete"])
        data = fixture() + line(run=[200, 0]) + line(2, run=[200, 0])
        self.assertFalse(analyze(data)["observed"])
        self.assertTrue(analyze(data, expected_run=[100, 0])["observed"])

    def test_poll_then_end_is_complete_without_unavailable_begin(self):
        self.assertTrue(analyze(line(1) + line(2))["matchingNewRuns"][0]["complete"])
        self.assertFalse(analyze(line() + line(2))["matchingNewRuns"][0]["complete"])

    def test_success_checkpoint_still_does_not_admit_frame(self):
        for checkpoint in (29, 31):
            value = fields()
            value[8:11] = [checkpoint, 0, 0]
            result = analyze(line(values=value))
            self.assertTrue(result["observed"])
            self.assertFalse(result["nativeDispatchObserved"])
            self.assertFalse(result["gpuOwnershipVerified"])

    def test_scalar_progress_and_readback_reachability(self):
        for checkpoint in range(1, 32):
            allowed = {0} if checkpoint < 15 else {0, 1} if checkpoint == 15 else {1} if checkpoint < 19 else {1, 3} if checkpoint == 19 else {3}
            for mask in range(4):
                value = fields()
                value[8:14] = [checkpoint, 5, 0xc0000005, mask, 0, 0]
                with self.subTest(checkpoint=checkpoint, mask=mask):
                    self.assertEqual(analyze(line(values=value))["observed"], mask in allowed)
        for phase in (2, 3):
            for checkpoint in (29, 30, 31):
                value = fields()
                value[7:11] = [phase, checkpoint, 5, 0]
                self.assertEqual(analyze(line(values=value))["observed"], checkpoint == 29)
        value = fields()
        value[8:11] = [30, 0, 0]
        self.assertFalse(analyze(line(values=value))["observed"])


if __name__ == "__main__":
    unittest.main()
