#!/usr/bin/env python3
# This file is part of the Multi Theft Auto project.
# SPDX-License-Identifier: GPL-3.0-or-later
"""A single-server transport prototype for a separate Definitive Edition client.

This deliberately small, standard-library server lets the new x64 client prove
session handling and transform replication before a game adapter is available.
It does not control GTA, implement MTA's wire protocol, or provide anti-cheat.
The executable loop accepts at most 64 incoming datagrams per 20 Hz tick; it
does not impose a separate per-client update-rate limit.
"""

import argparse
import json
import math
import secrets
import select
import socket
import struct
import time
from dataclasses import dataclass
from typing import Callable, Dict, List, Optional, Tuple


MAGIC = b"DES1"
VERSION = 1
JOIN, WELCOME, UPDATE, SNAPSHOT, LEAVE = range(1, 6)
MAX_DATAGRAM = 512
MAX_PLAYERS = 16
SNAPSHOT_HZ = 20
IDLE_SECONDS = 5.0
PACKETS_PER_TICK = 64
MAX_TRANSFORM_COMPONENT = 100000.0

HEADER = struct.Struct("<4sBBHI")
JOIN_BODY = struct.Struct("<Q")
WELCOME_BODY = struct.Struct("<QIQ")
UPDATE_BODY = struct.Struct("<IQffff")
SNAPSHOT_BODY = struct.Struct("<IQH")
PLAYER_BODY = struct.Struct("<Iffff")
LEAVE_BODY = struct.Struct("<IQ")

Address = Tuple[str, int]
Reply = Tuple[bytes, Address]
EventSink = Callable[[dict], None]


@dataclass
class Player:
    player_id: int
    nonce: int
    token: int
    address: Address
    last_seen: float
    transform: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0)
    last_sequence: Optional[int] = None


def packet(kind: int, sequence: int, body: bytes) -> bytes:
    return HEADER.pack(MAGIC, VERSION, kind, len(body), sequence & 0xFFFFFFFF) + body


def decode_packet(data: bytes) -> Optional[Tuple[int, int, bytes]]:
    if not HEADER.size <= len(data) <= MAX_DATAGRAM:
        return None
    magic, version, kind, body_length, sequence = HEADER.unpack_from(data)
    # Exact framing prevents truncated or concatenated payloads becoming a
    # different command when a later protocol version gains new message kinds.
    if (
        magic != MAGIC
        or version != VERSION
        or kind not in (JOIN, WELCOME, UPDATE, SNAPSHOT, LEAVE)
        or body_length != len(data) - HEADER.size
    ):
        return None
    return kind, sequence, data[HEADER.size:]


class Server:
    """Deterministic protocol state; callers provide monotonic time and I/O."""

    def __init__(self, event_sink: Optional[EventSink] = None):
        self.players: Dict[int, Player] = {}
        self.addresses: Dict[Address, int] = {}
        self._next_id = 1
        self._snapshot_sequence = 0
        self._event_sink = event_sink

    def _event(self, event: str, player: Player, **fields) -> None:
        if self._event_sink is not None:
            self._event_sink(
                dict(event=event, player_id=player.player_id,
                     address=player.address[0], port=player.address[1], **fields)
            )

    def _remove(self, player: Player, reason: str) -> None:
        del self.players[player.player_id]
        del self.addresses[player.address]
        self._event("leave", player, reason=reason)

    def _expire(self, now: float) -> None:
        for player in list(self.players.values()):
            if now - player.last_seen >= IDLE_SECONDS:
                self._remove(player, "timeout")

    def _authorized(self, player_id: int, token: int, address: Address) -> Optional[Player]:
        player = self.players.get(player_id)
        # An ID is public in snapshots, so possession of that ID alone must
        # never permit another endpoint to move or disconnect its owner.
        if player is None or token == 0 or player.token != token or player.address != address:
            return None
        return player

    def process(self, data: bytes, address: Address, now: float) -> List[Reply]:
        self._expire(now)
        decoded = decode_packet(data)
        if decoded is None:
            return []
        kind, sequence, body = decoded

        if kind == JOIN:
            if len(body) != JOIN_BODY.size:
                return []
            nonce, = JOIN_BODY.unpack(body)
            if nonce == 0:
                return []
            existing_id = self.addresses.get(address)
            player = self.players.get(existing_id) if existing_id is not None else None
            if player is not None and player.nonce == nonce:
                # UDP can drop a welcome; retrying the same join must retain
                # the session instead of continuously creating new identities.
                player.last_seen = now
            else:
                if player is None and len(self.players) >= MAX_PLAYERS:
                    return []
                if self._next_id > 0xFFFFFFFF:
                    return []
                if player is not None:
                    self._remove(player, "restart")
                token = 0
                while token == 0:
                    token = secrets.randbits(64)
                player = Player(self._next_id, nonce, token, address, now)
                self._next_id += 1
                self.players[player.player_id] = player
                self.addresses[address] = player.player_id
                self._event("join", player)
            welcome = WELCOME_BODY.pack(player.nonce, player.player_id, player.token)
            return [(packet(WELCOME, sequence, welcome), address)]

        if kind == UPDATE:
            if len(body) != UPDATE_BODY.size:
                return []
            player_id, token, x, y, z, heading = UPDATE_BODY.unpack(body)
            player = self._authorized(player_id, token, address)
            if player is None:
                return []
            transform = (x, y, z, heading)
            if not all(math.isfinite(value) and abs(value) <= MAX_TRANSFORM_COMPONENT for value in transform):
                return []
            if player.last_sequence is not None:
                # Modular ordering preserves UDP reordering protection when
                # the uint32 sequence counter wraps during a long connection.
                delta = (sequence - player.last_sequence) & 0xFFFFFFFF
                if not 0 < delta < 0x80000000:
                    return []
            player.transform = transform
            player.last_sequence = sequence
            player.last_seen = now

        elif kind == LEAVE:
            if len(body) != LEAVE_BODY.size:
                return []
            player_id, token = LEAVE_BODY.unpack(body)
            player = self._authorized(player_id, token, address)
            if player is not None:
                self._remove(player, "requested")

        return []

    def snapshots(self, now: float) -> List[Reply]:
        self._expire(now)
        self._snapshot_sequence = (self._snapshot_sequence + 1) & 0xFFFFFFFF
        players = sorted(self.players.values(), key=lambda player: player.player_id)
        # A JOIN establishes identity, not a game position. Publishing the
        # default transform would make adapters create a proxy at the origin
        # while a peer is still waiting for its first usable position. Keep
        # recipients alive with snapshots, but expose state only after an
        # accepted UPDATE; a deliberately submitted zero transform is valid.
        published = [player for player in players if player.last_sequence is not None]
        states = b"".join(PLAYER_BODY.pack(player.player_id, *player.transform) for player in published)
        replies = []
        for player in players:
            # Binding snapshots to the recipient's session lets a restarted
            # client discard datagrams left over from its previous connection.
            body = SNAPSHOT_BODY.pack(player.player_id, player.token, len(published)) + states
            replies.append((packet(SNAPSHOT, self._snapshot_sequence, body), player.address))
        return replies


def print_event(event: dict) -> None:
    print(json.dumps(event, sort_keys=True), flush=True)


def run(host: str, port: int, duration: Optional[float] = None) -> None:
    server = Server(print_event)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as transport:
        transport.bind((host, port))
        transport.setblocking(False)
        bound_host, bound_port = transport.getsockname()
        print_event(dict(event="listening", host=bound_host, port=bound_port,
                         protocol_version=VERSION, max_players=MAX_PLAYERS,
                         snapshot_hz=SNAPSHOT_HZ))
        started = time.monotonic()
        deadline = None if duration is None else started + duration
        next_tick = started
        processed = 0

        def send(replies: List[Reply]) -> None:
            for data, address in replies:
                try:
                    transport.sendto(data, address)
                except OSError:
                    # A failed UDP send must not remove the session; expiry
                    # handles vanished clients while subsequent ticks can retry.
                    continue

        try:
            while True:
                now = time.monotonic()
                if deadline is not None and now >= deadline:
                    break
                if now >= next_tick:
                    send(server.snapshots(now))
                    next_tick = now + 1.0 / SNAPSHOT_HZ
                    processed = 0
                timeout = max(0.0, next_tick - now)
                if deadline is not None:
                    timeout = min(timeout, max(0.0, deadline - now))
                # A noisy sender cannot starve snapshots: once this tick's
                # packet budget is spent, wait only for the next timer deadline.
                if processed >= PACKETS_PER_TICK:
                    # Windows select requires a socket, while keeping a flooded
                    # socket in its read set would return immediately and spin.
                    time.sleep(timeout)
                    continue
                readable, _, _ = select.select([transport], [], [], timeout)
                if not readable:
                    continue
                while processed < PACKETS_PER_TICK:
                    try:
                        data, address = transport.recvfrom(MAX_DATAGRAM + 1)
                    except BlockingIOError:
                        break
                    except OSError:
                        # Windows can report a UDP connection-reset ICMP error
                        # or an oversized datagram. Count consumed errors too
                        # so malformed traffic cannot bypass the tick budget.
                        processed += 1
                        break
                    processed += 1
                    send(server.process(data, address, time.monotonic()))
        except KeyboardInterrupt:
            pass
        finally:
            print_event(dict(event="stopped", players=len(server.players)))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="IPv4 bind address (default: loopback only)")
    parser.add_argument("--port", type=int, default=22040, help="UDP port (default: 22040; 0 chooses an available port)")
    parser.add_argument("--duration", type=float, help="Stop after this many seconds, for smoke tests")
    arguments = parser.parse_args()
    if not 0 <= arguments.port <= 65535:
        parser.error("--port must be between 0 and 65535")
    if arguments.duration is not None and (not math.isfinite(arguments.duration) or arguments.duration <= 0):
        parser.error("--duration must be a finite positive number")
    run(arguments.host, arguments.port, arguments.duration)


if __name__ == "__main__":
    main()
