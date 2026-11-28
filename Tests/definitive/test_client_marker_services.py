"""Run the Deathmatch checkpoint and marker visibility code with controlled game services."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
LOGIC = ROOT / "Client/mods/deathmatch/logic"
TEST = Path(__file__).with_name("ClientMarkerServiceTests.cpp")


def method(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=("x86", "x64"), default="x64")
    arch = parser.parse_args().arch
    out = ROOT / "Build/mta-client-marker-services" / uuid4().hex
    out.mkdir(parents=True)
    record = {"architecture": arch, "success": False, "inputs": {}}

    def source(path):
        raw = path.read_bytes()
        record["inputs"][path.relative_to(ROOT).as_posix()] = hashlib.sha256(raw).hexdigest()
        return raw.decode("utf-8-sig").replace("\r\n", "\n")

    try:
        scratch = ROOT / "Build/tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="marker-services-", dir=scratch) as temp:
            work = Path(temp)
            for filename in ("CClientCheckpoint.cpp", "CClientCheckpoint.h", "CClientMarkerCommon.cpp", "CClientMarkerCommon.h"):
                source(LOGIC / filename)
                shutil.copy2(LOGIC / filename, work)
            for boundary in ("StdInc.h", "CClientEntity.h"):
                (work / boundary).write_text("// Supplied by the test host.\n")

            sdk = source(ROOT / "Client/sdk/game/CCheckpoint.h")
            signatures = re.findall(r"^\s*virtual\s+([^;]+?)\s*= 0;", sdk, re.M)
            excluded = {"SetRotateRate", "Remove", "SetTargetArrowData"}
            (work / "UnusedCheckpoint.inl").write_text("\n".join(
                sig.strip() + " override { std::abort(); }" for sig in signatures if re.search(r"(\w+)\(", sig)[1] not in excluded))
            utils = source(ROOT / "Shared/mods/deathmatch/logic/Utils.h")
            (work / "MarkerDistances.inl").write_text("\n".join(method(utils, f"inline bool IsPointNearPoint{dimension}D(") for dimension in (2, 3)))
            marker = source(LOGIC / "CClientMarker.cpp")
            definitions = source(LOGIC / "CStaticFunctionDefinitions.cpp")
            rpc = source(LOGIC / "rpc/CMarkerRPCs.cpp")
            methods = [method(marker, "bool CClientMarker::IsOnScreen() const"), method(definitions, "bool CStaticFunctionDefinitions::GetMarkerTarget("),
                       method(definitions, "bool CStaticFunctionDefinitions::SetMarkerTarget("), method(rpc, "void CMarkerRPCs::SetMarkerTarget(")]
            (work / "MarkerServices.inl").write_text("\n\n".join(methods))
            packet = source(LOGIC / "CPacketHandler.cpp").split("case CClientGame::MARKER:", 1)[1].split("case CClientGame::BLIP:", 1)[0]
            target_case = re.search(r"(bool\s+bTarget = bitStream.ReadBit\(\);.*?^([ ]*)if \(bTarget\).*?^\2\})", packet, re.M | re.S)
            if not target_case:
                raise ValueError("Cannot find the entity-add checkpoint target block")
            (work / "InitialMarkerTarget.inl").write_text(target_case[1])
            for path in (Path(__file__).resolve(), TEST, ROOT / "Client/sdk/game/CCheckpoints.h", ROOT / "Client/sdk/game/C3DMarker.h",
                         ROOT / "Client/sdk/game/C3DMarkers.h", ROOT / "Shared/sdk/CVector.h", ROOT / "Shared/sdk/CMatrix.h"):
                source(path)

            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                          "-property", "installationPath"], text=True).strip()
            argv = ["cl", "/nologo", "/EHsc", "/std:c++17", "/O2", "/Gy", "/DNOMINMAX", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS", "/I" + temp,
                    "/I" + str(ROOT / "Shared/sdk"), "/I" + str(ROOT / "Client/sdk"), str(TEST), "/Fe:marker-test.exe", "/link", "/OPT:REF"]
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch={arch} -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\n{subprocess.list2cmdline(argv)}\n')
            env = dict(os.environ, TEMP=temp, TMP=temp)
            for key in ("CL", "_CL_", "LINK", "_LINK_"):
                env.pop(key, None)
            for phase, cmd in (("compile", [env["COMSPEC"], "/d", "/c", "build.cmd"]), ("run", [work / "marker-test.exe"])):
                with (out / (phase + ".log")).open("wb") as log:
                    result = subprocess.run(cmd, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120)
                record[phase] = result.returncode
                result.check_returncode()
            record["success"] = True
    finally:
        path = out / "result.json"
        path.write_text(json.dumps(record, indent=2) + "\n")
        print(path)


if __name__ == "__main__":
    main()
