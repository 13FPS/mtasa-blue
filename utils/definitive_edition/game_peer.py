#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic transport peer for an already running local DE adapter experiment.

Run Server/definitive/server.py separately on 127.0.0.1:22040, then load the
game's online adapter. This helper is not a second GTA client and cannot verify
rendering. Its default orbit follows the first nonzero remote transform with
a five-unit circle. The opt-in idle-walk-idle profile holds the first received
anchor fixed, letting a manual observer distinguish idle and moving animation.
The opt-in straight-walk-calibration profile separates facing and stride from
orbit turning. Its cardinal headings are candidates until visually checked in DE.
The stride-speed-sweep compares three translation speeds on that same lane;
it measures translation versus playback and does not establish foot contact.
"""

import argparse
import ctypes
import json
import math
import os
from pathlib import Path
import secrets
import time
from typing import Optional


REPOSITORY = Path(__file__).resolve().parents[2]
DEFAULT_BRIDGE = REPOSITORY / "Build/definitive-online/de_client_bridge.dll"
HOST = b"127.0.0.1"
PORT = 22040
MAX_PLAYERS = 16
UPDATE_INTERVAL = 0.05
RADIUS = 5.0
PROFILES = ("orbit", "idle-walk-idle", "straight-walk-calibration", "stride-speed-sweep")
CALIBRATION_STAGES = ("idle_start", "walk_out", "idle_turn", "walk_back", "idle_end")
STRIDE_STAGES = ("idle_start", "walk_slow", "idle_turn_one", "walk_medium",
                 "idle_turn_two", "walk_fast", "idle_end")
STRIDE_STAGE_WEIGHTS = (1, 6, 1, 3, 1, 2, 1)


class Player(ctypes.Structure):
    _fields_ = [("id", ctypes.c_uint32), ("x", ctypes.c_float),
                ("y", ctypes.c_float), ("z", ctypes.c_float),
                ("heading", ctypes.c_float)]


def emit(event: str, **fields) -> None:
    # Every record explicitly distinguishes this helper from a real DE client.
    print(json.dumps(dict(event=event, synthetic=True, game_rendering_verified=False,
                          **fields), sort_keys=True), flush=True)


class Bridge:
    def __init__(self, path: Path):
        if os.name != "nt" or ctypes.sizeof(ctypes.c_void_p) != 8:
            raise RuntimeError("The helper requires Windows and a 64-bit Python runtime")
        self.library = ctypes.CDLL(str(path.resolve(strict=True)))
        self.library.DE_ApiVersion.argtypes = []
        self.library.DE_ApiVersion.restype = ctypes.c_uint32
        self.library.DE_Connect.argtypes = [ctypes.c_char_p, ctypes.c_uint16, ctypes.c_uint64]
        self.library.DE_Connect.restype = ctypes.c_int
        self.library.DE_Pump.argtypes = []
        self.library.DE_Pump.restype = ctypes.c_int
        self.library.DE_LocalPlayerId.argtypes = []
        self.library.DE_LocalPlayerId.restype = ctypes.c_uint32
        self.library.DE_SubmitTransform.argtypes = [ctypes.c_float] * 4
        self.library.DE_SubmitTransform.restype = ctypes.c_int
        self.library.DE_ReadPlayers.argtypes = [ctypes.POINTER(Player), ctypes.c_uint32]
        self.library.DE_ReadPlayers.restype = ctypes.c_uint32
        self.library.DE_Disconnect.argtypes = []
        self.library.DE_Disconnect.restype = None
        if self.library.DE_ApiVersion() != 2 or ctypes.sizeof(Player) != 20:
            raise RuntimeError("Bridge ABI version 2 and a 20-byte player structure are required")
        self.states = (Player * MAX_PLAYERS)()

    def connect(self) -> None:
        if self.library.DE_Connect(HOST, PORT, secrets.randbits(64) or 1) != 1:
            raise RuntimeError("Could not open the fixed loopback transport")

    def players(self):
        count = self.library.DE_ReadPlayers(self.states, MAX_PLAYERS)
        if count > MAX_PLAYERS:
            raise RuntimeError("Bridge returned too many player states")
        return [dict(id=self.states[index].id, x=self.states[index].x,
                     y=self.states[index].y, z=self.states[index].z,
                     heading=self.states[index].heading) for index in range(count)]

    def disconnect(self) -> None:
        self.library.DE_Disconnect()


def valid(player: dict) -> bool:
    return player["id"] != 0 and all(
        math.isfinite(player[field]) and abs(player[field]) <= 100000
        for field in ("x", "y", "z", "heading"))


def idle_walk_idle_transform(anchor: dict, elapsed: float, seconds: float,
                             radius: float):
    """Keep position and heading continuous across a 1:4:1 timing profile."""
    idle_seconds = seconds / 6.0
    walk_seconds = seconds * 4.0 / 6.0
    if elapsed < idle_seconds:
        stage = "idle_start"
    elif elapsed < idle_seconds + walk_seconds:
        stage = "walk"
    else:
        stage = "idle_end"
    # Advance the same orbit angle only during the middle stage. Freezing its
    # final value avoids a teleport or heading snap when the proxy stops.
    moving_elapsed = min(max(elapsed - idle_seconds, 0.0), walk_seconds)
    angle = math.radians(anchor["heading"]) + moving_elapsed * 0.75
    transform = (anchor["x"] + radius * math.cos(angle),
                 anchor["y"] + radius * math.sin(angle), anchor["z"],
                 (math.degrees(angle) + 90.0) % 360.0)
    return stage, transform


def straight_walk_calibration_transform(anchor: dict, elapsed: float,
                                        seconds: float, radius: float):
    """Walk one bounded cardinal lane, with an idle turn before the return leg."""
    stage_seconds = seconds / 5.0
    stage_index = min(max(int(elapsed / stage_seconds), 0), 4)
    stage = CALIBRATION_STAGES[stage_index]
    # The offset lane avoids crossing CJ. Its endpoints stay inside the given
    # radius, while a fixed anchor isolates the test from the human's movement.
    outbound = min(max((elapsed - stage_seconds) / stage_seconds, 0.0), 1.0)
    inbound = min(max((elapsed - 3.0 * stage_seconds) / stage_seconds, 0.0), 1.0)
    x = anchor["x"] - 0.6 * radius + 1.2 * radius * (outbound - inbound)
    # Classic heading conventions suggest +X=270 and -X=90. Turning once while
    # idle makes a DE facing mismatch visible without correcting game-client
    # headings or treating the old orbit's perpendicular heading as proof.
    heading = 270.0 if elapsed < 2.0 * stage_seconds else 90.0
    return stage, (x, anchor["y"] + 0.6 * radius, anchor["z"], heading)


def stride_speed_sweep_transform(anchor: dict, elapsed: float,
                                seconds: float, radius: float):
    """Compare three speeds without changing lane length or moving through CJ."""
    phase = max(0.0, elapsed * 15.0 / seconds)
    boundaries = (1, 7, 8, 11, 12, 14, 15)
    stage_index = next((index for index, end in enumerate(boundaries) if phase < end), 6)
    slow = min(max((phase - 1.0) / 6.0, 0.0), 1.0)
    medium = min(max((phase - 8.0) / 3.0, 0.0), 1.0)
    fast = min(max((phase - 12.0) / 2.0, 0.0), 1.0)
    # All legs traverse the previously tested endpoints; changing their duration
    # isolates speed from terrain and camera distance without modifying the NPC.
    x = anchor["x"] - 0.6 * radius + 1.2 * radius * (slow - medium + fast)
    heading = 90.0 if 7.0 <= phase < 11.0 else 270.0
    return STRIDE_STAGES[stage_index], (x, anchor["y"] + 0.6 * radius, anchor["z"], heading)


def run(bridge_path: Path, seconds: float, radius: float = RADIUS,
        profile: str = "orbit") -> int:
    bridge: Optional[Bridge] = None
    started = time.monotonic()
    remote_id = None
    remote_started = 0.0
    remote_positions = set()
    sent_positions = set()
    updates = 0
    departed = False
    own_id = 0
    failure = None
    cleanup_failure = None
    fixed_anchor = None
    last_stage = None
    stage_updates = {}
    profile_complete = False
    try:
        if profile not in PROFILES:
            raise RuntimeError("Unknown synthetic motion profile: " + str(profile))
        calibration_fields = {}
        if profile == "straight-walk-calibration":
            # Reject unsuitable timing before loading the DLL: an extreme
            # derived speed would silently defeat the walking inference being
            # calibrated. These bounds describe this probe, not GTA constants.
            if (not math.isfinite(seconds) or not 1 <= seconds <= 300
                    or not math.isfinite(radius) or not 0.5 <= radius <= 20):
                raise RuntimeError("Calibration needs finite seconds in 1..300 and radius in 0.5..20")
            speed = 6.0 * radius / seconds
            if not 0.5 <= speed <= 1.5:
                raise RuntimeError("Calibration speed must be between 0.5 and 1.5 units per second")
            calibration_fields = {
                "profile_speed_units_per_second": speed,
                "heading_basis": "classic_cardinal_candidate_not_game_verified",
            }
        elif profile == "stride-speed-sweep":
            # Validate every leg before constructing Bridge. A slow leg below
            # walking inference or an excessive fast leg would confound cadence
            # measurement while making a superficially successful transport run.
            if (not math.isfinite(seconds) or not 1 <= seconds <= 300
                    or not math.isfinite(radius) or not 0.5 <= radius <= 20):
                raise RuntimeError("Stride sweep needs finite seconds in 1..300 and radius in 0.5..20")
            speeds = dict(zip(("walk_slow", "walk_medium", "walk_fast"),
                              (factor * radius / seconds for factor in (3.0, 6.0, 9.0))))
            if not all(0.25 <= speed <= 1.5 for speed in speeds.values()):
                raise RuntimeError("All stride sweep speeds must be between 0.25 and 1.5 units per second")
            calibration_fields = {
                "profile_speeds_units_per_second": speeds,
                "heading_basis": "classic_cardinal_candidate_not_game_verified",
            }
        bridge = Bridge(bridge_path)
        bridge.connect()
        session_started = started
        next_update = started
        emit("synthetic_peer_started", bridge=str(bridge_path.resolve()),
             endpoint="127.0.0.1:22040", seconds=seconds, radius=radius,
             profile=profile,
             anchor_mode="first_remote_transform_fixed" if profile != "orbit" else "latest_remote_transform",
             height_source="copied_anchor_z_without_terrain_query",
             profile_stage_seconds=([seconds / 6.0, seconds * 4.0 / 6.0, seconds / 6.0]
                                    if profile == "idle-walk-idle" else
                                    [seconds / 5.0] * 5 if profile == "straight-walk-calibration" else
                                    [seconds * weight / 15.0 for weight in STRIDE_STAGE_WEIGHTS]
                                    if profile == "stride-speed-sweep" else None),
             waiting_for_anchor_consumes_duration=True,
             remote_identity_source="inferred_from_transport_only", **calibration_fields)
        while time.monotonic() - started < seconds:
            now = time.monotonic()
            # The server expires sessions without updates after five seconds.
            # Rejoin before that while waiting; publishing placeholder movement
            # would otherwise make a convincing-looking proxy at the map origin.
            if remote_id is None and now - session_started >= 4.0:
                bridge.disconnect()
                bridge.connect()
                session_started = now
            if bridge.library.DE_Pump() != 1:
                raise RuntimeError("Local UDP pump failed; ensure the loopback server is running")
            own_id = bridge.library.DE_LocalPlayerId()
            if not own_id:
                time.sleep(0.01)
                continue
            peers = [player for player in bridge.players() if player["id"] != own_id and valid(player)]
            if remote_id is None:
                # The relay now publishes only accepted updates. A true origin
                # transform is valid, but this visual experiment intentionally
                # waits for a nonzero anchor rather than orbiting the map origin.
                # A transport peer remains only an inferred game identity.
                candidates = [player for player in peers if any(player[field] != 0 for field in ("x", "y", "z"))]
                if not candidates:
                    time.sleep(0.01)
                    continue
                remote = min(candidates, key=lambda player: player["id"])
                remote_id = remote["id"]
                remote_started = now
                # A fixed snapshot isolates synthetic idle from user movement;
                # continue reading the live peer to observe its departure. This
                # copied height is not a terrain sample at the offset position.
                if profile != "orbit":
                    fixed_anchor = dict(remote)
                emit("remote_transport_observed", synthetic_player_id=own_id,
                     remote_player_id=remote_id, remote_transform=remote,
                     remote_identity_source="inferred_from_transport_only")
            else:
                remote = next((player for player in peers if player["id"] == remote_id), None)
                if remote is None:
                    departed = True
                    emit("remote_transport_departed", remote_player_id=remote_id)
                    break
            remote_positions.add(tuple(round(remote[field], 3) for field in ("x", "y", "z")))
            if now >= next_update:
                if profile == "idle-walk-idle":
                    stage, transform = idle_walk_idle_transform(
                        fixed_anchor, now - remote_started, seconds, radius)
                elif profile == "straight-walk-calibration":
                    stage, transform = straight_walk_calibration_transform(
                        fixed_anchor, now - remote_started, seconds, radius)
                elif profile == "stride-speed-sweep":
                    stage, transform = stride_speed_sweep_transform(
                        fixed_anchor, now - remote_started, seconds, radius)
                else:
                    angle = math.radians(remote["heading"]) + (now - remote_started) * 0.75
                    transform = (remote["x"] + radius * math.cos(angle),
                                 remote["y"] + radius * math.sin(angle), remote["z"],
                                 (math.degrees(angle) + 90.0) % 360.0)
                if bridge.library.DE_SubmitTransform(*transform) != 1:
                    raise RuntimeError("Synthetic transform submission failed")
                if profile != "orbit":
                    stage_updates[stage] = stage_updates.get(stage, 0) + 1
                    if stage != last_stage:
                        # Bounded stage records describe only submitted state;
                        # failed submissions must not look like played stages.
                        emit("synthetic_profile_stage", profile=profile, stage=stage,
                             seconds_after_anchor=round(now - remote_started, 3),
                             fixed_anchor=fixed_anchor,
                             submitted_transform=dict(zip(("x", "y", "z", "heading"), transform)))
                        last_stage = stage
                sent_positions.add(tuple(round(value, 3) for value in transform[:3]))
                updates += 1
                next_update = now + UPDATE_INTERVAL
            time.sleep(0.01)
    except KeyboardInterrupt:
        failure = "Interrupted by user"
    except (OSError, RuntimeError, AttributeError) as error:
        failure = str(error)
    finally:
        if bridge is not None:
            # This explicit departure is also executed on failed joins or Ctrl+C;
            # report cleanup failure without hiding the original runtime error
            # or suppressing the result needed to assess a bounded experiment.
            try:
                bridge.disconnect()
            except (OSError, RuntimeError, AttributeError) as error:
                cleanup_failure = str(error)
                if failure is None:
                    failure = "Disconnect failed: " + cleanup_failure
        # A late anchor or departure can still submit multiple positions. The
        # calibration must not report success unless every probe stage was sent.
        # Keep older profiles' exit behavior intact for historical comparability.
        expected_stages = STRIDE_STAGES if profile == "stride-speed-sweep" else CALIBRATION_STAGES
        profile_complete = (tuple(stage_updates) == expected_stages
                            and all(count > 0 for count in stage_updates.values()))
        completion_fields = ({"profile_complete": profile_complete}
                             if profile in ("straight-walk-calibration", "stride-speed-sweep") else {})
        emit("synthetic_peer_result", synthetic_player_id=own_id,
             remote_player_id=remote_id, remote_observed=remote_id is not None,
             remote_distinct_positions=len(remote_positions),
             remote_motion_observed=len(remote_positions) >= 2,
             synthetic_distinct_positions=len(sent_positions),
             synthetic_motion_sent=len(sent_positions) >= 2,
             updates_sent=updates, remote_departure_observed=departed,
             elapsed_seconds=round(time.monotonic() - started, 3), error=failure,
             cleanup_error=cleanup_failure,
             profile=profile, profile_stage_updates=stage_updates,
             remote_identity_source="inferred_from_transport_only", **completion_fields)
    return 0 if (failure is None and remote_id is not None and len(sent_positions) >= 2
                 and (profile not in ("straight-walk-calibration", "stride-speed-sweep") or profile_complete)
                 # A departure after the last stage was submitted still ends the
                 # new measurement early. Preserve older profiles' exit behavior.
                 and (profile != "stride-speed-sweep" or not departed)) else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, default=30.0,
                        help="Experiment duration, 1..300 seconds (default: 30)")
    parser.add_argument("--bridge", type=Path, default=DEFAULT_BRIDGE,
                        help="Standalone x64 ABI-v2 bridge DLL")
    parser.add_argument("--radius", type=float, default=RADIUS,
                        help="Synthetic peer distance, 0.5..20 game units (default: 5)")
    parser.add_argument("--profile", choices=PROFILES, default="orbit",
                        help="Motion profile (default: orbit); opt-ins fix the first anchor; straight-walk-calibration uses five equal stages at 0.5..1.5 units/sec; stride-speed-sweep measures three speeds, all within 0.25..1.5 units/sec")
    arguments = parser.parse_args()
    if not math.isfinite(arguments.seconds) or not 1 <= arguments.seconds <= 300:
        parser.error("--seconds must be finite and between 1 and 300")
    if not math.isfinite(arguments.radius) or not 0.5 <= arguments.radius <= 20:
        parser.error("--radius must be finite and between 0.5 and 20")
    return run(arguments.bridge, arguments.seconds, arguments.radius, arguments.profile)


if __name__ == "__main__":
    raise SystemExit(main())
