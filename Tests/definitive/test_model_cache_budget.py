"""Compile Core's model cache limit methods and sample interpolation against a recording backend."""
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
CASES = ("samples", "endpoints", "availability", "overrides", "large-overrides")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit")
    args = parser.parse_args()
    if args.source_commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", args.source_commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-model-cache-budget" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/model-cache-budget"
    scratch.mkdir(parents=True, exist_ok=True)
    result = {"sourceCommit": args.source_commit, "inputs": {}, "cases": {}}

    def read(path):
        data = (subprocess.check_output(["git", "show", f"{args.source_commit}:{path}"], cwd=ROOT)
                if args.source_commit else (ROOT / path).read_bytes())
        result["inputs"][path] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    with tempfile.TemporaryDirectory(dir=scratch) as tmp:
        work = Path(tmp)
        source = read("Client/core/CModelCacheManager.cpp")
        methods = []
        for name in ("SetCustomLimits", "DoPulse"):
            start = source.index(f"void CModelCacheManagerImpl::{name}(")
            methods.append(source[start:source.index("\n}", start) + 2])
        implementation = "\n\n".join(methods)
        legacy = "*(int*)0x08A5A80"
        result["historicalReadSubstituted"] = bool(args.source_commit and legacy in implementation)
        if result["historicalReadSubstituted"]:
            implementation = implementation.replace(legacy, "legacyBudgetBytes")
        elif legacy in implementation:
            raise RuntimeError("Current Core still directly reads the classic streaming budget")
        (work / "CacheBudget.generated.inl").write_text(implementation, encoding="utf-8")
        shared = read("Shared/sdk/SharedUtil.Misc.h")
        start = shared.index("    // Clamps a value between two other values")
        end = shared.index("    // Container helpers", start)
        (work / "Interpolation.generated.inl").write_text(shared[start:end], encoding="utf-8")
        fixture = Path(__file__).with_name("ModelCacheBudgetTests.cpp")
        result["inputs"][fixture.relative_to(ROOT).as_posix()] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        shutil.copyfile(fixture, work / fixture.name)
        env = dict(os.environ, TMP=tmp, TEMP=tmp, TMPDIR=tmp)
        if os.name == "nt":
            for option in ("CL", "_CL_"):
                env.pop(option, None)
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                          "-property", "installationPath"], text=True).strip()
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc {fixture.name} /Fe:test.exe\n')
            build, exe = [env["COMSPEC"], "/d", "/c", "build.cmd"], work / "test.exe"
        else:
            build, exe = ["c++", "-std=c++17", fixture.name, "-o", "test"], work / "test"

        def run(label, args):
            with (output / f"{label}.log").open("wb") as log:
                return subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        result["compile"] = run("build", build)
        if result["compile"] == 0:
            result["cases"] = {case: run(case, [exe, case]) for case in CASES}
    result["success"] = result["compile"] == 0 and not any(result["cases"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(receipt)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
