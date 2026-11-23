"""Exercise asynchronous object streaming across real sector transitions."""

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
LOGIC = Path("Client/mods/deathmatch/logic")
COMPONENTS = ("CClientStreamer", "CClientStreamSector", "CClientStreamSectorRow",
              "CClientStreamElement", "CClientModelRequestManager")
SCENARIOS = ("camera-sector", "camera-before-streamer", "object-moves", "parent-streams-out", "forced", "hysteresis",
             "explicit-request", "direct-stream-in", "acquire-retry", "repair-broken", "enable-respawn", "break-while-loading")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", help="Use these historical client sources with the same fixture")
    commit = parser.parse_args().source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-client-streamer-pending-model" / uuid4().hex
    output.mkdir(parents=True)
    result = {"sourceCommit": commit, "sourceHashes": {}, "scenarios": {}}
    scratch = ROOT / "Build/tmp/client-streamer-pending-model"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temp:
        work = Path(temp)
        fixture = Path(__file__).with_name("ClientStreamerPendingModelTests.cpp")
        shutil.copyfile(fixture, work / fixture.name)
        result["sourceHashes"][fixture.name] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        for name in [c + ext for c in COMPONENTS for ext in (".h", ".cpp")] + ["CClientObject.cpp"]:
            path = LOGIC / name
            data = (subprocess.check_output(["git", "show", f"{commit}:{path.as_posix()}"], cwd=ROOT)
                    if commit else (ROOT / path).read_bytes())
            result["sourceHashes"][path.as_posix()] = hashlib.sha256(data).hexdigest()
            source = data.decode("utf-8-sig").replace("\r\n", "\n")
            if name == "CClientStreamElement.cpp":
                # The engine-backed bounding box is a point in this fixture.
                source = source[:source.index("CSphere CClientStreamElement::GetWorldBoundingSphere(")]
                name = "CClientStreamElement.generated.inl"
            elif name == "CClientObject.cpp":
                methods = []
                for method in ("StreamIn", "StreamOut", "ModelRequestCallback"):
                    begin = source.index(f"void CClientObject::{method}(")
                    methods.append(source[begin:source.index("\n}", begin) + 2])
                source = "\n\n".join(methods)
                name = "CClientObject.generated.inl"
            elif name == "CClientStreamer.cpp":
                source = source.replace(r'..\deathmatch\logic\CClientGame.h', "CClientGame.h")
            (work / name).write_text(source, encoding="utf-8")
        for header in ("StdInc.h", "CClientCommon.h", "CClientEntity.h", "CClientGame.h"):
            (work / header).touch()

        env = dict(os.environ, TMP=temp, TEMP=temp, TMPDIR=temp)
        for option in ("CL", "_CL_"):
            env.pop(option, None)
        if os.name == "nt":
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            installation = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
                                                    "installationPath"], text=True).strip()
            developer = Path(installation) / "Common7/Tools/VsDevCmd.bat"
            (work / "build.cmd").write_text(f'@echo off\ncall "{developer}" -no_logo -arch=x64 -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc {fixture.name} /Fe:test.exe\n')
            build = [os.environ["COMSPEC"], "/d", "/c", "build.cmd"]
            executable = work / "test.exe"
        else:
            executable = work / "test"
            build = ["c++", "-std=c++17", fixture.name, "-o", str(executable)]

        def run(label, command):
            with (output / f"{label}.log").open("wb") as log:
                return subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        result["compile"] = run("compile", build)
        if result["compile"] == 0:
            result["scenarios"] = {case: run(case, [executable, case]) for case in SCENARIOS}
    result["success"] = result["compile"] == 0 and all(code == 0 for code in result["scenarios"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(receipt)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
