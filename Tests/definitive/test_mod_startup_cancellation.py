"""Compile and run the real Core mod manager against controlled client callbacks."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
INPUTS = {
    "Client/core/CModManager.cpp": "CModManager.cpp",
    "Client/core/CModManager.h": "CModManager.h",
    "Client/sdk/CSingleton.h": "CSingleton.h",
    "Client/sdk/core/CModManagerInterface.h": "core/CModManagerInterface.h",
    "Client/sdk/core/CClientBase.h": "core/CClientBase.h",
    "Tests/definitive/ModStartupCancellationTests.cpp": "ModStartupCancellationTests.cpp",
}


def main():
    output = ROOT / "Build/mta-mod-startup-cancellation" / uuid4().hex
    output.mkdir(parents=True)
    parent = ROOT / "Build/tmp/mod-startup-cancellation"
    parent.mkdir(parents=True, exist_ok=True)
    assert parent.resolve().is_relative_to((ROOT / "Build/tmp").resolve())
    record = {"success": False, "inputs": {}, "commands": []}
    try:
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            scratch = Path(temporary)
            for source, name in INPUTS.items():
                data = (ROOT / source).read_bytes()
                record["inputs"][source] = hashlib.sha256(data).hexdigest()
                target = scratch / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            # The host supplies only the platform services used by this full
            # translation unit. Its lifecycle methods and declarations remain
            # byte-identical to their shipping source throughout compilation.
            for header in ("StdInc.h", "SharedUtil.Misc.h", "CModuleLoader.h", "core/CCoreInterface.h",
                           "core/CExceptionInformation.h", "profiler/SharedUtil.Profiler.h"):
                target = scratch / header
                target.parent.mkdir(parents=True, exist_ok=True)
                target.touch()
            environment = {**os.environ, "TEMP": str(scratch), "TMP": str(scratch)}
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            install = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                               "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                                              env=environment, text=True, timeout=30).strip()
            setup = Path(install) / "Common7/Tools/VsDevCmd.bat"
            # A fixed local batch file keeps paths with spaces out of cmd's
            # nested command-line quoting and keeps caller options out of CL.
            (scratch / "setup.cmd").write_text(f'@call "{setup}" -no_logo -arch=x64 -host_arch=x64 >nul\n@if errorlevel 1 exit /b 1\n@set\n')
            variables = subprocess.check_output([os.environ["COMSPEC"], "/d", "/c", "setup.cmd"], cwd=scratch,
                                                text=True, errors="replace", env=environment, timeout=60)
            for line in variables.splitlines():
                key, _, value = line.partition("=")
                if key.upper() in {"PATH", "INCLUDE", "LIB", "LIBPATH", "VCTOOLSINSTALLDIR"}:
                    environment[key.upper()] = value
            for key in ("CL", "_CL_"):
                environment.pop(key, None)
            compiler = Path(environment["VCTOOLSINSTALLDIR"]) / "bin/Hostx64/x64/cl.exe"
            for name, command in (
                ("compile", [compiler, "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX", "/O2",
                             "/DMTA_WITH_DEFINITIVE_ADAPTER", "/I.", "ModStartupCancellationTests.cpp", "/Fe:mod-cancel.exe"]),
                ("test", [scratch / "mod-cancel.exe"]),
            ):
                run = subprocess.run(command, cwd=scratch, env=environment, capture_output=True, timeout=90)
                (output / (name + ".log")).write_bytes(run.stdout + run.stderr)
                record["commands"].append({"stage": name, "exitCode": run.returncode})
                run.check_returncode()
        record["success"] = True
    finally:
        receipt = output / "test-result.json"
        receipt.write_text(json.dumps(record, indent=2) + "\n")
        print(receipt)


if __name__ == "__main__":
    main()
