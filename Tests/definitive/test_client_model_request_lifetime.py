"""Build the production streaming queue and exercise custom registration lifetime."""

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
SCENARIOS = ("cancel", "replace", "replace-loaded", "complete", "destroy", "replace-after-cancel",
             "callback-failure", "apply-failure", "completion-release-failure", "poll-failure",
             "replace-query-failure", "replace-acquire-failure", "replace-release-failure",
             "cancel-release-failure", "teardown-failure")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", help="Build historical queue sources with the same fixture")
    commit = parser.parse_args().source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-client-model-request-lifetime" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/client-model-request-lifetime"
    scratch.mkdir(parents=True, exist_ok=True)
    result = {"success": False, "sourceCommit": commit, "sourceHashes": {}, "scenarios": {}}
    with tempfile.TemporaryDirectory(dir=scratch) as temp:
        work = Path(temp)
        fixture = Path(__file__).with_name("ClientModelRequestLifetimeTests.cpp")
        shutil.copyfile(fixture, work / fixture.name)
        result["sourceHashes"][fixture.name] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        for name in ("CClientModelRequestManager.h", "CClientModelRequestManager.cpp"):
            path = LOGIC / name
            data = subprocess.check_output(["git", "show", f"{commit}:{path.as_posix()}"], cwd=ROOT) if commit else (ROOT / path).read_bytes()
            (work / name).write_bytes(data)
            result["sourceHashes"][path.as_posix()] = hashlib.sha256(data).hexdigest()
        # The test supplies the SDK and client collaborators before including both production files.
        for header in ("StdInc.h", "CClientCommon.h", "CClientEntity.h"):
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
                                            f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc /W4 /WX {fixture.name} /Fe:test.exe\n')
            build = [os.environ["COMSPEC"], "/d", "/c", "build.cmd"]
            executable = work / "test.exe"
        else:
            executable = work / "test"
            build = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", fixture.name, "-o", str(executable)]

        def run(label, command):
            with (output / f"{label}.log").open("wb") as log:
                return subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        result["compile"] = run("compile", build)
        if result["compile"] == 0:
            result["scenarios"] = {case: run(case, [executable, case]) for case in SCENARIOS}
            result["success"] = all(code == 0 for code in result["scenarios"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(receipt)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
