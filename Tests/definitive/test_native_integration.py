"""Real x64 transport tests with synthetic transforms, never an installed GTA game."""

import ctypes
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import struct
import time
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]
NATIVE_DIRECTORY = Path(os.environ.get("DE_NATIVE_DIRECTORY", REPOSITORY / "Build/definitive-online"))
SERVER = REPOSITORY / "Server/definitive/server.py"


class Player(ctypes.Structure):
    _fields_ = [("id", ctypes.c_uint32), ("x", ctypes.c_float), ("y", ctypes.c_float),
                ("z", ctypes.c_float), ("heading", ctypes.c_float)]


class NativeIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name != "nt" or not (NATIVE_DIRECTORY / "de_client_bridge.dll").is_file():
            raise unittest.SkipTest("Build the standalone Windows x64 components first")
        cls.bridge = ctypes.CDLL(str(NATIVE_DIRECTORY / "de_client_bridge.dll"))
        cls.bridge.DE_ApiVersion.restype = ctypes.c_uint32
        cls.bridge.DE_Connect.argtypes = [ctypes.c_char_p, ctypes.c_uint16, ctypes.c_uint64]
        cls.bridge.DE_Connect.restype = ctypes.c_int
        cls.bridge.DE_Disconnect.argtypes = []
        cls.bridge.DE_Disconnect.restype = None
        cls.bridge.DE_Pump.restype = ctypes.c_int
        cls.bridge.DE_SubmitTransform.argtypes = [ctypes.c_float] * 4
        cls.bridge.DE_SubmitTransform.restype = ctypes.c_int
        cls.bridge.DE_LocalPlayerId.restype = ctypes.c_uint32
        cls.bridge.DE_ReadPlayers.argtypes = [ctypes.POINTER(Player), ctypes.c_uint32]
        cls.bridge.DE_ReadPlayers.restype = ctypes.c_uint32
        cls.bridge.DE_SubmitTransformBits.argtypes = [ctypes.c_uint32] * 4
        cls.bridge.DE_SubmitTransformBits.restype = ctypes.c_int
        cls.bridge.DE_PlayerCount.restype = ctypes.c_uint32
        cls.bridge.DE_ReadPlayerComponent.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
        cls.bridge.DE_ReadPlayerComponent.restype = ctypes.c_uint32
        cls.bridge.DE_CheckExecutableBuild.argtypes = [ctypes.c_wchar_p]
        cls.bridge.DE_CheckExecutableBuild.restype = ctypes.c_int
        cls.bridge.DE_CheckGameBuild.restype = ctypes.c_int
        cls.bridge.DE_BindRuntime.restype = ctypes.c_int
        cls.bridge.DE_ConnectLocal.argtypes = [ctypes.c_uint32]
        cls.bridge.DE_ConnectLocal.restype = ctypes.c_int

    def setUp(self):
        self.server = subprocess.Popen([sys.executable, str(SERVER), "--host", "127.0.0.1", "--port", "0"],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(self.stop_server)
        startup = queue.Queue()
        threading.Thread(target=lambda: startup.put(self.server.stdout.readline()), daemon=True).start()
        try:
            line = startup.get(timeout=5)
        except queue.Empty:
            self.fail("Server did not report startup within five seconds")
        self.assertTrue(line, "Server exited before reporting its listening address")
        message = json.loads(line)
        self.assertEqual(message["event"], "listening")
        self.port = message["port"]
        self.addCleanup(self.bridge.DE_Disconnect)

    def stop_server(self):
        if self.server.poll() is None:
            self.server.terminate()
        self.server.communicate(timeout=5)

    def probe(self, seed, seconds=2):
        return subprocess.Popen([str(NATIVE_DIRECTORY / "de_client_probe.exe"), "--port", str(self.port),
                                 "--seed", str(seed), "--seconds", str(seconds), "--require-peer"],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def collect_probe(self, process):
        try:
            output, errors = process.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()
            self.fail("Native probe did not complete")
        self.assertEqual(process.returncode, 0, errors + output)
        result = json.loads(output)
        self.assertTrue(result["joined"])
        self.assertTrue(result["remote_motion_observed"])
        self.assertEqual(result["max_remote_players"], 1)
        self.assertFalse(result["game_adapter_active"])
        return result

    def test_two_native_clients_receive_each_others_motion(self):
        first, second = self.probe(1), self.probe(2)
        try:
            first_result = self.collect_probe(first)
            second_result = self.collect_probe(second)
            self.assertNotEqual(first_result["player_id"], second_result["player_id"])
        finally:
            for process in (first, second):
                if process.poll() is None:
                    process.kill()
                    process.communicate()

    def test_bridge_abi_receives_remote_motion_and_departure(self):
        self.assertEqual(self.bridge.DE_ApiVersion(), 2)
        self.assertEqual(self.bridge.DE_Connect(b"127.0.0.1", self.port, 0), 0)
        self.assertEqual(self.bridge.DE_Connect(b"127.0.0.1", self.port, 12345), 1)
        peer = self.probe(2, seconds=2)
        try:
            states = (Player * 16)()
            observed_positions = set()
            saw_departure = False
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                self.assertEqual(self.bridge.DE_Pump(), 1)
                own_id = self.bridge.DE_LocalPlayerId()
                if own_id:
                    self.assertEqual(self.bridge.DE_SubmitTransform(10 + time.monotonic() % 5, 1, 3, 0), 1)
                    count = self.bridge.DE_ReadPlayers(states, 16)
                    self.assertLessEqual(count, 16)
                    self.assertEqual(self.bridge.DE_PlayerCount(), count)
                    for index in range(count):
                        self.assertEqual(self.bridge.DE_ReadPlayerComponent(index, 0), states[index].id)
                        for component, name in enumerate(("x", "y", "z", "heading"), start=1):
                            bits = self.bridge.DE_ReadPlayerComponent(index, component)
                            value = struct.unpack("<f", struct.pack("<I", bits))[0]
                            self.assertEqual(value, getattr(states[index], name))
                    bits = [struct.unpack("<I", struct.pack("<f", value))[0] for value in (-10.25, 1.5, 3.125, 270)]
                    self.assertEqual(self.bridge.DE_SubmitTransformBits(*bits), 1)
                    remote = [states[index] for index in range(count) if states[index].id != own_id]
                    for player in remote:
                        if player.x >= 20:
                            observed_positions.add(round(player.x, 2))
                    if len(observed_positions) >= 3 and peer.poll() is not None and not remote:
                        saw_departure = True
                        break
                time.sleep(0.02)
            self.collect_probe(peer)
            self.assertGreaterEqual(len(observed_positions), 3)
            self.assertTrue(saw_departure, "A departed peer must disappear from subsequent snapshots")
            self.assertEqual(self.bridge.DE_SubmitTransform(float("nan"), 0, 0, 0), 0)
            self.assertEqual(self.bridge.DE_SubmitTransformBits(0x7fc00000, 0, 0, 0), 0)
            self.assertEqual(self.bridge.DE_ReadPlayerComponent(16, 0), 0)
            self.assertEqual(self.bridge.DE_ReadPlayerComponent(0, 5), 0)
            self.assertEqual(self.bridge.DE_ReadPlayers(None, 1), 0)
            self.bridge.DE_Disconnect()
            self.assertEqual(self.bridge.DE_LocalPlayerId(), 0)
            self.assertEqual(self.bridge.DE_Pump(), 0)
        finally:
            if peer.poll() is None:
                peer.kill()
                peer.communicate()

    def test_game_binding_rejects_non_game_process(self):
        # A transport harness must not be mistaken for the verified game host.
        self.assertEqual(self.bridge.DE_CheckGameBuild(), 0)
        self.assertEqual(self.bridge.DE_CheckExecutableBuild(sys.executable), 0)
        self.assertEqual(self.bridge.DE_CheckExecutableBuild(None), 0)
        self.assertEqual(self.bridge.DE_CheckExecutableBuild("missing-SanAndreas.exe"), 0)
        self.assertEqual(self.bridge.DE_BindRuntime(), 0)
        self.assertEqual(self.bridge.DE_ConnectLocal(1), 0)

    def test_known_installed_build_fingerprint(self):
        game = Path(r"C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64\SanAndreas.exe")
        if not game.exists():
            self.skipTest("Local game fingerprint check requires the inventoried installation")
        self.assertEqual(self.bridge.DE_CheckExecutableBuild(str(game)), 1)


if __name__ == "__main__":
    unittest.main()
