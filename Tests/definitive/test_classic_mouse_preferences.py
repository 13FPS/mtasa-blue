"""Compile the classic controller's preference methods with two isolated input bytes."""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from uuid import uuid4


def main():
    root = Path(__file__).resolve().parents[2]
    source = (root / "Client/game_sa/CControllerConfigManagerSA.cpp").read_text(encoding="utf-8-sig")
    bodies = []
    for name in ("CControllerConfigManagerSA", "SetFlyWithMouse", "SetSteerWithMouse",
                 "SuspendSteerAndFlyWithMouse", "ApplySteerAndFlyWithMouseSettings"):
        method = re.search(rf"^(?:void )?CControllerConfigManagerSA::{name}\([^)]*\)\n\{{.*?^\}}", source, re.M | re.S)
        if not method:
            raise ValueError(f"Missing controller method {name}")
        bodies.append(method.group())
    extracted = "\n\n".join(bodies)
    output = root / "Build/mta-classic-mouse-preferences" / uuid4().hex
    output.mkdir(parents=True)
    parent = root / "Build/tmp/classic-mouse-preferences"
    parent.mkdir(parents=True, exist_ok=True)
    record = {"methodsSha256": hashlib.sha256(extracted.encode()).hexdigest()}
    with tempfile.TemporaryDirectory(dir=parent) as scratch:
        work = Path(scratch)
        (work / "MousePreferences.generated.inl").write_text(extracted, encoding="utf-8")
        fixture = Path(__file__).with_name("ClassicMousePreferencesTests.cpp")
        shutil.copy(fixture, work)
        env = dict(os.environ, TMP=scratch, TEMP=scratch, TMPDIR=scratch)
        env.pop("CL", None)
        env.pop("_CL_", None)
        if os.name == "nt":
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
            dev = Path(vs) / "Common7/Tools/VsDevCmd.bat"
            (work / "build.cmd").write_text(f'@echo off\ncall "{dev}" -no_logo -arch=x86 -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\ncl /nologo /W4 /WX /std:c++17 "{fixture.name}" /Fe:test.exe\n')
            build = [env["COMSPEC"], "/d", "/c", "build.cmd"]
            executable = work / "test.exe"
        else:
            build = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", fixture.name, "-o", "test"]
            executable = work / "test"
        for phase, command in (("compile", build), ("execute", [str(executable)])):
            result = subprocess.run(command, cwd=work, env=env, capture_output=True, timeout=90)
            (output / f"{phase}.log").write_bytes(result.stdout + result.stderr)
            record[phase] = result.returncode
            if result.returncode:
                break
    record["success"] = record.get("execute") == 0
    (output / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print(output / "result.json")
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
