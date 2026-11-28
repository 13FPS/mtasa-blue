"""Exercise Deathmatch player startup and ped streaming against in-process game and event fakes."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
LOGIC = Path("Client/mods/deathmatch/logic")
CASES = ("init-refusal", "missing-pools", "pool-replaced", "missing-ped", "wrong-ped", "foreign-owner",
         "foreign-slot", "missing-intelligence", "missing-tasks", "model-refusal", "success",
         "early-stream-in", "ped-only-stream-in", "model-only-stream-in", "online", "offline-then-online",
         "server-starting", "reentrant-focus", "reentrant-success", "reentrant-refusal", "repeated-refusal",
         "withdrawn-stream-in", "transferred-during-stream", "lost-handle-during-stream", "foreign-stream-in", "waiting-for-game",
         "cleanup-owned", "cleanup-reassociated", "cleanup-missing-pool", "cleanup-after-vehicle-callback",
         "remote-success", "remote-missing-pool", "remote-missing-ped", "remote-wrong-ped", "remote-missing-intelligence",
         "remote-missing-tasks", "remote-throw-add", "remote-throw-tasks", "remote-model-refusal",
         "remote-recreate-failure", "remote-recreate-model-refusal", "remote-reentrant-recreate",
         "remote-throw-render", "remote-recreate-throw-render", "startup-throw-task", "startup-throw-render",
         "startup-reassigned-task-refusal", "startup-reassigned-task-success")


def method(source, owner, name):
    match = re.search(rf"^(?:void|bool) {owner}::{name}\(", source, re.MULTILINE)
    if not match:
        raise ValueError(f"Missing {owner}::{name}")
    return source[match.start():source.index("\n}", match.start()) + 2]


def between(source, begin, end):
    start = source.index(begin)
    return source[start:source.index(end, start)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", help="Run this fixture against a historical commit")
    commit = parser.parse_args().source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-local-player-startup" / uuid4().hex
    output.mkdir(parents=True)
    runner = Path(__file__)
    receipt = {"sourceCommit": commit, "platform": platform.system(), "machine": platform.machine(),
               "sourceHashes": {runner.name: hashlib.sha256(runner.read_bytes()).hexdigest()}, "scenarios": {}}

    def read_source(name):
        path = LOGIC / name
        data = (subprocess.check_output(["git", "show", f"{commit}:{path.as_posix()}"], cwd=ROOT)
                if commit else (ROOT / path).read_bytes())
        receipt["sourceHashes"][path.as_posix()] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    ped = read_source("CClientPed.cpp")
    ped_manager = read_source("CClientPedManager.cpp")
    stream = read_source("CClientStreamElement.cpp")
    game = read_source("CClientGame.cpp")
    game_header = read_source("CClientGame.h")
    packet = read_source("CPacketHandler.cpp")
    event = method(game, "CClientGame", "Event_OnIngame")
    pulse = method(game, "CClientGame", "DoPulses")
    packet_joined = method(packet, "CPacketHandler", "Packet_ServerJoined")

    # Keep the frame entry guard, first-frame initialization and connected notification.
    # The omitted middle of DoPulses handles unrelated client services and frame rendering.
    pulse = (pulse[:pulse.index("    TIMING_CHECKPOINT(")] +
             between(pulse, "    // If the game is loaded ...", "        // Check if the player is hitting") + "    }\n" +
             between(pulse, "    // Not waiting for local connect?", "        unsigned char ucError") + "    }\n}")
    joined_gate = between(packet_joined, "    // Are we ingame?", "    // Are we JOINING?")
    parts = [method(ped_manager, "CClientPedManager", name) for name in ("OnCreation", "OnDestruction")]
    for helper in ("HasLocalModel", "ReleaseLocalModel"):
        if f"CClientPed::{helper}(" in ped:
            parts.append(method(ped, "CClientPed", helper))
    parts += [method(ped, "CClientPed", name) for name in ("AcquireRequestedModel", "ReleaseLoadedModel", "_CreateLocalModel",
                                                          "_DestroyLocalModel", "StreamIn", "NotifyCreate")]
    parts += [method(stream, "CClientStreamElement", name) for name in ("InternalStreamIn", "InternalStreamOut", "NotifyUnableToCreate")]
    # Property restoration after a valid remote ped has been published is independent
    # of acquiring or rejecting the new ped. Keep the real pool, task, and stream-event calls.
    remote = method(ped, "CClientPed", "_CreateModel")
    restore = between(remote, "        // Jump straight to the target position", "        // Tell the streamer we created the player")
    parts += [remote.replace(restore, ""), method(ped, "CClientPed", "ReCreateGameEntity")]
    parts += [method(ped, "CClientPed", "ModelRequestCallback")]
    destructor_start = between(ped, "CClientPed::~CClientPed()", "    g_pClientGame->RemovePedPointerFromSet(this);")
    parts.append(destructor_start.replace("CClientPed::~CClientPed()", "void CClientPed::BeginDestruction()", 1) + "}")
    parts += [event, method(game, "CClientGame", "Event_OnIngameAndConnected"), pulse,
              "void CPacketHandler::EnterJoined()\n{\n" + joined_gate + "    ++accepted;\n}"]
    scratch = ROOT / "Build/tmp/local-player-startup"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temp:
        work = Path(temp)
        fixture = Path(__file__).with_name("LocalPlayerStartupTests.cpp")
        receipt["sourceHashes"][fixture.name] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        shutil.copyfile(fixture, work / fixture.name)
        (work / "production.inl").write_text("\n\n".join(parts), encoding="utf-8")
        returns = event.split(" ", 1)[0]
        (work / "declaration.inl").write_text(f"{returns} Event_OnIngame();\n", encoding="utf-8")
        if "enum class IngameStartup" in game_header:
            state = between(game_header, "    enum class IngameStartup", "    bool m_bTriggeredIngameAndConnected")
        else:
            state = "bool m_bGameLoaded = false;\n"
        (work / "startup-state.inl").write_text(state, encoding="utf-8")
        env = dict(os.environ, TMP=temp, TEMP=temp, TMPDIR=temp)
        for key in ("CL", "_CL_"):
            env.pop(key, None)
        if os.name == "nt":
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                          "-property", "installationPath"], text=True).strip()
            developer = Path(vs) / "Common7/Tools/VsDevCmd.bat"
            (work / "build.cmd").write_text(f'@echo off\ncall "{developer}" -no_logo -arch=x64 -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc {fixture.name} /Fe:test.exe\n')
            build = [os.environ["COMSPEC"], "/d", "/c", "build.cmd"]
            executable = work / "test.exe"
        else:
            executable = work / "test"
            build = ["c++", "-std=c++17", fixture.name, "-o", str(executable)]

        def run(name, command):
            with (output / f"{name}.log").open("wb") as log:
                return subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        receipt["compile"] = run("compile", build)
        if receipt["compile"] == 0:
            for case in CASES:
                receipt["scenarios"][case] = run(case, [executable, case])
    receipt["success"] = receipt["compile"] == 0 and all(code == 0 for code in receipt["scenarios"].values())
    path = output / "result.json"
    path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(path)
    return 0 if receipt["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
