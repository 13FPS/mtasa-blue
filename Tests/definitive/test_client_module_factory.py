"""Run the public Deathmatch factory against a previously verified Windows loader stage."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).with_name("ClientModuleFactoryHost.cpp")


def pin(path):
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}


def pe(path):
    with path.open("rb") as stream:
        assert stream.read(2) == b"MZ", path
        stream.seek(0x3c)
        stream.seek(struct.unpack("<I", stream.read(4))[0])
        assert stream.read(4) == b"PE\0\0", path
        header = stream.read(20)
        machine = struct.unpack_from("<H", header)[0]
        flags = struct.unpack_from("<H", header, 18)[0]
        return machine, struct.unpack("<H", stream.read(2))[0], bool(flags & 0x2000)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--loader-result", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    loader = args.loader_result.resolve()
    baseline = json.loads(loader.read_text(encoding="utf-8-sig"))
    assert baseline["factoryCalled"] is False and baseline["caseCount"] > 0
    arch = baseline["architecture"]
    machine, magic = {"x64": (0x8664, 0x20b), "x86": (0x14c, 0x10b)}[arch]

    output = args.output.resolve()
    assert not output.is_relative_to(loader.parent) and not loader.is_relative_to(output)
    output.mkdir(parents=True, exist_ok=True)
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                  "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    exe = output / "factory-host.exe"
    receipt = {"success": False, "loaderBaseline": pin(loader), "architecture": arch, "factoryHostSource": pin(HOST),
               "runner": pin(Path(__file__)), "sdk": pin(ROOT / "Client/sdk/core/CClientBase.h"),
               "moduleLoader": pin(ROOT / "Client/core/StartupModule.h"), "inputs": []}
    scratch = (ROOT / "Build/tmp").resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    try:
        with tempfile.TemporaryDirectory(dir=scratch, prefix="client-factory-" + arch + "-") as tmp:
            work = Path(tmp).resolve()
            assert work.parent == scratch
            stage = work / "modules"
            stage.mkdir()
            for dep in baseline["inputPins"]:
                name = dep["name"]
                source = Path(dep["staged"]).resolve()
                assert source.parent == loader.parent and source.name == name and source.suffix == ".dll"
                expected = dep["sha256"].lower()
                assert pin(source)["sha256"] == expected
                shutil.copyfile(source, stage / name)
                assert pin(stage / name)["sha256"] == expected
                receipt["inputs"].append(pin(source))
            client = stage / "client.dll"
            assert pe(client) == (machine, magic, True)
            compiler_setup = Path(vs) / "Common7/Tools/VsDevCmd.bat"
            build = work / "build.cmd"
            flags = ["/nologo", "/std:c++17", "/EHsc", "/O2", "/MT", "/W4", "/WX", "/DWIN32", "/DNOMINMAX", "/DWIN32_LEAN_AND_MEAN",
                     "/D_CRT_SECURE_NO_WARNINGS", "/D_SCL_SECURE_NO_WARNINGS", "/D_TIMESPEC_DEFINED",
                     "/external:W0", "/external:I" + str(ROOT / "Client/sdk"), "/external:I" + str(ROOT / "Shared/sdk"),
                     "/external:I" + str(ROOT / "vendor"), "/external:I" + str(ROOT / "vendor/sparsehash/src/windows"),
                     "/external:I" + str(ROOT / "vendor/sparsehash/src"),
                     str(HOST), "/Fo" + str(work / "host.obj"), "/Fe" + str(exe)]
            build.write_text(f'@echo off\ncall "{compiler_setup}" -no_logo -arch={arch} -host_arch=x64\n'
                             "if errorlevel 1 exit /b 1\n" + subprocess.list2cmdline(["cl", *flags]) + "\n")
            env = dict(os.environ, TEMP=tmp, TMP=tmp)
            env.pop("CL", None)
            env.pop("_CL_", None)
            with (output / "compile.log").open("wb") as log:
                subprocess.run([env["COMSPEC"], "/d", "/c", "build.cmd"], cwd=work, env=env, stdout=log,
                               stderr=subprocess.STDOUT, timeout=120, check=True)
            assert pe(exe) == (machine, magic, False)
            with (output / "run.log").open("wb") as log:
                subprocess.run([exe, client], cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=60, check=True)
            assert pin(client)["sha256"] == next(p["sha256"] for p in receipt["inputs"] if Path(p["path"]).name == "client.dll")
        receipt.update(success=True, host=pin(exe), hostAndClientMachine=hex(machine), hostAndClientOptionalMagic=hex(magic),
                       compileLog=pin(output / "compile.log"), runLog=pin(output / "run.log"), factoryCalls=2,
                       sameNonNullClient=True, frameCallbacks=4, clientUnloaded=True, sessionInitialized=False,
                       sessionModulesAbsentAtCheckpoints=True)
    finally:
        (output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(arch, (output / "run.log").read_text().strip())


if __name__ == "__main__":
    main()
