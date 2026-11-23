"""Compile the ordinary Core first-pulse sequence and exercise it with controlled callbacks."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit")
    commit = parser.parse_args().source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-core-first-gameplay-frame" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/core-first-gameplay-frame"
    scratch.mkdir(parents=True, exist_ok=True)
    record = {"sourceCommit": commit, "inputs": {}, "cases": {}, "success": False}

    def read(name):
        data = subprocess.check_output(["git", "show", f"{commit}:{name}"], cwd=ROOT) if commit else (ROOT / name).read_bytes()
        record["inputs"][name] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    source = read("Client/core/CCore.cpp")
    header = read("Client/core/CCore.h")
    start = source.index("void CCore::DoPostFramePulse()")
    end = source.index("// This is the first frame in the menu?", start)
    state = re.search(r"enum class FirstFrameStep\s*\{[^}]*\};\s*FirstFrameStep\s+m_firstFrameStep[^;]*;", header)
    with tempfile.TemporaryDirectory(dir=scratch) as temp:
        work = Path(temp)
        fixture = Path(__file__).with_name("CoreFirstGameplayFrameTests.cpp")
        shutil.copyfile(fixture, work / fixture.name)
        record["inputs"][fixture.relative_to(ROOT).as_posix()] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        (work / "FirstFrameState.generated.inl").write_text(state.group() if state else "")
        # Replace subsequent game-frame work with an observable boundary after the actual startup code.
        (work / "FirstGameplayFrame.generated.inl").write_text(source[start:end] + 'host.Call("frame");\n}\n')
        env = dict(os.environ, TMP=temp, TEMP=temp, TMPDIR=temp)
        env.pop("CL", None)
        env.pop("_CL_", None)
        if os.name == "nt":
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\ncl /nologo /EHsc /std:c++17 /W4 /WX {fixture.name} /Fe:test.exe\n')
            command, exe = [env["COMSPEC"], "/d", "/c", "build.cmd"], work / "test.exe"
        else:
            command, exe = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", fixture.name, "-o", "test"], work / "test"

        def run(label, args):
            with (output / f"{label}.log").open("wb") as log:
                return subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        record["compile"] = run("build", command)
        if record["compile"] == 0:
            scenarios = ("instances", "reentrant", "steam-disabled", "validate", "console", "game", "steam", "input")
            record["cases"] = {name: run(name, [exe, name]) for name in scenarios}
            record["success"] = not any(record["cases"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(receipt)
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
