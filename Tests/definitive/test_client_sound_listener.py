"""Run Deathmatch's camera-listener pulse using the x64 or Win32 BASS no-sound device."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
LOGIC = ROOT / "Client/mods/deathmatch/logic"
SDK = ROOT / "Shared/sdk"
HOST = Path(__file__).with_name("ClientSoundListenerTests.cpp")


def pe_architecture(path):
    with path.open("rb") as binary:
        if binary.read(2) != b"MZ":
            raise ValueError(f"Missing MZ header in {path}")
        binary.seek(0x3C)
        binary.seek(struct.unpack("<I", binary.read(4))[0])
        if binary.read(4) != b"PE\0\0":
            raise ValueError(f"Missing PE header in {path}")
        machine = struct.unpack("<H", binary.read(2))[0]
        return {0x014C: "x86", 0x8664: "x64"}.get(machine, f"0x{machine:04x}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=("x64", "x86"), default="x64", help="target architecture (default: x64)")
    parser.add_argument("--bass-dll", type=Path, help="installed BASS DLL to use instead of the staged runtime for this architecture")
    args = parser.parse_args()
    arch = args.arch
    output = ROOT / "Build/mta-client-sound-listener" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp"
    scratch.mkdir(exist_ok=True)
    record = {"success": False, "requestedArchitecture": arch, "architecture": None, "inputs": {}, "methods": {}, "bassDevice": 0}
    dll = (args.bass_dll or ROOT / ("Bin/mods/deathmatch/x64/bass.dll" if arch == "x64" else "Shared/data/MTA San Andreas/MTA/bass.dll")).resolve()
    lib = ROOT / ("vendor/bass/lib/x64/bass.lib" if arch == "x64" else "vendor/bass/lib/bass.lib")

    def read(path):
        data = path.read_bytes()
        key = path.relative_to(ROOT).as_posix() if path.is_relative_to(ROOT) else str(path)
        record["inputs"][key] = hashlib.sha256(data).hexdigest()
        return data

    try:
        record["bassDll"] = {"path": str(dll), "architecture": pe_architecture(dll)}
        record["importLibrary"] = lib.relative_to(ROOT).as_posix()
        if record["bassDll"]["architecture"] != arch:
            raise ValueError(f"BASS DLL machine {record['bassDll']['architecture']} does not match requested {arch}: {dll}")
        methods = []
        for path, signatures in (
            (ROOT / "Shared/mods/deathmatch/logic/Utils.h", ["inline float DistanceBetweenPoints3D(", "inline float ConvertDegreesToRadiansNoWrap("]),
            (LOGIC / "CClientCamera.cpp", ["bool CClientCamera::GetMatrix(", "void CClientCamera::SetFixedTarget("]),
            (LOGIC / "CBassAudio.cpp", ["void CBassAudio::Process3D(", "void CBassAudio::DoPulse("]),
            (LOGIC / "CClientSound.cpp", ["void CClientSound::Process3D("]),
            (LOGIC / "CClientSoundManager.cpp", ["void CClientSoundManager::DoPulse()", "void CClientSoundManager::UpdateDistanceStreaming("]),
        ):
            source = read(path).decode("utf-8-sig").replace("\r\n", "\n")
            if path.name == "CClientCamera.cpp":
                # Include the same degree conversion, roll table and orthonormality helpers used by the fixed camera.
                methods.append(source[source.index("    constexpr float"):source.index("    inline CMatrix& ScratchMatrix")])
            for signature in signatures:
                start = source.index(signature)
                body = source[start:source.index("\n}", start) + 2]
                methods.append(body)
                record["methods"][signature] = hashlib.sha256(body.encode()).hexdigest()
        header = read(LOGIC / "CBassAudio.h").decode("utf-8-sig")
        cutoff = next(line for line in header.splitlines() if line.startswith("#define CUT_OFF "))
        for path in (HOST, Path(__file__).resolve(), dll, lib, ROOT / "vendor/bass/bass.h",
                     SDK / "CMatrix.h", SDK / "CVector.h", SDK / "CVector4D.h", SDK / "CSphere.h", SDK / "SharedUtil.Map.h", SDK / "SharedUtil.Misc.h", SDK / "SString.hpp"):
            read(path)
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                      "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
        record["toolchain"] = {"installation": vs, "host": "x64", "target": arch}
        with tempfile.TemporaryDirectory(dir=scratch, prefix="client-listener-") as tmp:
            work = Path(tmp)
            (work / "ListenerMethods.generated.inl").write_text(cutoff + "\n\n" + "\n\n".join(methods) + "\n", encoding="utf-8")
            shutil.copy2(dll, work / "bass.dll")
            cmd = ["cl", "/nologo", "/EHsc", "/O2", "/MT", "/std:c++17", "/DNOMINMAX", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS",
                   "/I" + str(work), "/I" + str(SDK), "/I" + str(ROOT / "vendor/bass"), str(HOST), str(lib), "/Fe:listener.exe"]
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch={arch} -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\n{subprocess.list2cmdline(cmd)}\n')
            env = dict(os.environ, TEMP=tmp, TMP=tmp)
            for var in ("CL", "_CL_", "LINK", "_LINK_"):
                env.pop(var, None)
            for label, command in (("compile", [env["COMSPEC"], "/d", "/c", "build.cmd"]), ("run", [str(work / "listener.exe")])):
                if label == "run":
                    record["architecture"] = pe_architecture(work / "listener.exe")
                    if record["architecture"] != arch:
                        raise ValueError(f"Listener executable machine {record['architecture']} does not match requested {arch}")
                with (output / (label + ".log")).open("wb") as log:
                    result = subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
                record[label] = result.returncode
                result.check_returncode()
        for path, expected in record["inputs"].items():
            if hashlib.sha256((ROOT / path).read_bytes()).hexdigest() != expected:
                raise RuntimeError(f"Listener test input changed during run: {path}")
        record["success"] = True
    except Exception as error:
        record["error"] = str(error)
        raise
    finally:
        receipt = output / "result.json"
        receipt.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        print(receipt)


if __name__ == "__main__":
    main()
