"""Compile Core's resolution list and the classic mode selector against the actual settings SDK."""
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
    parser.add_argument("--source-commit", help="Run the same fixture against a historical client")
    parser.add_argument("--arch", choices=("x64", "x86"), default="x64", help="MSVC target architecture")
    args = parser.parse_args()
    if args.source_commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", args.source_commit):
        parser.error("Expected hexadecimal commit ID")
    output = ROOT / "Build/mta-settings-video-modes" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/settings-video-modes"
    scratch.mkdir(parents=True, exist_ok=True)
    result = {"success": False, "sourceCommit": args.source_commit, "sourceHashes": {}, "scenarios": {}}

    def source(name):
        data = (subprocess.check_output(["git", "show", f"{args.source_commit}:{name}"], cwd=ROOT)
                if args.source_commit else (ROOT / name).read_bytes())
        result["sourceHashes"][name] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    def definition(code, start):
        begin = code.index(start)
        return code[begin:code.index("\n}", begin) + 2]

    with tempfile.TemporaryDirectory(dir=scratch) as tmp:
        work = Path(tmp)
        header = source("Client/sdk/game/CSettings.h")
        classic = source("Client/game_sa/CSettingsSA.cpp")
        core = source("Client/core/CSettings.cpp")
        (work / "CSettings.h").write_text(header)
        methods = [definition(classic, start) for start in ("bool CSettingsSA::HasUnsafeResolutions(",
                   "uint CSettingsSA::FindVideoMode(", "void CSettingsSA::SetAspectRatio(")]
        methods += [definition(core, "struct ResolutionData") + ";", definition(core, "void CSettings::PopulateResolutionComboBox(")]
        (work / "VideoModeMethods.generated.inl").write_text("\n\n".join(methods))
        implemented = {"GetNumVideoModes", "GetVideoModeInfo", "GetCurrentVideoMode", "IsUnsafeResolution", "SetAspectRatio", "HasUnsafeResolutions"}
        # Abort if an unrelated game operation is reached while testing these UI and SDK paths.
        stubs = []
        for signature, name in re.findall(r"virtual\s+([^;{}\n]+?\b(\w+)\([^;{}\n]*?\)(?:\s+const)?(?:\s+noexcept)?)\s*=\s*0;", header):
            if name not in implemented:
                stubs.append(signature + " override { std::abort(); }")
        (work / "OtherSettings.generated.inl").write_text("\n".join(stubs))
        fixture = Path(__file__).with_name("GameSettingsVideoModeTests.cpp")
        shutil.copy(fixture, work)
        result["sourceHashes"][fixture.relative_to(ROOT).as_posix()] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        env = dict(os.environ, TMP=tmp, TEMP=tmp, TMPDIR=tmp)
        for name in ("CL", "_CL_"):
            env.pop(name, None)
        if os.name == "nt":
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
            dev = Path(vs) / "Common7/Tools/VsDevCmd.bat"
            (work / "build.cmd").write_text(f'@echo off\ncall "{dev}" -no_logo -arch={args.arch} -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc "{fixture.name}" /Fe:test.exe\n')
            build = [env["COMSPEC"], "/d", "/c", "build.cmd"]
            exe = work / "test.exe"
        else:
            build = ["c++", "-std=c++17", "-m64" if args.arch == "x64" else "-m32", fixture.name, "-o", "test"]
            exe = work / "test"

        def run(label, command):
            with (output / (label + ".log")).open("wb") as log:
                return subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        result["architecture"] = args.arch
        result["compile"] = run("compile", build)
        if result["compile"] == 0:
            result["scenarios"] = {case: run(case, [exe, case]) for case in ("unsafe", "select", "auto-aspect", "choices")}
            result["success"] = all(code == 0 for code in result["scenarios"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(receipt)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
