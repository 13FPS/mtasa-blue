"""Compile the Core, Deathmatch and Lua streaming budget paths against recording receivers."""
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
CASES = ("missing", "junk", "suffix", "zero", "high", "valid", "legacy-hex", "override", "size-boundaries", "late-range", "late-limits")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit")
    parser.add_argument("--skip-missing-service", action="store_true", help="Skip the unavailable service case on an older source revision.")
    args = parser.parse_args()
    commit = args.source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-streaming-memory-settings" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/streaming-memory-settings"
    scratch.mkdir(parents=True, exist_ok=True)
    result = {"sourceCommit": commit, "inputs": {}, "cases": {}, "skipped": {}}

    def read(path):
        data = subprocess.check_output(["git", "show", f"{commit}:{path}"], cwd=ROOT) if commit else (ROOT / path).read_bytes()
        result["inputs"][path] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    def method(text, signature):
        start = text.index(signature + "\n{")
        return text[start:text.index("\n}", start) + 2]

    with tempfile.TemporaryDirectory(dir=scratch) as tmp:
        work = Path(tmp)
        fixture = Path(__file__).with_name("StreamingMemorySettingsTests.cpp")
        shutil.copyfile(fixture, work / fixture.name)
        result["inputs"][fixture.relative_to(ROOT).as_posix()] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        core = read("Client/core/CCore.cpp")
        (work / "CCVarsInterface.h").write_text(read("Client/sdk/core/CCVarsInterface.h"), encoding="utf-8")
        if "#include <core/CStreamingMemorySettings.h>" in core:
            (work / "CStreamingMemorySettings.h").write_text(read("Client/sdk/core/CStreamingMemorySettings.h"), encoding="utf-8")
        (work / "Core.generated.inl").write_text("\n".join(method(core, signature) for signature in (
            "void CCore::SetCustomStreamingMemory(size_t sizeBytes)", "bool CCore::IsUsingCustomStreamingMemorySize()", "size_t CCore::GetStreamingMemory()")))
        lua = read("Client/mods/deathmatch/logic/luadefs/CLuaEngineDefs.cpp")
        (work / "Lua.generated.inl").write_text("\n".join(method(lua, signature) for signature in (
            "void EngineStreamingSetMemorySize(size_t sizeBytes)", "void EngineStreamingRestoreMemorySize()", "size_t EngineStreamingGetMemorySize()")))
        xml = read("Shared/XML/CXMLNodeImpl.cpp")
        (work / "Xml.generated.inl").write_text("\n".join(method(xml, signature) for signature in (
            "bool CXMLNodeImpl::StringToLong(const char* szString, long& lValue)", "bool CXMLNodeImpl::GetTagContent(unsigned int& uiContent)")))
        game = read("Client/mods/deathmatch/logic/CClientGame.cpp")
        begin = game.index("*** NETWORK TROUBLE ***")
        pulse = game[game.index("\n        }", begin) + len("\n        }"):game.index("// If we're in debug mode and are supposed to show task data", begin)]
        (work / "Pulse.generated.inl").write_text(pulse)
        if args.skip_missing_service and commit:
            result["skipped"]["late-limits"] = "Missing-service case skipped for this comparison."

        env = dict(os.environ, TEMP=tmp, TMP=tmp, TMPDIR=tmp)
        if os.name == "nt":
            env.pop("CL", None)
            env.pop("_CL_", None)
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                          "-property", "installationPath"], text=True).strip()
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64\n'
                                             f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc /DWIN32 {fixture.name} /Fe:test.exe\n')
            build, exe = [env["COMSPEC"], "/d", "/c", "build.cmd"], work / "test.exe"
        else:
            build, exe = ["c++", "-std=c++17", fixture.name, "-o", "test"], work / "test"

        def run(label, args):
            with (output / f"{label}.log").open("wb") as log:
                return subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        result["compile"] = run("build", build)
        if result["compile"] == 0:
            result["cases"] = {case: run(case, [exe, case]) for case in CASES if case not in result["skipped"]}
    result["success"] = result["compile"] == 0 and not result["skipped"] and not any(result["cases"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(receipt)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
