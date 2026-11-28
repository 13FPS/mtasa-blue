"""Compile Deathmatch's world sound callback and event cancellation methods against the MTA Lua DLL."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).with_name("ClientWorldSoundTests.cpp")
LOGIC = ROOT / "Client/mods/deathmatch/logic"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=("x64", "x86"), required=True)
    arch = parser.parse_args().arch
    stage = ROOT / "Bin/mods/deathmatch"
    if arch == "x64":
        stage /= "x64"
    output = ROOT / "Build/mta-client-world-sound" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp"
    scratch.mkdir(exist_ok=True)
    record = {"architecture": arch, "success": False, "inputs": {}}

    def pin(path):
        record["inputs"][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()

    try:
        methods = []
        for name, signatures in (("CClientGame.cpp", ("bool CClientGame::StaticWorldSoundHandler(", "bool CClientGame::WorldSoundHandler(")),
                                 ("CEvents.cpp", ("void CEvents::PreEventPulse()", "void CEvents::PostEventPulse()",
                                                  "void CEvents::CancelEvent(", "bool CEvents::WasEventCancelled()"))):
            path = LOGIC / name
            pin(path)
            source = path.read_text(encoding="utf-8-sig")
            for signature in signatures:
                start = source.index(signature)
                methods.append(source[start:source.index("\n}", start) + 2])
        for path in (HOST, Path(__file__).resolve(), ROOT / "Client/sdk/game/CAudioEngine.h", stage / "lua5.1c.dll", stage / "lua5.1c.lib"):
            pin(path)
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                      "-property", "installationPath"], text=True).strip()
        with tempfile.TemporaryDirectory(dir=scratch, prefix="client-world-sound-") as tmp:
            work = Path(tmp)
            (work / "WorldSoundMethods.inl").write_text("\n\n".join(methods))
            shutil.copy2(stage / "lua5.1c.dll", work)
            includes = [work, ROOT / "Client/sdk", ROOT / "Shared/sdk", ROOT / "vendor/lua/src"]
            flags = ["/nologo", "/EHsc", "/std:c++17", "/W4", "/WX", *("/I" + str(p) for p in includes),
                     str(HOST), str(stage / "lua5.1c.lib"), "/Fe:world-sound.exe"]
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch={arch} -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\n{subprocess.list2cmdline(["cl", *flags])}\n')
            env = dict(os.environ, TEMP=tmp, TMP=tmp)
            for key in ("CL", "_CL_", "LINK", "_LINK_"):
                env.pop(key, None)
            for label, command in (("compile", [env["COMSPEC"], "/d", "/c", "build.cmd"]), ("execute", [str(work / "world-sound.exe")])):
                with (output / (label + ".log")).open("wb") as log:
                    result = subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120)
                record[label] = result.returncode
                result.check_returncode()
        record["success"] = True
    finally:
        path = output / "result.json"
        path.write_text(json.dumps(record, indent=2) + "\n")
        print(path)


if __name__ == "__main__":
    main()
