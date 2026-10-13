# SPDX-License-Identifier: GPL-3.0-or-later
"""Mock helper lifecycle without loading a DLL, launching GTA, or opening UDP."""

import contextlib
import io
import json
import math
from pathlib import Path
import unittest
from unittest import mock

import game_peer


def player(identity=1, x=10.0, y=-20.0, z=3.0, heading=90.0):
    return dict(id=identity, x=x, y=y, z=z, heading=heading)


class Clock:
    def __init__(self):
        self.now = 0.0
        self.sleeps = []

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.sleeps.append(seconds)
        self.now += seconds


class FakeBridge:
    def __init__(self, clock, peers=lambda now: []):
        self.clock = clock
        self.peer_states = peers
        self.library = self
        self.connect_times = []
        self.disconnect_times = []
        self.pump_times = []
        self.submissions = []
        self.own_id = 100
        self.pump_result = 1
        self.submit_result = 1
        self.connect_error = None
        self.disconnect_error = None

    def connect(self):
        self.connect_times.append(self.clock.now)
        if self.connect_error is not None:
            raise self.connect_error

    def disconnect(self):
        self.disconnect_times.append(self.clock.now)
        if self.disconnect_error is not None:
            raise self.disconnect_error

    def players(self):
        return self.peer_states(self.clock.now)

    def DE_Pump(self):
        self.pump_times.append(self.clock.now)
        return self.pump_result

    def DE_LocalPlayerId(self):
        return self.own_id

    def DE_SubmitTransform(self, *transform):
        self.submissions.append((self.clock.now, transform))
        return self.submit_result


class HelperRunTests(unittest.TestCase):
    def setUp(self):
        self.clock = Clock()
        self.bridge = FakeBridge(self.clock)
        self.events = []

    def run_helper(self, seconds=1.0, radius=5.0, factory=None, sleep=None,
                   profile="orbit"):
        def capture(event, **fields):
            self.events.append(dict(event=event, captured_at=self.clock.now, **fields))

        # Make real DLL loading fail loudly even if a future refactor bypasses
        # the fake Bridge. All timing and output are local fixture state.
        with mock.patch.object(game_peer, "Bridge", factory or mock.Mock(return_value=self.bridge)), \
                mock.patch.object(game_peer.ctypes, "CDLL", side_effect=AssertionError("No DLL load allowed")), \
                mock.patch.object(game_peer.time, "monotonic", self.clock.monotonic), \
                mock.patch.object(game_peer.time, "sleep", sleep or self.clock.sleep), \
                mock.patch.object(game_peer, "emit", capture):
            return game_peer.run(Path("fake-bridge.dll"), seconds, radius, profile)

    def result(self):
        results = [event for event in self.events if event["event"] == "synthetic_peer_result"]
        self.assertEqual(len(results), 1)
        return results[0]

    def test_no_peer_times_out_without_placeholder_updates_and_disconnects(self):
        self.assertEqual(self.run_helper(), 1)
        result = self.result()
        self.assertFalse(result["remote_observed"])
        self.assertEqual(result["updates_sent"], 0)
        self.assertIsNone(result["error"])
        self.assertEqual(self.bridge.submissions, [])
        self.assertEqual(len(self.bridge.disconnect_times), 1)
        self.assertAlmostEqual(result["elapsed_seconds"], 1.0, places=2)

    def test_waiting_peer_rejoins_before_five_second_idle_without_publishing(self):
        self.assertEqual(self.run_helper(seconds=9.0), 1)
        self.assertEqual(len(self.bridge.connect_times), 3)
        for previous, current in zip(self.bridge.connect_times, self.bridge.connect_times[1:]):
            self.assertGreaterEqual(current - previous, 4.0)
            self.assertLess(current - previous, 5.0)
        self.assertEqual(len(self.bridge.disconnect_times), 3)
        self.assertEqual(self.bridge.submissions, [])
        self.assertFalse(self.result()["remote_observed"])

    def test_moving_remote_produces_bounded_circle_at_received_height(self):
        self.bridge.peer_states = lambda now: [player(x=10.0 + now)]
        self.assertEqual(self.run_helper(seconds=0.3), 0)
        self.assertGreaterEqual(len(self.bridge.submissions), 3)
        for stamp, (x, y, z, heading) in self.bridge.submissions:
            self.assertAlmostEqual(math.hypot(x - (10.0 + stamp), y + 20.0), 5.0)
            self.assertEqual(z, 3.0)
            self.assertGreaterEqual(heading, 0.0)
            self.assertLess(heading, 360.0)
        self.assertTrue(self.result()["remote_motion_observed"])
        self.assertTrue(self.result()["synthetic_motion_sent"])
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_stationary_remote_is_distinguished_from_synthetic_motion(self):
        self.bridge.peer_states = lambda now: [player()]
        self.assertEqual(self.run_helper(seconds=0.3), 0)
        self.assertFalse(self.result()["remote_motion_observed"])
        self.assertTrue(self.result()["synthetic_motion_sent"])

    def test_zero_anchor_waits_until_a_nonzero_position_arrives(self):
        self.bridge.peer_states = lambda now: [player(x=0, y=0, z=0)] if now < 0.2 else [player()]
        self.assertEqual(self.run_helper(seconds=0.4), 0)
        self.assertGreaterEqual(self.bridge.submissions[0][0], 0.2)
        observed = [event for event in self.events if event["event"] == "remote_transport_observed"]
        self.assertEqual(len(observed), 1)
        self.assertEqual(observed[0]["remote_transform"], player())

    def test_own_player_record_cannot_be_selected_as_remote(self):
        self.bridge.peer_states = lambda now: [player(identity=self.bridge.own_id)]
        self.assertEqual(self.run_helper(), 1)
        self.assertEqual(self.bridge.submissions, [])
        self.assertFalse(self.result()["remote_observed"])

    def test_invalid_transform_records_cannot_anchor_the_helper(self):
        self.bridge.peer_states = lambda now: [player(x=math.nan), player(identity=2, z=100001), player(identity=0)]
        self.assertEqual(self.run_helper(), 1)
        self.assertEqual(self.bridge.submissions, [])
        self.assertFalse(self.result()["remote_observed"])

    def test_missing_connection_identity_waits_without_reading_peers(self):
        self.bridge.own_id = 0
        self.bridge.players = mock.Mock(side_effect=AssertionError("No joined identity"))
        self.assertEqual(self.run_helper(), 1)
        self.bridge.players.assert_not_called()
        self.assertEqual(self.bridge.submissions, [])
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_remote_departure_stops_early_and_disconnects(self):
        self.bridge.peer_states = lambda now: [player()] if now < 0.2 else []
        self.assertEqual(self.run_helper(), 0)
        result = self.result()
        self.assertTrue(result["remote_departure_observed"])
        self.assertLess(result["elapsed_seconds"], 0.3)
        self.assertEqual(len(self.bridge.disconnect_times), 1)
        self.assertEqual([event["event"] for event in self.events], ["synthetic_peer_started",
                         "remote_transport_observed", "remote_transport_departed", "synthetic_peer_result"])

    def test_other_peer_cannot_replace_the_selected_remote_after_departure(self):
        self.bridge.peer_states = lambda now: [player()] if now < 0.2 else [player(identity=2)]
        self.assertEqual(self.run_helper(), 0)
        self.assertEqual(self.result()["remote_player_id"], 1)
        self.assertTrue(self.result()["remote_departure_observed"])

    def test_submission_failure_disconnects_and_does_not_count_the_failed_update(self):
        self.bridge.peer_states = lambda now: [player()]
        self.bridge.submit_result = 0
        self.assertEqual(self.run_helper(), 1)
        self.assertEqual(self.result()["error"], "Synthetic transform submission failed")
        self.assertEqual(self.result()["updates_sent"], 0)
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_pump_failure_disconnects_with_actionable_error(self):
        self.bridge.pump_result = 0
        self.assertEqual(self.run_helper(), 1)
        self.assertIn("ensure the loopback server is running", self.result()["error"])
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_keyboard_interrupt_disconnects_and_reports_interruption(self):
        self.bridge.peer_states = lambda now: [player()]
        self.assertEqual(self.run_helper(sleep=mock.Mock(side_effect=KeyboardInterrupt)), 1)
        self.assertEqual(self.result()["error"], "Interrupted by user")
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_bridge_construction_failure_reports_result_without_partial_connect(self):
        factory = mock.Mock(side_effect=RuntimeError("Unsupported bridge ABI"))
        self.assertEqual(self.run_helper(factory=factory), 1)
        self.assertEqual(self.result()["error"], "Unsupported bridge ABI")
        self.assertEqual(self.bridge.connect_times, [])
        self.assertEqual(self.bridge.disconnect_times, [])

    def test_failed_connect_still_attempts_disconnect(self):
        self.bridge.connect_error = OSError("Fake connect failed")
        self.assertEqual(self.run_helper(), 1)
        self.assertEqual(self.result()["error"], "Fake connect failed")
        self.assertEqual(len(self.bridge.disconnect_times), 1)
        self.assertEqual([event["event"] for event in self.events], ["synthetic_peer_result"])

    def test_disconnect_failure_keeps_result_and_prevents_success(self):
        self.bridge.peer_states = lambda now: [player()]
        self.bridge.disconnect_error = OSError("Fake disconnect failed")
        self.assertEqual(self.run_helper(seconds=0.3), 1)
        result = self.result()
        self.assertTrue(result["synthetic_motion_sent"])
        self.assertEqual(result["cleanup_error"], "Fake disconnect failed")
        self.assertEqual(result["error"], "Disconnect failed: Fake disconnect failed")

    def test_disconnect_failure_does_not_hide_original_submission_error(self):
        self.bridge.peer_states = lambda now: [player()]
        self.bridge.submit_result = 0
        self.bridge.disconnect_error = RuntimeError("Fake cleanup failed")
        self.assertEqual(self.run_helper(), 1)
        self.assertEqual(self.result()["error"], "Synthetic transform submission failed")
        self.assertEqual(self.result()["cleanup_error"], "Fake cleanup failed")

    def test_duration_and_update_interval_bound_the_experiment(self):
        self.bridge.peer_states = lambda now: [player()]
        self.assertEqual(self.run_helper(seconds=0.3), 0)
        self.assertAlmostEqual(self.clock.now, 0.3, places=2)
        stamps = [stamp for stamp, _ in self.bridge.submissions]
        self.assertTrue(all(stamp < 0.3 for stamp in stamps))
        self.assertLessEqual(len(stamps), 6)
        for previous, current in zip(stamps, stamps[1:]):
            self.assertGreaterEqual(current - previous, 0.05 - 1e-12)
        self.assertLessEqual(len(self.events), 3)

    def test_idle_walk_idle_submits_three_bounded_stages_then_disconnects(self):
        self.bridge.peer_states = lambda now: [player()]
        self.assertEqual(self.run_helper(seconds=30.0, profile="idle-walk-idle"), 0)
        stages = [event for event in self.events if event["event"] == "synthetic_profile_stage"]
        self.assertEqual([event["stage"] for event in stages], ["idle_start", "walk", "idle_end"])
        self.assertEqual(stages[0]["seconds_after_anchor"], 0.0)
        self.assertGreaterEqual(stages[1]["seconds_after_anchor"], 5.0)
        self.assertLess(stages[1]["seconds_after_anchor"], 5.1)
        self.assertGreaterEqual(stages[2]["seconds_after_anchor"], 25.0)
        self.assertLess(stages[2]["seconds_after_anchor"], 25.1)
        initial = [transform for stamp, transform in self.bridge.submissions if stamp < 5.0]
        moving = [transform for stamp, transform in self.bridge.submissions if 5.0 <= stamp < 25.0]
        final = [transform for stamp, transform in self.bridge.submissions if stamp >= 25.0]
        self.assertTrue(initial and moving and final)
        self.assertEqual(len(set(initial)), 1)
        self.assertGreater(len(set(moving)), 2)
        self.assertEqual(len(set(final)), 1)
        self.assertAlmostEqual(self.clock.now, 30.0, places=2)
        self.assertTrue(all(stamp < 30.0 for stamp, _ in self.bridge.submissions))
        self.assertEqual(len(self.bridge.disconnect_times), 1)
        self.assertEqual(sum(self.result()["profile_stage_updates"].values()), self.result()["updates_sent"])

    def test_idle_walk_idle_fixed_anchor_cannot_follow_user_motion_or_height(self):
        self.bridge.peer_states = lambda now: [player(x=10.0 + now, y=-20.0 + now * 2,
                                                    z=3.0 + now, heading=90.0 + now * 10)]
        self.assertEqual(self.run_helper(seconds=3.0, profile="idle-walk-idle"), 0)
        for stamp, transform in self.bridge.submissions:
            _, expected = game_peer.idle_walk_idle_transform(player(), stamp, 3.0, 5.0)
            self.assertEqual(transform, expected)
            self.assertAlmostEqual(math.hypot(transform[0] - 10.0, transform[1] + 20.0), 5.0)
            self.assertEqual(transform[2], 3.0)
        self.assertTrue(self.result()["remote_motion_observed"])
        started = self.events[0]
        self.assertEqual(started["anchor_mode"], "first_remote_transform_fixed")
        self.assertEqual(started["height_source"], "copied_anchor_z_without_terrain_query")
        self.assertTrue(started["waiting_for_anchor_consumes_duration"])
        self.assertEqual(started["profile_stage_seconds"], [0.5, 2.0, 0.5])

    def test_idle_walk_idle_stages_scale_from_anchor_time_without_extending_deadline(self):
        self.bridge.peer_states = lambda now: [] if now < 0.6 else [player()]
        self.assertEqual(self.run_helper(seconds=1.2, profile="idle-walk-idle"), 0)
        stages = [event for event in self.events if event["event"] == "synthetic_profile_stage"]
        self.assertEqual([event["stage"] for event in stages], ["idle_start", "walk"])
        self.assertGreaterEqual(stages[0]["captured_at"], 0.6)
        self.assertEqual(stages[0]["seconds_after_anchor"], 0.0)
        self.assertGreaterEqual(stages[1]["seconds_after_anchor"], 0.2)
        self.assertLess(stages[1]["seconds_after_anchor"], 0.3)
        self.assertNotIn("idle_end", self.result()["profile_stage_updates"])
        self.assertAlmostEqual(self.clock.now, 1.2, places=2)
        self.assertTrue(all(stamp < 1.2 for stamp, _ in self.bridge.submissions))

    def test_idle_walk_idle_remote_departure_during_idle_still_stops(self):
        self.bridge.peer_states = lambda now: [player()] if now < 0.2 else []
        self.assertEqual(self.run_helper(seconds=3.0, profile="idle-walk-idle"), 1)
        self.assertEqual(self.result()["synthetic_distinct_positions"], 1)
        self.assertTrue(self.result()["remote_departure_observed"])
        self.assertLess(self.result()["elapsed_seconds"], 0.3)
        self.assertEqual(len(self.bridge.disconnect_times), 1)
        stages = [event["stage"] for event in self.events if event["event"] == "synthetic_profile_stage"]
        self.assertEqual(stages, ["idle_start"])

    def test_idle_walk_idle_failed_first_submission_cannot_emit_stage(self):
        self.bridge.peer_states = lambda now: [player()]
        self.bridge.submit_result = 0
        self.assertEqual(self.run_helper(profile="idle-walk-idle"), 1)
        self.assertEqual(self.result()["profile_stage_updates"], {})
        self.assertEqual(self.result()["updates_sent"], 0)
        self.assertEqual([event["event"] for event in self.events],
                         ["synthetic_peer_started", "remote_transport_observed", "synthetic_peer_result"])
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_unknown_profile_cannot_load_bridge(self):
        factory = mock.Mock(side_effect=AssertionError("Invalid profile must not load a DLL"))
        self.assertEqual(self.run_helper(factory=factory, profile="unknown"), 1)
        factory.assert_not_called()
        self.assertEqual(self.result()["error"], "Unknown synthetic motion profile: unknown")
        self.assertEqual(self.bridge.disconnect_times, [])

    def test_straight_calibration_submits_all_five_stages_before_success(self):
        self.bridge.peer_states = lambda now: [player()]
        self.assertEqual(self.run_helper(seconds=30.0, profile="straight-walk-calibration"), 0)
        stages = [event for event in self.events if event["event"] == "synthetic_profile_stage"]
        expected = ["idle_start", "walk_out", "idle_turn", "walk_back", "idle_end"]
        self.assertEqual([event["stage"] for event in stages], expected)
        for event, boundary in zip(stages, (0.0, 6.0, 12.0, 18.0, 24.0)):
            self.assertGreaterEqual(event["seconds_after_anchor"], boundary)
            self.assertLess(event["seconds_after_anchor"], boundary + 0.1)
            self.assertEqual(event["fixed_anchor"], player())
        result = self.result()
        self.assertTrue(result.get("profile_complete"))
        self.assertEqual(set(result["profile_stage_updates"]), set(expected))
        self.assertTrue(all(count > 0 for count in result["profile_stage_updates"].values()))
        self.assertEqual(sum(result["profile_stage_updates"].values()), result["updates_sent"])
        self.assertTrue(result["synthetic_motion_sent"])
        self.assertEqual(len(self.bridge.disconnect_times), 1)
        self.assertAlmostEqual(self.clock.now, 30.0, places=2)
        self.assertTrue(all(stamp < 30.0 for stamp, _ in self.bridge.submissions))

    def test_straight_calibration_fixed_lane_ignores_later_player_motion(self):
        self.bridge.peer_states = lambda now: [player(x=10.0 + now, y=-20.0 - now,
                                                    z=3.0 + now, heading=90.0 + now)]
        self.assertEqual(self.run_helper(seconds=30.0, profile="straight-walk-calibration"), 0)
        for stamp, (x, y, z, heading) in self.bridge.submissions:
            self.assertEqual(y, -17.0)
            self.assertEqual(z, 3.0)
            self.assertGreaterEqual(x, 7.0)
            self.assertLessEqual(x, 13.0)
            self.assertEqual(heading, 270.0 if stamp < 12.0 else 90.0)
            if stamp < 6.0:
                self.assertEqual(x, 7.0)
            elif 12.0 <= stamp < 18.0:
                self.assertEqual(x, 13.0)
            elif stamp >= 24.0:
                self.assertEqual(x, 7.0)
        self.assertTrue(self.result()["remote_motion_observed"])
        started = self.events[0]
        self.assertEqual(started.get("anchor_mode"), "first_remote_transform_fixed")
        self.assertEqual(started.get("profile_stage_seconds"), [6.0] * 5)
        self.assertEqual(started.get("profile_speed_units_per_second"), 1.0)
        self.assertEqual(started.get("heading_basis"), "classic_cardinal_candidate_not_game_verified")
        self.assertEqual(started["height_source"], "copied_anchor_z_without_terrain_query")
        self.assertTrue(started["waiting_for_anchor_consumes_duration"])

    def test_straight_calibration_late_anchor_cannot_claim_completed_profile(self):
        self.bridge.peer_states = lambda now: [] if now < 8.0 else [player()]
        self.assertEqual(self.run_helper(seconds=30.0, profile="straight-walk-calibration"), 1)
        result = self.result()
        self.assertFalse(result.get("profile_complete", True))
        self.assertTrue(result["synthetic_motion_sent"])
        self.assertNotIn("idle_end", result["profile_stage_updates"])
        self.assertEqual([event["stage"] for event in self.events
                          if event["event"] == "synthetic_profile_stage"],
                         ["idle_start", "walk_out", "idle_turn", "walk_back"])
        self.assertAlmostEqual(self.clock.now, 30.0, places=2)
        self.assertTrue(all(stamp < 30.0 for stamp, _ in self.bridge.submissions))
        self.assertEqual(len(self.bridge.disconnect_times), len(self.bridge.connect_times))

    def test_straight_calibration_departure_during_turn_prevents_success(self):
        self.bridge.peer_states = lambda now: [player()] if now < 15.0 else []
        self.assertEqual(self.run_helper(seconds=30.0, profile="straight-walk-calibration"), 1)
        result = self.result()
        self.assertTrue(result["remote_departure_observed"])
        self.assertTrue(result["synthetic_motion_sent"])
        self.assertFalse(result.get("profile_complete", True))
        self.assertEqual(set(result["profile_stage_updates"]), {"idle_start", "walk_out", "idle_turn"})
        self.assertLess(result["elapsed_seconds"], 15.1)
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_straight_calibration_departure_before_walking_sends_only_idle(self):
        self.bridge.peer_states = lambda now: [player()] if now < 1.0 else []
        self.assertEqual(self.run_helper(seconds=30.0, profile="straight-walk-calibration"), 1)
        result = self.result()
        self.assertTrue(result["remote_departure_observed"])
        self.assertEqual(result["synthetic_distinct_positions"], 1)
        self.assertFalse(result.get("profile_complete", True))
        self.assertEqual(set(result["profile_stage_updates"]), {"idle_start"})
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_straight_calibration_failed_submission_cannot_emit_a_stage(self):
        self.bridge.peer_states = lambda now: [player()]
        self.bridge.submit_result = 0
        self.assertEqual(self.run_helper(seconds=30.0, profile="straight-walk-calibration"), 1)
        result = self.result()
        self.assertEqual(result["error"], "Synthetic transform submission failed")
        self.assertEqual(result["updates_sent"], 0)
        self.assertEqual(result["profile_stage_updates"], {})
        self.assertFalse(result.get("profile_complete", True))
        self.assertFalse(any(event["event"] == "synthetic_profile_stage" for event in self.events))
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_straight_calibration_cleanup_failure_overrides_complete_motion(self):
        self.bridge.peer_states = lambda now: [player()]
        self.bridge.disconnect_error = RuntimeError("Fake cleanup failed")
        self.assertEqual(self.run_helper(seconds=30.0, profile="straight-walk-calibration"), 1)
        result = self.result()
        self.assertTrue(result.get("profile_complete"))
        self.assertTrue(result["synthetic_motion_sent"])
        self.assertEqual(result["cleanup_error"], "Fake cleanup failed")
        self.assertIn("Disconnect failed", result["error"])

    def test_straight_calibration_invalid_parameters_fail_before_dll_loading(self):
        invalid = ((math.nan, 5.0), (math.inf, 5.0), (-math.inf, 5.0),
                   (0.0, 5.0), (0.99, 5.0), (300.01, 5.0),
                   (30.0, math.nan), (30.0, math.inf), (30.0, -math.inf),
                   (30.0, 0.49), (30.0, 20.01))
        for seconds, radius in invalid:
            with self.subTest(seconds=seconds, radius=radius):
                self.events = []
                factory = mock.Mock(side_effect=AssertionError("Invalid parameters must not load a DLL"))
                self.assertEqual(self.run_helper(seconds=seconds, radius=radius,
                                                profile="straight-walk-calibration", factory=factory), 1)
                factory.assert_not_called()
                self.assertIn("finite", self.result()["error"])
                self.assertFalse(self.result().get("profile_complete", True))
        self.assertEqual(self.bridge.connect_times, [])
        self.assertEqual(self.bridge.disconnect_times, [])

    def test_straight_calibration_excessive_or_slow_speed_fails_before_dll_loading(self):
        for seconds, radius in ((19.0, 5.0), (61.0, 5.0)):
            with self.subTest(seconds=seconds):
                self.events = []
                factory = mock.Mock(side_effect=AssertionError("Unsafe calibration speed must not load a DLL"))
                self.assertEqual(self.run_helper(seconds=seconds, radius=radius,
                                                profile="straight-walk-calibration", factory=factory), 1)
                factory.assert_not_called()
                self.assertIn("speed", self.result()["error"].lower())
                self.assertFalse(self.result().get("profile_complete", True))

    def test_straight_calibration_inclusive_speed_limits_and_maximum_lane_are_allowed(self):
        for seconds, radius, speed in ((60.0, 5.0, 0.5), (20.0, 5.0, 1.5),
                                       (80.0, 20.0, 1.5), (6.0, 0.5, 0.5)):
            with self.subTest(seconds=seconds, radius=radius):
                self.clock = Clock()
                self.bridge = FakeBridge(self.clock, lambda now: [player()])
                self.events = []
                self.assertEqual(self.run_helper(seconds=seconds, radius=radius,
                                                profile="straight-walk-calibration"), 0)
                self.assertEqual(self.events[0].get("profile_speed_units_per_second"), speed)
                self.assertTrue(self.result().get("profile_complete"))


    def test_stride_sweep_all_seven_accepted_stages_complete_on_fixed_deadline(self):
        self.bridge.peer_states = lambda now: [player()]
        self.assertEqual(self.run_helper(seconds=30, profile="stride-speed-sweep"), 0)
        stages = [e for e in self.events if e["event"] == "synthetic_profile_stage"]
        expected = ("idle_start", "walk_slow", "idle_turn_one", "walk_medium",
                    "idle_turn_two", "walk_fast", "idle_end")
        self.assertEqual(tuple(e["stage"] for e in stages), expected)
        for event, boundary in zip(stages, (0, 2, 14, 16, 22, 24, 28)):
            self.assertGreaterEqual(event["seconds_after_anchor"], boundary)
            self.assertLess(event["seconds_after_anchor"], boundary + 0.1)
            self.assertEqual(event["fixed_anchor"], player())
        started = self.events[0]
        self.assertEqual(started["profile_stage_seconds"], [2, 12, 2, 6, 2, 4, 2])
        self.assertEqual(started["profile_speeds_units_per_second"],
                         {"walk_slow": 0.5, "walk_medium": 1.0, "walk_fast": 1.5})
        self.assertTrue(started["waiting_for_anchor_consumes_duration"])
        self.assertEqual(started["height_source"], "copied_anchor_z_without_terrain_query")
        self.assertEqual(started["anchor_mode"], "first_remote_transform_fixed")
        result = self.result()
        self.assertTrue(result["profile_complete"])
        self.assertEqual(tuple(result["profile_stage_updates"]), expected)
        self.assertTrue(all(n > 0 for n in result["profile_stage_updates"].values()))
        self.assertEqual(sum(result["profile_stage_updates"].values()), result["updates_sent"])
        self.assertFalse(result["remote_departure_observed"])
        self.assertEqual(result["remote_player_id"], 1)
        self.assertEqual(result["synthetic_player_id"], 100)
        self.assertAlmostEqual(self.clock.now, 30, places=2)
        self.assertTrue(all(stamp < 30 for stamp, _ in self.bridge.submissions))
        for previous, current in zip(self.bridge.submissions, self.bridge.submissions[1:]):
            self.assertGreaterEqual(current[0] - previous[0], game_peer.UPDATE_INTERVAL - 1e-12)
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_stride_sweep_fixed_anchor_ignores_later_game_motion(self):
        self.bridge.peer_states = lambda now: [player(x=10 + now, y=-20 + now, z=3 + now, heading=90 + now)]
        self.assertEqual(self.run_helper(seconds=30, profile="stride-speed-sweep"), 0)
        for stamp, transform in self.bridge.submissions:
            self.assertEqual(transform[1:3], (-17.0, 3.0))
            self.assertGreaterEqual(transform[0], 7.0)
            self.assertLessEqual(transform[0], 13.0)
            self.assertEqual(transform[3], 90.0 if 14 <= stamp < 22 else 270.0)
        self.assertTrue(self.result()["remote_motion_observed"])

    def test_stride_sweep_invalid_numbers_fail_before_loading_bridge(self):
        for seconds, radius in ((math.nan, 5), (math.inf, 5), (0, 5), (301, 5),
                                (30, math.nan), (30, math.inf), (30, 0.49), (30, 20.01)):
            with self.subTest(seconds=seconds, radius=radius):
                self.events = []
                factory = mock.Mock(side_effect=AssertionError("Invalid sweep cannot load a DLL"))
                self.assertEqual(self.run_helper(seconds=seconds, radius=radius,
                                                profile="stride-speed-sweep", factory=factory), 1)
                factory.assert_not_called()
                self.assertIn("finite", self.result()["error"])
                self.assertFalse(self.result()["profile_complete"])
        self.assertEqual(self.bridge.connect_times, [])
        self.assertEqual(self.bridge.disconnect_times, [])

    def test_stride_sweep_rejects_if_either_slowest_or_fastest_speed_exceeds_limits(self):
        for seconds, radius in ((29.99, 5), (60.01, 5), (30, 20), (300, 0.5)):
            with self.subTest(seconds=seconds, radius=radius):
                self.events = []
                factory = mock.Mock(side_effect=AssertionError("Invalid speed cannot load a DLL"))
                self.assertEqual(self.run_helper(seconds=seconds, radius=radius,
                                                profile="stride-speed-sweep", factory=factory), 1)
                factory.assert_not_called()
                self.assertIn("speed", self.result()["error"].lower())
                self.assertFalse(self.result()["profile_complete"])

    def test_stride_sweep_inclusive_all_speed_limits_and_scaled_timings(self):
        for seconds, radius, expected in ((30, 5, (0.5, 1.0, 1.5)), (60, 5, (0.25, 0.5, 0.75)),
                                          (3, 0.5, (0.5, 1.0, 1.5)), (120, 20, (0.5, 1.0, 1.5))):
            with self.subTest(seconds=seconds, radius=radius):
                self.clock = Clock()
                self.bridge = FakeBridge(self.clock, lambda now: [player()])
                self.events = []
                self.assertEqual(self.run_helper(seconds=seconds, radius=radius,
                                                profile="stride-speed-sweep"), 0)
                self.assertEqual(tuple(self.events[0]["profile_speeds_units_per_second"].values()), expected)
                self.assertTrue(self.result()["profile_complete"])

    def test_stride_sweep_late_anchor_cannot_extend_deadline_or_claim_missing_final_stages(self):
        self.bridge.peer_states = lambda now: [] if now < 4 else [player()]
        self.assertEqual(self.run_helper(seconds=30, profile="stride-speed-sweep"), 1)
        result = self.result()
        self.assertFalse(result["profile_complete"])
        self.assertNotIn("idle_end", result["profile_stage_updates"])
        self.assertAlmostEqual(result["elapsed_seconds"], 30, places=2)
        self.assertTrue(all(stamp < 30 for stamp, _ in self.bridge.submissions))
        self.assertEqual(len(self.bridge.disconnect_times), len(self.bridge.connect_times))

    def test_stride_sweep_missing_middle_stage_from_scheduler_gap_cannot_succeed(self):
        self.bridge.peer_states = lambda now: [player()]
        def skip_turn(seconds):
            if 13.99 <= self.clock.now < 14.0:
                self.clock.now = 16.1
            else:
                self.clock.sleep(seconds)
        self.assertEqual(self.run_helper(seconds=30, profile="stride-speed-sweep", sleep=skip_turn), 1)
        self.assertFalse(self.result()["profile_complete"])
        self.assertNotIn("idle_turn_one", self.result()["profile_stage_updates"])
        self.assertIn("idle_end", self.result()["profile_stage_updates"])
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_stride_sweep_selected_peer_cannot_be_replaced_after_departure(self):
        self.bridge.peer_states = lambda now: [player(identity=100), player(identity=1)] if now < 15 else [player(identity=2)]
        self.assertEqual(self.run_helper(seconds=30, profile="stride-speed-sweep"), 1)
        result = self.result()
        self.assertTrue(result["remote_departure_observed"])
        self.assertFalse(result["profile_complete"])
        self.assertEqual(result["remote_player_id"], 1)
        self.assertLess(result["elapsed_seconds"], 15.1)
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_stride_sweep_departure_after_last_stage_still_prevents_run_success(self):
        self.bridge.peer_states = lambda now: [player()] if now < 29 else []
        self.assertEqual(self.run_helper(seconds=30, profile="stride-speed-sweep"), 1)
        self.assertTrue(self.result()["profile_complete"])
        self.assertTrue(self.result()["remote_departure_observed"])
        self.assertLess(self.result()["elapsed_seconds"], 29.1)
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_stride_sweep_rejected_submission_cannot_record_accepted_stage(self):
        self.bridge.peer_states = lambda now: [player()]
        self.bridge.submit_result = 0
        self.assertEqual(self.run_helper(seconds=30, profile="stride-speed-sweep"), 1)
        self.assertFalse(self.result()["profile_complete"])
        self.assertEqual(self.result()["profile_stage_updates"], {})
        self.assertEqual(self.result()["updates_sent"], 0)
        self.assertFalse(any(e["event"] == "synthetic_profile_stage" for e in self.events))
        self.assertEqual(len(self.bridge.disconnect_times), 1)

    def test_stride_sweep_cleanup_failure_preserves_complete_stage_evidence_but_fails_run(self):
        self.bridge.peer_states = lambda now: [player()]
        self.bridge.disconnect_error = RuntimeError("Sweep cleanup failed")
        self.assertEqual(self.run_helper(seconds=30, profile="stride-speed-sweep"), 1)
        self.assertTrue(self.result()["profile_complete"])
        self.assertEqual(self.result()["cleanup_error"], "Sweep cleanup failed")
        self.assertEqual(self.result()["error"], "Disconnect failed: Sweep cleanup failed")


class MotionProfileTests(unittest.TestCase):
    def calibration_transform(self, anchor, elapsed, seconds=30.0, radius=5.0):
        # This guard gives an explicit failing contract before the production
        # function exists; the remaining assertions exercise its real output.
        transform = getattr(game_peer, "straight_walk_calibration_transform", None)
        self.assertTrue(callable(transform), "Missing straight walking calibration transform")
        return transform(anchor, elapsed, seconds, radius)

    def sweep_transform(self, anchor, elapsed, seconds=30.0, radius=5.0):
        transform = getattr(game_peer, "stride_speed_sweep_transform", None)
        self.assertTrue(callable(transform), "Missing stride speed measurement sweep")
        return transform(anchor, elapsed, seconds, radius)

    def test_stride_sweep_hand_checked_positions_and_stage_boundaries(self):
        samples = ((0, "idle_start", (7.0, -17.0, 3.0, 270.0)),
                   (2, "walk_slow", (7.0, -17.0, 3.0, 270.0)),
                   (8, "walk_slow", (10.0, -17.0, 3.0, 270.0)),
                   (14, "idle_turn_one", (13.0, -17.0, 3.0, 90.0)),
                   (16, "walk_medium", (13.0, -17.0, 3.0, 90.0)),
                   (19, "walk_medium", (10.0, -17.0, 3.0, 90.0)),
                   (22, "idle_turn_two", (7.0, -17.0, 3.0, 270.0)),
                   (24, "walk_fast", (7.0, -17.0, 3.0, 270.0)),
                   (26, "walk_fast", (10.0, -17.0, 3.0, 270.0)),
                   (28, "idle_end", (13.0, -17.0, 3.0, 270.0)),
                   (30, "idle_end", (13.0, -17.0, 3.0, 270.0)))
        for elapsed, expected_stage, expected_transform in samples:
            with self.subTest(elapsed=elapsed):
                self.assertEqual(self.sweep_transform(player(), elapsed),
                                 (expected_stage, expected_transform))

    def test_stride_sweep_continuous_positions_and_only_idle_heading_turns(self):
        for boundary in (2, 14, 16, 22, 24, 28):
            with self.subTest(boundary=boundary):
                before = self.sweep_transform(player(), boundary - 1e-9)[1]
                after = self.sweep_transform(player(), boundary + 1e-9)[1]
                self.assertLess(math.dist(before[:3], after[:3]), 1e-7)
                self.assertEqual(abs(before[3] - after[3]), 180.0 if boundary in (14, 22) else 0.0)

    def test_stride_sweep_three_speeds_are_measured_from_displacement(self):
        for first, last, speed in ((3, 4, 0.5), (17, 18, -1.0), (25, 26, 1.5)):
            with self.subTest(speed=speed):
                before = self.sweep_transform(player(), first)[1]
                after = self.sweep_transform(player(), last)[1]
                self.assertEqual((after[0] - before[0]) / (last - first), speed)
                self.assertEqual(after[1:3], before[1:3])

    def test_stride_sweep_idle_stages_cannot_drift(self):
        for start, last in ((0, 1.99), (14, 15.99), (22, 23.99), (28, 29.99)):
            with self.subTest(start=start):
                self.assertEqual(self.sweep_transform(player(), start),
                                 self.sweep_transform(player(), last))

    def test_stride_sweep_scaled_timing_and_lane_copy_anchor_height(self):
        anchor = player(x=-100, y=40, z=-7.25, heading=315)
        self.assertEqual(self.sweep_transform(anchor, 16, 60, 10),
                         ("walk_slow", (-100.0, 46.0, -7.25, 270.0)))
        self.assertEqual(self.sweep_transform(anchor, 38, 60, 10),
                         ("walk_medium", (-100.0, 46.0, -7.25, 90.0)))
        self.assertEqual(self.sweep_transform(anchor, 52, 60, 10),
                         ("walk_fast", (-100.0, 46.0, -7.25, 270.0)))
        for elapsed in range(61):
            value = self.sweep_transform(anchor, elapsed, 60, 10)[1]
            self.assertLessEqual(math.hypot(value[0] - anchor["x"], value[1] - anchor["y"]), math.sqrt(72))

    def test_stride_sweep_clamps_before_start_and_after_last_stage(self):
        self.assertEqual(self.sweep_transform(player(), -100), self.sweep_transform(player(), 0))
        self.assertEqual(self.sweep_transform(player(), 100), self.sweep_transform(player(), 30))

    def test_straight_calibration_cardinal_samples_have_hand_checked_positions(self):
        samples = ((0.0, "idle_start", (7.0, -17.0, 3.0, 270.0)),
                   (6.0, "walk_out", (7.0, -17.0, 3.0, 270.0)),
                   (9.0, "walk_out", (10.0, -17.0, 3.0, 270.0)),
                   (12.0, "idle_turn", (13.0, -17.0, 3.0, 90.0)),
                   (18.0, "walk_back", (13.0, -17.0, 3.0, 90.0)),
                   (21.0, "walk_back", (10.0, -17.0, 3.0, 90.0)),
                   (24.0, "idle_end", (7.0, -17.0, 3.0, 90.0)),
                   (30.0, "idle_end", (7.0, -17.0, 3.0, 90.0)))
        for elapsed, expected_stage, expected_transform in samples:
            with self.subTest(elapsed=elapsed):
                self.assertEqual(self.calibration_transform(player(), elapsed),
                                 (expected_stage, expected_transform))

    def test_straight_calibration_position_is_continuous_with_one_idle_heading_turn(self):
        for boundary in (6.0, 12.0, 18.0, 24.0):
            with self.subTest(boundary=boundary):
                before = self.calibration_transform(player(), boundary - 1e-9)[1]
                after = self.calibration_transform(player(), boundary + 1e-9)[1]
                self.assertLess(math.dist(before[:3], after[:3]), 1e-7)
                self.assertEqual(abs(before[3] - after[3]), 180.0 if boundary == 12.0 else 0.0)

    def test_straight_calibration_idle_stages_cannot_drift(self):
        for start, last in ((0.0, 5.99), (12.0, 17.99), (24.0, 29.99)):
            with self.subTest(start=start):
                self.assertEqual(self.calibration_transform(player(), start),
                                 self.calibration_transform(player(), last))

    def test_straight_calibration_speed_is_one_unit_per_second_in_both_directions(self):
        first = self.calibration_transform(player(), 7.0)[1]
        second = self.calibration_transform(player(), 8.0)[1]
        back_first = self.calibration_transform(player(), 19.0)[1]
        back_second = self.calibration_transform(player(), 20.0)[1]
        self.assertEqual(second[0] - first[0], 1.0)
        self.assertEqual(back_second[0] - back_first[0], -1.0)
        self.assertEqual(first[1:3], second[1:3])
        self.assertEqual(back_first[1:3], back_second[1:3])

    def test_straight_calibration_copies_height_and_ignores_anchor_facing(self):
        self.assertEqual(self.calibration_transform(player(x=-100.0, y=40.0, z=-7.25, heading=315.0),
                                                     9.0),
                         ("walk_out", (-100.0, 43.0, -7.25, 270.0)))
        self.assertEqual(self.calibration_transform(player(x=-100.0, y=40.0, z=-7.25, heading=315.0),
                                                     21.0),
                         ("walk_back", (-100.0, 43.0, -7.25, 90.0)))

    def test_straight_calibration_radius_twenty_remains_within_twenty_units(self):
        for elapsed, stage, transform in ((0.0, "idle_start", (-2.0, -8.0, 3.0, 270.0)),
                                          (32.0, "idle_turn", (22.0, -8.0, 3.0, 90.0)),
                                          (64.0, "idle_end", (-2.0, -8.0, 3.0, 90.0))):
            with self.subTest(elapsed=elapsed):
                self.assertEqual(self.calibration_transform(player(), elapsed, 80.0, 20.0),
                                 (stage, transform))
                self.assertLess(math.hypot(transform[0] - 10.0, transform[1] + 20.0), 20.0)

    def test_stage_output_cannot_claim_real_game_rendering(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            game_peer.emit("synthetic_profile_stage", profile="idle-walk-idle", stage="walk")
        event = json.loads(output.getvalue())
        self.assertTrue(event["synthetic"])
        self.assertFalse(event["game_rendering_verified"])

    def test_idle_walk_idle_position_and_heading_are_continuous_at_boundaries(self):
        anchor = player()
        for elapsed, expected_stage in ((0, "idle_start"), (5, "walk"), (25, "idle_end"), (30, "idle_end")):
            with self.subTest(elapsed=elapsed):
                stage, transform = game_peer.idle_walk_idle_transform(anchor, elapsed, 30.0, 5.0)
                self.assertEqual(stage, expected_stage)
                self.assertAlmostEqual(math.hypot(transform[0] - anchor["x"], transform[1] - anchor["y"]), 5.0)
                self.assertEqual(transform[2], anchor["z"])
        for boundary in (5.0, 25.0):
            before = game_peer.idle_walk_idle_transform(anchor, boundary - 1e-9, 30.0, 5.0)[1]
            after = game_peer.idle_walk_idle_transform(anchor, boundary + 1e-9, 30.0, 5.0)[1]
            self.assertLess(math.dist(before[:3], after[:3]), 1e-7)
            heading_distance = abs((before[3] - after[3] + 180.0) % 360.0 - 180.0)
            self.assertLess(heading_distance, 1e-7)
        self.assertEqual(game_peer.idle_walk_idle_transform(anchor, 0, 30.0, 5.0)[1],
                         game_peer.idle_walk_idle_transform(anchor, 4.99, 30.0, 5.0)[1])
        self.assertEqual(game_peer.idle_walk_idle_transform(anchor, 25, 30.0, 5.0)[1],
                         game_peer.idle_walk_idle_transform(anchor, 29.99, 30.0, 5.0)[1])


class ValidationTests(unittest.TestCase):
    def test_zero_position_and_inclusive_component_boundaries_are_valid(self):
        self.assertTrue(game_peer.valid(player(x=0, y=0, z=0, heading=0)))
        self.assertTrue(game_peer.valid(player(x=100000, y=-100000, z=-0.0, heading=100000)))

    def test_zero_identity_is_invalid(self):
        self.assertFalse(game_peer.valid(player(identity=0)))

    def test_nonfinite_and_unbounded_values_are_invalid_in_every_component(self):
        for field in ("x", "y", "z", "heading"):
            for value in (math.nan, math.inf, -math.inf, 100001, -100001):
                with self.subTest(field=field, value=value):
                    state = player()
                    state[field] = value
                    self.assertFalse(game_peer.valid(state))


class CommandLineTests(unittest.TestCase):
    def test_cli_accepts_stride_sweep_without_changing_default_timing(self):
        with mock.patch("sys.argv", ["game_peer.py", "--profile", "stride-speed-sweep"]), \
                mock.patch.object(game_peer, "run", return_value=0) as run, \
                contextlib.redirect_stderr(io.StringIO()):
            try:
                result = game_peer.main()
            except SystemExit as error:
                self.fail("Stride measurement profile must be accepted: " + str(error))
            self.assertEqual(result, 0)
            run.assert_called_once_with(game_peer.DEFAULT_BRIDGE, 30.0, 5.0, "stride-speed-sweep")

    def test_invalid_duration_is_rejected_without_running_bridge(self):
        for value in ("nan", "inf", "0", "301"):
            with self.subTest(value=value), mock.patch("sys.argv", ["game_peer.py", "--seconds", value]), \
                    mock.patch.object(game_peer, "run") as run, contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    game_peer.main()
                self.assertEqual(error.exception.code, 2)
                run.assert_not_called()

    def test_invalid_radius_is_rejected_without_running_bridge(self):
        for value in ("nan", "inf", "0", "21"):
            with self.subTest(value=value), mock.patch("sys.argv", ["game_peer.py", "--radius", value]), \
                    mock.patch.object(game_peer, "run") as run, contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    game_peer.main()
                self.assertEqual(error.exception.code, 2)
                run.assert_not_called()

    def test_cli_defaults_keep_experiment_short_and_five_units_away(self):
        with mock.patch("sys.argv", ["game_peer.py"]), mock.patch.object(game_peer, "run", return_value=0) as run:
            self.assertEqual(game_peer.main(), 0)
            run.assert_called_once_with(game_peer.DEFAULT_BRIDGE, 30.0, 5.0, "orbit")

    def test_cli_accepts_opt_in_idle_walk_idle_profile(self):
        with mock.patch("sys.argv", ["game_peer.py", "--profile", "idle-walk-idle"]), \
                mock.patch.object(game_peer, "run", return_value=0) as run:
            self.assertEqual(game_peer.main(), 0)
            run.assert_called_once_with(game_peer.DEFAULT_BRIDGE, 30.0, 5.0, "idle-walk-idle")

    def test_cli_accepts_opt_in_straight_walk_calibration_profile(self):
        with mock.patch("sys.argv", ["game_peer.py", "--profile", "straight-walk-calibration"]), \
                mock.patch.object(game_peer, "run", return_value=0) as run, \
                contextlib.redirect_stderr(io.StringIO()):
            try:
                result = game_peer.main()
            except SystemExit as error:
                self.fail("Calibration profile must be accepted by the CLI: " + str(error))
            self.assertEqual(result, 0)
            run.assert_called_once_with(game_peer.DEFAULT_BRIDGE, 30.0, 5.0, "straight-walk-calibration")

    def test_cli_unknown_profile_is_rejected_without_running_bridge(self):
        with mock.patch("sys.argv", ["game_peer.py", "--profile", "unknown"]), \
                mock.patch.object(game_peer, "run") as run, contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                game_peer.main()
            self.assertEqual(error.exception.code, 2)
            run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
