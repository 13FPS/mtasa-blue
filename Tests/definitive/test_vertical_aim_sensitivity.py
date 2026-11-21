"""Exercise classic/DE controller SDK slider conversion on Windows x86 and x64."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]


def main():
    output = ROOT / "Build/mta-vertical-aim-sensitivity" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/vertical-aim-sensitivity"
    scratch.mkdir(parents=True, exist_ok=True)
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    location = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
                                        "installationPath"], text=True).strip()
    developer = Path(location) / "Common7/Tools/VsDevCmd.bat"
    fixture = Path(__file__).with_name("VerticalAimSensitivityTests.cpp")
    sources = (fixture, ROOT / "Client/sdk/game/CControllerConfigManager.h", ROOT / "Shared/sdk/SharedUtil.Misc.h")
    result = {"sourceHashes": {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
              "architectures": {}}
    for arch in ("x86", "x64"):
        with tempfile.TemporaryDirectory(dir=scratch) as temp:
            work = Path(temp)
            env = dict(os.environ, TMP=temp, TEMP=temp, TMPDIR=temp)
            for option in ("CL", "_CL_"):
                env.pop(option, None)
            (work / "build.cmd").write_text(
                f'@echo off\ncall "{developer}" -no_logo -arch={arch} -host_arch=x64\nif errorlevel 1 exit /b 1\n'
                'cl /nologo /EHsc /std:c++17 /O2 /W4 /WX /DWIN32 /DWIN32_LEAN_AND_MEAN /DNOMINMAX '
                f'/I"{ROOT / "Shared/sdk"}" /I"{ROOT / "Client/sdk"}" "{fixture}" /Fe:test.exe\n')
            code = {}
            for phase, args in (("compile", [os.environ["COMSPEC"], "/d", "/c", "build.cmd"]), ("execute", [str(work / "test.exe")])):
                with (output / f"{arch}-{phase}.log").open("wb") as log:
                    code[phase] = subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90).returncode
                if code[phase]:
                    break
            result["architectures"][arch] = code
    result["success"] = all(code.get("execute") == 0 for code in result["architectures"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(receipt)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
