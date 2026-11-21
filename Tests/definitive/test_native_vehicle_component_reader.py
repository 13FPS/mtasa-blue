"""Build and execute the DE vehicle component reader with controlled x64 memory."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from datetime import datetime, timezone
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
SOURCES = ("Tests/definitive/NativeVehicleComponentReaderTests.cpp", "Client/game_de/NativeImageAccess.cpp")


def main():
    run_id = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid4().hex
    output = ROOT / "Build/mta-native-vehicle-component-reader" / run_id
    output.mkdir(parents=True)
    scratch_parent = ROOT / "Build/tmp/native-vehicle-component-reader"
    scratch_parent.mkdir(parents=True, exist_ok=True)
    result = {"success": False}
    try:
        with tempfile.TemporaryDirectory(dir=scratch_parent) as scratch:
            work = Path(scratch)
            env = dict(os.environ, TMP=scratch, TEMP=scratch)
            for option in ("CL", "_CL_"):
                env.pop(option, None)
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            location = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
                                                "installationPath"], text=True).strip()
            developer = Path(location) / "Common7/Tools/VsDevCmd.bat"
            includes = " ".join(f'/I"{ROOT / path}"' for path in ("Shared/sdk", "Client/sdk", "vendor"))
            sources = " ".join(f'"{ROOT / path}"' for path in SOURCES)
            (work / "compile.cmd").write_text(
                f'@echo off\ncall "{developer}" -no_logo -arch=x64 -host_arch=x64\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++17 /O2 /EHsc /W4 /WX /MD /DWIN32 /DNOMINMAX /DWIN32_LEAN_AND_MEAN '
                f'/D_CRT_SECURE_NO_WARNINGS /D_TIMESPEC_DEFINED {includes} {sources} /sourceDependencies . '
                '/Fe:test.exe /link bcrypt.lib\n', encoding="utf-8")
            phases = (("compile", [env["COMSPEC"], "/d", "/c", "compile.cmd"]), ("execute", [str(work / "test.exe")]))
            for phase, command in phases:
                with (output / f"{phase}.log").open("wb") as log:
                    result[phase] = subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT,
                                                   timeout=120).returncode
                if result[phase] != 0:
                    break
            inputs = {ROOT / path for path in SOURCES}
            for dependency_file in work.glob("*.json"):
                dependencies = json.loads(dependency_file.read_text(encoding="utf-8-sig"))["Data"]["Includes"]
                inputs.update(path for name in dependencies if (path := Path(name)).is_relative_to(ROOT))
            result["sourceHashes"] = {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                                      for path in sorted(inputs)}
            result["success"] = result.get("execute") == 0
    finally:
        receipt = output / "result.json"
        receipt.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(receipt)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
