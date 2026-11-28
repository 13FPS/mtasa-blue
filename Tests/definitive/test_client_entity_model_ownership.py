"""Compile and execute actual object, vehicle and ped model transitions with controlled native objects."""
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
METHODS = {
    "CClientObject": ["SetModel", "StreamIn", "ModelRequestCallback"],
    "CClientVehicle": ["SetModelBlocking", "StreamIn", "ModelRequestCallback"],
    "CClientPed": ["SetModel", "RestoreTemporaryModel", "AcquireRequestedModel", "RetainRequestedModel", "ReleaseModelReference", "ResolveUnconfirmedSkin", "ReleaseLoadedModel",
                   "_ChangeModel", "ModelRequestCallback", "ReCreateGameEntity"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", help="Run the same fixture against historical setters")
    commit = parser.parse_args().source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal Git commit")
    result = {"success": False, "sourceCommit": commit, "inputSha256": {}}
    output = ROOT / "Build/mta-client-entity-model-ownership" / uuid4().hex
    output.mkdir(parents=True)
    parent = ROOT / "Build/tmp/client-entity-model-ownership"
    parent.mkdir(parents=True, exist_ok=True)
    try:
        with tempfile.TemporaryDirectory(dir=parent) as temp:
            w = Path(temp)
            pieces = []
            for cls, methods in METHODS.items():
                path = LOGIC / (cls + ".cpp")
                raw = subprocess.check_output(["git", "show", f"{commit}:{path.as_posix()}"], cwd=ROOT) if commit else (ROOT / path).read_bytes()
                result["inputSha256"][path.as_posix()] = hashlib.sha256(raw).hexdigest()
                source = raw.decode("utf-8-sig").replace("\r\n", "\n")
                for method in methods:
                    found = re.search(rf"^(?:bool|void) {cls}::{method}\(", source, re.MULTILINE)
                    if not found:
                        # Earlier versions lacked these helpers. Only the historical
                        # object regression is reached. Keep original setters intact.
                        fallbacks = {"RestoreTemporaryModel": "void CClientPed::RestoreTemporaryModel(){}",
                                     "AcquireRequestedModel": "bool CClientPed::AcquireRequestedModel(const char*){return false;}",
                                     "ReleaseLoadedModel": "void CClientPed::ReleaseLoadedModel(){}"}
                        if commit and method in ("ResolveUnconfirmedSkin", "RetainRequestedModel", "ReleaseModelReference"):
                            continue
                        if not commit or method not in fallbacks:
                            raise RuntimeError(f"Missing {cls}::{method}")
                        pieces.append(fallbacks[method])
                    else:
                        end = source.index("\n}", found.end()) + 2
                        pieces.append(source[found.start():end])
            (w / "EntityModelOwnership.generated.inl").write_text("\n\n".join(pieces))
            fixture = Path(__file__).with_name("ClientEntityModelOwnershipTests.cpp")
            shutil.copy(fixture, w)
            result["inputSha256"][fixture.name] = hashlib.sha256(fixture.read_bytes()).hexdigest()
            env = dict(os.environ, TMP=temp, TEMP=temp, TMPDIR=temp)
            env.pop("CL", None)
            env.pop("_CL_", None)
            if os.name == "nt":
                vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
                vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                              "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
                dev = Path(vs) / "Common7/Tools/VsDevCmd.bat"
                (w / "build.cmd").write_text(f'@echo off\ncall "{dev}" -no_logo -arch=x64 -host_arch=x64\nif errorlevel 1 exit /b 1\n'
                                             f'cl /nologo /EHsc /std:c++17 "{fixture.name}" /Fe:test.exe\n')
                build = [env["COMSPEC"], "/d", "/c", "build.cmd"]
                exe = w / "test.exe"
            else:
                build = ["c++", "-std=c++17", fixture.name, "-o", "test"]
                exe = w / "test"
            cp = subprocess.run(build, cwd=w, env=env, capture_output=True, timeout=120)
            (output / "compile.log").write_bytes(cp.stdout + cp.stderr)
            result["compile"] = cp.returncode
            if cp.returncode == 0:
                result["cases"] = {}
                with (output / "execute.log").open("wb") as log:
                    for case in ("ownership", "object-completion", "vehicle-completion", "release-refusal", "restore-refusal",
                                 "two-releases", "destruction-releases", "early-release", "final-restore"):
                        args = [str(exe)] if case == "ownership" else [str(exe), case]
                        cp = subprocess.run(args, cwd=w, env=env, capture_output=True, timeout=120)
                        log.write(case.encode() + b"\n" + cp.stdout + cp.stderr)
                        result["cases"][case] = cp.returncode
                result["execute"] = int(any(result["cases"].values()))
            result["success"] = result.get("execute") == 0
    finally:
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(output / "result.json")
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
