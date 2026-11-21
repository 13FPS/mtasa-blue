"""Compile the actual upgrade code and single-vehicle scripting wrappers with controlled SDK replies."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
LOGIC = Path("Client/mods/deathmatch/logic")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", help="Test historical production sources with the current fixture")
    commit = parser.parse_args().source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal commit ID")

    run_id = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid4().hex
    output = ROOT / "Build/mta-client-vehicle-upgrades" / run_id
    output.mkdir(parents=True)
    scratch_parent = ROOT / "Build/tmp/vehicle-upgrade-client"
    scratch_parent.mkdir(parents=True, exist_ok=True)
    inputs = {}

    def source(path):
        # A baseline compiles in its own directory with its matching production
        # header, so a changed return signature cannot mask a runtime regression.
        if commit and str(path).startswith("Client"):
            data = subprocess.check_output(["git", "show", f"{commit}:{path.as_posix()}"], cwd=ROOT)
        else:
            data = (ROOT / path).read_bytes()
        inputs[path.as_posix()] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    record = {"success": False, "sourceCommit": commit, "sourceHashes": inputs}
    try:
        with tempfile.TemporaryDirectory(dir=scratch_parent) as scratch:
            work = Path(scratch)
            for name in ("CVehicleUpgrades.cpp", "CVehicleUpgrades.h"):
                (work / name).write_text(source(LOGIC / name), encoding="utf-8")
            cpp = "VehicleUpgradeClientTests.cpp"
            (work / cpp).write_text(source(Path("Tests/definitive") / cpp), encoding="utf-8")
            # The client PCH is supplied by the doubles in the single test TU.
            (work / "StdInc.h").write_text("", encoding="utf-8")
            sdk = source(Path("Client/sdk/game/CModelInfo.h"))
            start = sdk.index("struct SVehicleSupportedUpgrades\n{")
            (work / "VehicleUpgradeCache.generated.h").write_text(sdk[start:sdk.index("\n};", start) + 3], encoding="utf-8")
            wrappers = source(LOGIC / "CStaticFunctionDefinitions.cpp")
            selected = []
            for name in ("AddVehicleUpgrade", "RemoveVehicleUpgrade"):
                start = wrappers.index(f"bool CStaticFunctionDefinitions::{name}(")
                selected.append(wrappers[start:wrappers.index("\n}", start) + 2])
            (work / "VehicleUpgradeWrappers.generated.inl").write_text("\n".join(selected), encoding="utf-8")

            env = dict(os.environ, TMP=scratch, TEMP=scratch, TMPDIR=scratch)
            for option in ("CL", "_CL_"):
                env.pop(option, None)
            program = work / ("test.exe" if os.name == "nt" else "test")
            if os.name == "nt":
                vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
                location = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
                                                    "installationPath"], text=True).strip()
                developer = Path(location) / "Common7/Tools/VsDevCmd.bat"
                batch = work / "compile.cmd"
                batch.write_text(f'@echo off\ncall "{developer}" -no_logo -arch=x64 -host_arch=x64\n'
                                 f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc /W4 /WX /I. {cpp} /Fe:test.exe\n',
                                 encoding="utf-8")
                command = [os.environ["COMSPEC"], "/d", "/c", "compile.cmd"]
            else:
                command = [shutil.which("c++") or "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I.", cpp, "-o", str(program)]
            for phase, args in (("compile", command), ("execute", [str(program)])):
                with (output / f"{phase}.log").open("wb") as log:
                    code = subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode
                record[phase] = code
                if code:
                    break
            record["success"] = record.get("execute") == 0
    finally:
        receipt = output / "result.json"
        receipt.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        print(receipt)
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
