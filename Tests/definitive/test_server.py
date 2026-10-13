# This file is part of the Multi Theft Auto project.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the transport without launching or modifying either GTA install."""

import importlib.util
import json
import math
from pathlib import Path
import socket
import struct
import subprocess
import sys
import time
import unittest
from unittest import mock


SERVER_PATH = Path(__file__).resolve().parents[2] / "Server" / "definitive" / "server.py"
SPEC = importlib.util.spec_from_file_location("definitive_server", SERVER_PATH)
server = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = server
SPEC.loader.exec_module(server)


def wire(kind, body=b"", sequence=0):
    # Keep test encoding independent from the implementation so changes in
    # framing cannot silently invalidate the separate C++ client's contract.
    return struct.pack("<4sBBHI", b"DES1", 1, kind, len(body), sequence) + body


def decode(data):
    magic, version, kind, length, sequence = struct.unpack_from("<4sBBHI", data)
    if (magic, version, length) != (b"DES1", 1, len(data) - 12):
        raise AssertionError("Invalid reply framing")
    return kind, sequence, data[12:]


class ServerTests(unittest.TestCase):
    def setUp(self):
        self.events = []
        self.subject = server.Server(self.events.append)
        self.address = ("127.0.0.1", 32000)
        self.other = ("127.0.0.1", 32001)

    def join(self, address=None, nonce=100, now=0, sequence=0):
        address = self.address if address is None else address
        replies = self.subject.process(wire(1, struct.pack("<Q", nonce), sequence), address, now)
        self.assertEqual(len(replies), 1)
        data, destination = replies[0]
        self.assertEqual(destination, address)
        kind, reply_sequence, body = decode(data)
        self.assertEqual((kind, reply_sequence), (2, sequence))
        returned_nonce, identity, token = struct.unpack("<QIQ", body)
        self.assertEqual(returned_nonce, nonce)
        self.assertNotEqual(identity, 0)
        self.assertNotEqual(token, 0)
        return identity, token

    def update(self, identity, token, transform=(1, 2, 3, 4), sequence=1, address=None, now=0):
        address = self.address if address is None else address
        data = wire(3, struct.pack("<IQffff", identity, token, *transform), sequence)
        return self.subject.process(data, address, now)

    def snapshot_states(self, now=0):
        snapshots = self.subject.snapshots(now)
        result = {}
        for data, address in snapshots:
            self.assertLessEqual(len(data), 512)
            kind, sequence, body = decode(data)
            self.assertEqual(kind, 4)
            identity, token, count = struct.unpack_from("<IQH", body)
            self.assertEqual(len(body), 14 + count * 20)
            states = [struct.unpack_from("<Iffff", body, 14 + index * 20) for index in range(count)]
            result[address] = (identity, token, sequence, states)
        return result

    def test_join_assigns_server_identity_and_nonzero_token(self):
        with mock.patch.object(server.secrets, "randbits", side_effect=[0, 987]):
            identity, token = self.join(sequence=42)
        self.assertEqual((identity, token), (1, 987))
        self.assertEqual(self.events[0]["event"], "join")

    def test_repeated_join_preserves_session_transform_and_sequence(self):
        identity, token = self.join()
        self.update(identity, token, sequence=12, now=1)
        self.assertEqual(self.join(now=4, sequence=99), (identity, token))
        self.assertEqual(self.subject.players[identity].last_sequence, 12)
        self.assertEqual(self.subject.players[identity].transform, (1, 2, 3, 4))
        self.assertEqual(self.subject.players[identity].last_seen, 4)
        self.assertEqual(len(self.events), 1)

    def test_changed_nonce_replaces_own_session_and_rejects_old_packets(self):
        old_identity, old_token = self.join()
        self.update(old_identity, old_token)
        self.assertEqual(self.snapshot_states()[self.address][3], [(old_identity, 1, 2, 3, 4)])
        new_identity, new_token = self.join(nonce=101, now=1)
        self.assertGreater(new_identity, old_identity)
        self.assertNotIn(old_identity, self.subject.players)
        self.update(old_identity, old_token, now=2)
        self.subject.process(wire(5, struct.pack("<IQ", old_identity, old_token)), self.address, 2)
        self.assertEqual(self.subject.players[new_identity].transform, (0, 0, 0, 0))
        self.assertEqual(self.subject.players[new_identity].token, new_token)
        self.assertEqual([event.get("reason") for event in self.events if event["event"] == "leave"], ["restart"])
        self.assertEqual(self.snapshot_states(2)[self.address][3], [])

    def test_same_nonce_at_different_endpoint_gets_distinct_identity(self):
        first, _ = self.join()
        second, _ = self.join(self.other)
        self.assertNotEqual(first, second)
        self.assertEqual(len(self.subject.players), 2)

    def test_snapshots_share_sorted_states_but_bind_recipient_sessions(self):
        first, first_token = self.join()
        second, second_token = self.join(self.other, nonce=101)
        self.update(second, second_token, (8, 7, 6, 5), address=self.other)
        self.update(first, first_token, (-1, -2, -3, -4))
        snapshots = self.snapshot_states()
        first_snapshot, second_snapshot = snapshots[self.address], snapshots[self.other]
        self.assertEqual(first_snapshot[:2], (first, first_token))
        self.assertEqual(second_snapshot[:2], (second, second_token))
        self.assertEqual(first_snapshot[2:], second_snapshot[2:])
        self.assertEqual(first_snapshot[3], [(first, -1, -2, -3, -4), (second, 8, 7, 6, 5)])

    def test_uninitialized_recipient_receives_empty_session_bound_snapshot(self):
        identity, token = self.join()
        snapshot = self.snapshot_states()[self.address]
        self.assertEqual(snapshot[:2], (identity, token))
        self.assertEqual(snapshot[3], [])
        self.assertIn(identity, self.subject.players)
        self.assertIsNone(self.subject.players[identity].last_sequence)

    def test_first_accepted_update_publishes_to_initialized_and_waiting_peers(self):
        identity, token = self.join()
        waiting, waiting_token = self.join(self.other, nonce=101)
        self.assertTrue(all(snapshot[3] == [] for snapshot in self.snapshot_states().values()))
        self.update(identity, token, (-1, -2, 3, 90), sequence=0)
        snapshots = self.snapshot_states()
        self.assertEqual(snapshots[self.other][:2], (waiting, waiting_token))
        for snapshot in snapshots.values():
            self.assertEqual(snapshot[3], [(identity, -1, -2, 3, 90)])

    def test_accepted_zero_transform_is_published_as_real_player_state(self):
        identity, token = self.join()
        self.update(identity, token, (0, 0, 0, 0), sequence=0)
        self.assertEqual(self.snapshot_states()[self.address][3], [(identity, 0, 0, 0, 0)])

    def test_rejected_updates_do_not_publish_join_placeholder(self):
        identity, token = self.join()
        rejected = [
            (wire(3, struct.pack("<IQffff", identity, token ^ 1, 1, 2, 3, 4), 1), self.address),
            (wire(3, struct.pack("<IQffff", identity, token, 1, 2, 3, 4), 1), self.other),
            (wire(3, struct.pack("<IQffff", identity, token, math.nan, 2, 3, 4), 1), self.address),
            (wire(3, struct.pack("<IQffff", identity, token, 100001, 2, 3, 4), 1), self.address),
            (wire(3, struct.pack("<IQffff", identity, token, 1, 2, 3, 4)[:-1], 1), self.address),
        ]
        for data, address in rejected:
            with self.subTest(data=data, address=address):
                self.subject.process(data, address, 0)
                self.assertIsNone(self.subject.players[identity].last_sequence)
                self.assertEqual(self.snapshot_states()[self.address][3], [])

    def test_snapshot_sequence_advances_and_wraps(self):
        self.join()
        first = self.snapshot_states()[self.address][2]
        second = self.snapshot_states()[self.address][2]
        self.assertEqual(second, first + 1)
        self.subject._snapshot_sequence = 0xFFFFFFFF
        self.assertEqual(self.snapshot_states()[self.address][2], 0)

    def test_wrong_source_id_and_token_cannot_update_or_refresh_idle(self):
        identity, token = self.join()
        for invalid_id, invalid_token, address in (
            (identity, token, self.other), (identity, token ^ 1, self.address),
            (identity, 0, self.address), (0, token, self.address),
            (identity + 500, token, self.address),
        ):
            with self.subTest(identity=invalid_id, token=invalid_token, address=address):
                self.update(invalid_id, invalid_token, address=address, now=4)
                self.assertEqual(self.subject.players[identity].last_seen, 0)
                self.assertEqual(self.subject.players[identity].transform, (0, 0, 0, 0))
        self.assertEqual(self.subject.snapshots(5), [])

    def test_stale_update_cannot_refresh_idle(self):
        identity, token = self.join()
        self.update(identity, token, sequence=4, now=1)
        self.update(identity, token, (8, 8, 8, 8), sequence=4, now=5)
        self.assertEqual(self.subject.players[identity].last_seen, 1)
        self.assertEqual(self.subject.snapshots(6), [])

    def test_nonfinite_or_unbounded_component_is_rejected_in_every_field(self):
        identity, token = self.join()
        for value in (math.nan, math.inf, -math.inf, 100001, -100001):
            for index in range(4):
                with self.subTest(value=value, index=index):
                    values = [1, 2, 3, 4]
                    values[index] = value
                    self.update(identity, token, values, now=4)
                    self.assertEqual(self.subject.players[identity].transform, (0, 0, 0, 0))
                    self.assertIsNone(self.subject.players[identity].last_sequence)
                    self.assertEqual(self.subject.players[identity].last_seen, 0)

    def test_transform_boundary_values_are_accepted(self):
        identity, token = self.join()
        values = (100000, -100000, 0, -0.0)
        self.update(identity, token, values)
        self.assertEqual(self.subject.players[identity].transform, values)

    def test_duplicate_late_and_half_range_sequences_are_rejected(self):
        identity, token = self.join()
        self.update(identity, token, sequence=10)
        for sequence in (10, 9, 0x8000000A, 0xFFFFFFFF):
            with self.subTest(sequence=sequence):
                self.update(identity, token, (9, 9, 9, 9), sequence=sequence)
                self.assertEqual(self.subject.players[identity].transform, (1, 2, 3, 4))
                self.assertEqual(self.subject.players[identity].last_sequence, 10)
        self.update(identity, token, (5, 6, 7, 8), sequence=11)
        self.assertEqual(self.subject.players[identity].transform, (5, 6, 7, 8))

    def test_update_sequence_accepts_wrap_and_rejects_old_pre_wrap_packet(self):
        identity, token = self.join()
        self.update(identity, token, sequence=0xFFFFFFFE)
        self.update(identity, token, (5, 6, 7, 8), sequence=0)
        self.update(identity, token, (9, 9, 9, 9), sequence=0xFFFFFFFF)
        self.assertEqual(self.subject.players[identity].last_sequence, 0)
        self.assertEqual(self.subject.players[identity].transform, (5, 6, 7, 8))

    def test_first_update_accepts_any_uint32_sequence(self):
        for sequence in (0, 0x80000000, 0xFFFFFFFF):
            with self.subTest(sequence=sequence):
                identity, token = self.join(nonce=sequence + 1)
                self.update(identity, token, sequence=sequence)
                self.assertEqual(self.subject.players[identity].last_sequence, sequence)

    def test_authorized_leave_removes_player_from_every_snapshot(self):
        identity, token = self.join()
        remaining, remaining_token = self.join(self.other, nonce=101)
        self.update(identity, token)
        self.update(remaining, remaining_token, address=self.other)
        self.subject.process(wire(5, struct.pack("<IQ", identity, token)), self.address, 1)
        states = self.snapshot_states(1)
        self.assertEqual(set(states), {self.other})
        self.assertEqual([state[0] for state in states[self.other][3]], [remaining])
        self.assertEqual(self.events[-1]["reason"], "requested")

    def test_wrong_sender_or_session_cannot_leave(self):
        identity, token = self.join()
        for invalid_id, invalid_token, address in (
            (identity, token, self.other), (identity, token ^ 1, self.address),
            (0, token, self.address), (identity, 0, self.address),
        ):
            self.subject.process(wire(5, struct.pack("<IQ", invalid_id, invalid_token)), address, 1)
            self.assertIn(identity, self.subject.players)

    def test_timeout_at_five_seconds_removes_session_and_address(self):
        identity, _ = self.join()
        self.assertTrue(self.subject.snapshots(4.999))
        self.assertEqual(self.subject.snapshots(5), [])
        self.assertNotIn(identity, self.subject.players)
        self.assertNotIn(self.address, self.subject.addresses)
        self.assertEqual(self.events[-1]["reason"], "timeout")
        next_identity, _ = self.join(now=6)
        self.assertGreater(next_identity, identity)

    def test_expired_token_cannot_resurrect_session_before_snapshot_tick(self):
        identity, token = self.join()
        self.update(identity, token, now=5)
        self.assertNotIn(identity, self.subject.players)

    def test_valid_join_retry_keeps_session_alive(self):
        identity, token = self.join()
        self.assertEqual(self.join(now=4), (identity, token))
        self.assertTrue(self.subject.snapshots(8.999))
        self.assertEqual(self.subject.snapshots(9), [])

    def test_full_server_allows_own_restart_but_rejects_new_endpoint(self):
        sessions = [self.join(("127.0.0.1", 32000 + index), nonce=index + 1) for index in range(16)]
        for index, (identity, token) in enumerate(sessions):
            self.update(identity, token, address=("127.0.0.1", 32000 + index))
        denied = self.subject.process(wire(1, struct.pack("<Q", 999)), ("127.0.0.1", 40000), 1)
        self.assertEqual(denied, [])
        self.assertEqual(len(self.subject.players), 16)
        restarted, restarted_token = self.join(nonce=999, now=1)
        self.assertGreater(restarted, sessions[-1][0])
        self.assertEqual(len(self.subject.players), 16)
        self.update(restarted, restarted_token, now=1)
        snapshots = self.snapshot_states(1)
        self.assertEqual(len(snapshots), 16)
        self.assertEqual(len(snapshots[self.address][3]), 16)

    def test_expiry_frees_full_server_slots(self):
        for index in range(16):
            self.join(("127.0.0.1", 32000 + index), nonce=index + 1)
        identity, _ = self.join(("127.0.0.1", 40000), nonce=999, now=5)
        self.assertEqual(len(self.subject.players), 1)
        self.assertEqual(identity, 17)

    def test_zero_nonce_and_wrong_join_body_lengths_are_rejected(self):
        for body in (struct.pack("<Q", 0), b"", b"a" * 7, b"a" * 9):
            self.assertEqual(self.subject.process(wire(1, body), self.address, 0), [])
        self.assertEqual(self.subject.players, {})

    def test_malformed_datagrams_do_not_mutate_live_session(self):
        identity, token = self.join()
        valid = wire(3, struct.pack("<IQffff", identity, token, 1, 2, 3, 4), 1)
        malformed = [b"", b"x", valid[:11], valid[:-1], valid + b"x", b"x" * 513]
        for offset, value in ((0, 0), (4, 2), (5, 0), (5, 6), (6, 27), (7, 1)):
            altered = bytearray(valid)
            altered[offset] = value
            malformed.append(bytes(altered))
        for data in malformed:
            with self.subTest(data=data):
                self.assertEqual(self.subject.process(data, self.address, 4), [])
                self.assertEqual(self.subject.players[identity].transform, (0, 0, 0, 0))
                self.assertEqual(self.subject.players[identity].last_seen, 0)

    def test_wrong_update_and_leave_body_lengths_are_rejected(self):
        identity, token = self.join()
        for kind, body in ((3, struct.pack("<IQffff", identity, token, 1, 2, 3, 4)),
                           (5, struct.pack("<IQ", identity, token))):
            for altered in (body[:-1], body + b"x"):
                self.subject.process(wire(kind, altered), self.address, 4)
                self.assertIn(identity, self.subject.players)
                self.assertEqual(self.subject.players[identity].last_seen, 0)

    def test_server_only_message_kinds_cannot_change_or_refresh_player(self):
        identity, token = self.join()
        for kind in (2, 4):
            self.subject.process(wire(kind, struct.pack("<IQ", identity, token)), self.address, 4)
        self.assertEqual(self.subject.players[identity].last_seen, 0)
        self.assertEqual(self.subject.snapshots(5), [])


class ServerProcessTests(unittest.TestCase):
    def test_cli_uses_loopback_ephemeral_port_and_transmits_snapshots(self):
        process = subprocess.Popen(
            [sys.executable, "-B", str(SERVER_PATH), "--port", "0", "--duration", "1.5"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            startup = json.loads(process.stdout.readline())
            self.assertEqual(startup["event"], "listening")
            self.assertEqual(startup["host"], "127.0.0.1")
            self.assertGreater(startup["port"], 0)
            destination = (startup["host"], startup["port"])
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
                client.settimeout(1)
                client.sendto(b"x" * 1024, destination)
                # Exceed a whole tick's work budget to check that timer waits
                # remain valid on Windows and later joins still get processed.
                for _ in range(128):
                    client.sendto(b"x", destination)
                client.sendto(wire(1, struct.pack("<Q", 12345), 17), destination)
                data, source = client.recvfrom(512)
                self.assertEqual(source, destination)
                kind, sequence, body = decode(data)
                self.assertEqual((kind, sequence), (2, 17))
                nonce, identity, token = struct.unpack("<QIQ", body)
                self.assertEqual(nonce, 12345)
                # A waiting peer still receives valid snapshots even though no
                # position has been submitted for another client to render.
                data, _ = client.recvfrom(512)
                kind, _, body = decode(data)
                self.assertEqual(kind, 4)
                self.assertEqual(struct.unpack("<IQH", body), (identity, token, 0))
                client.sendto(wire(3, struct.pack("<IQffff", identity, token, 1, 2, 3, 4), 1), destination)
                deadline = time.monotonic() + 1
                while True:
                    data, _ = client.recvfrom(512)
                    kind, _, body = decode(data)
                    self.assertEqual(kind, 4)
                    recipient_id, recipient_token, count = struct.unpack_from("<IQH", body)
                    self.assertEqual((recipient_id, recipient_token), (identity, token))
                    self.assertIn(count, (0, 1))
                    self.assertEqual(len(body), 14 + count * 20)
                    if count == 1 and struct.unpack_from("<Iffff", body, 14) == (identity, 1, 2, 3, 4):
                        break
                    self.assertLess(time.monotonic(), deadline)
                client.sendto(wire(5, struct.pack("<IQ", identity, token), 2), destination)
            output, errors = process.communicate(timeout=4)
            self.assertEqual(process.returncode, 0, errors)
            events = [json.loads(line) for line in output.splitlines()]
            self.assertIn("join", [event["event"] for event in events])
            self.assertIn("leave", [event["event"] for event in events])
            self.assertEqual(events[-1]["event"], "stopped")
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate(timeout=4)


if __name__ == "__main__":
    unittest.main()
