"""Run Deathmatch's camera-listener pulse using the staged Windows x64 BASS no-sound device."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
LOGIC = ROOT / "Client/mods/deathmatch/logic"
SDK = ROOT / "Shared/sdk"
HOST = Path(__file__).with_name("ClientSoundListenerTests.cpp")


def main():
    output = ROOT / "Build/mta-client-sound-listener" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp"
    scratch.mkdir(exist_ok=True)
    record = {"success": False, "architecture": "x64", "inputs": {}, "methods": {}, "bassDevice": 0}
    dll = ROOT / "Bin/mods/deathmatch/x64/bass.dll"
    lib = ROOT / "vendor/bass/lib/x64/bass.lib"

    def read(path):
        data = path.read_bytes()
        record["inputs"][path.relative_to(ROOT).as_posix()] = hashlib.sha256(data).hexdigest()
        return data

    try:
        methods = []
        for path, signatures in (
            (ROOT / "Shared/mods/deathmatch/logic/Utils.h", ["inline float DistanceBetweenPoints3D("]),
            (LOGIC / "CBassAudio.cpp", ["void CBassAudio::Process3D(", "void CBassAudio::DoPulse("]),
            (LOGIC / "CClientSound.cpp", ["void CClientSound::Process3D("]),
            (LOGIC / "CClientSoundManager.cpp", ["void CClientSoundManager::DoPulse()", "void CClientSoundManager::UpdateDistanceStreaming("]),
        ):
            source = read(path).decode("utf-8-sig").replace("\r\n", "\n")
            for signature in signatures:
                start = source.index(signature)
                body = source[start:source.index("\n}", start) + 2]
                methods.append(body)
                record["methods"][signature] = hashlib.sha256(body.encode()).hexdigest()
        header = read(LOGIC / "CBassAudio.h").decode("utf-8-sig")
        cutoff = next(line for line in header.splitlines() if line.startswith("#define CUT_OFF "))
        for path in (HOST, Path(__file__).resolve(), dll, lib, ROOT / "vendor/bass/bass.h",
                     SDK / "CVector.h", SDK / "CVector4D.h", SDK / "CSphere.h", SDK / "SharedUtil.Map.h", SDK / "SharedUtil.Misc.h", SDK / "SString.hpp"):
            read(path)
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                      "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
        with tempfile.TemporaryDirectory(dir=scratch, prefix="client-listener-") as tmp:
            work = Path(tmp)
            (work / "ListenerMethods.generated.inl").write_text(cutoff + "\n\n" + "\n\n".join(methods) + "\n", encoding="utf-8")
            shutil.copy2(dll, work)
            cmd = ["cl", "/nologo", "/EHsc", "/O2", "/MT", "/std:c++17", "/DNOMINMAX", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS",
                   "/I" + str(work), "/I" + str(SDK), "/I" + str(ROOT / "vendor/bass"), str(HOST), str(lib), "/Fe:listener.exe"]
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\n{subprocess.list2cmdline(cmd)}\n')
            env = dict(os.environ, TEMP=tmp, TMP=tmp)
            for var in ("CL", "_CL_", "LINK", "_LINK_"):
                env.pop(var, None)
            for label, command in (("compile", [env["COMSPEC"], "/d", "/c", "build.cmd"]), ("run", [str(work / "listener.exe")])):
                with (output / (label + ".log")).open("wb") as log:
                    result = subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
                record[label] = result.returncode
                result.check_returncode()
        for path, expected in record["inputs"].items():
            if hashlib.sha256((ROOT / path).read_bytes()).hexdigest() != expected:
                raise RuntimeError(f"Listener test input changed during run: {path}")
        record["success"] = True
    finally:
        receipt = output / "result.json"
        receipt.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        print(receipt)


if __name__ == "__main__":
    main()
