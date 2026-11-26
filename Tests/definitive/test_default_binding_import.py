"""Exercise the ordinary Core default-binding import and its first-frame retry."""
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
    parser.add_argument("--source-commit")
    commit = parser.parse_args().source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-default-binding-import" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/default-binding-import"
    scratch.mkdir(parents=True, exist_ok=True)
    record = {"sourceCommit": commit, "inputs": {}, "cases": {}, "success": False}

    def source(name):
        data = subprocess.check_output(["git", "show", f"{commit}:{name}"], cwd=ROOT) if commit else (ROOT / name).read_bytes()
        record["inputs"][name] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    def method(text, name, params=r"[^)]*"):
        begin = re.search(r"^[\w* ]+ CKeyBinds::" + name + r"\(" + params + r"\)(?: const)?\n\{", text, re.M)
        return text[begin.start():text.index("\n}", begin.end()) + 2]

    with tempfile.TemporaryDirectory(dir=scratch) as tmp:
        work = Path(tmp)
        fixture = Path(__file__).with_name("DefaultBindingImportTests.cpp")
        shutil.copyfile(fixture, work / fixture.name)
        record["inputs"][fixture.relative_to(ROOT).as_posix()] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        binds = source("Client/core/CKeyBinds.cpp")
        pulse = method(binds, "DoPostFramePulse")
        pulse = pulse[:pulse.index("    if (systemState != SystemState::GS_PLAYING_GAME)")] + "}\n"
        sdk = source("Client/sdk/core/CKeyBindsInterface.h")
        types = sdk[sdk.index("enum eKeyData"):sdk.index("class CKeyFunctionBind :")]
        types += sdk[sdk.index("class CGTAControlBind :"):sdk.index("class CKeyBindsInterface\n")]
        (work / "BindTypes.inl").write_text(types)
        helpers = binds[binds.index("template <typename Container, typename Predicate, typename UnaryFunction>"):binds.index("bool CKeyBinds::ProcessMessage")]
        imports = [helpers] + [method(binds, name) for name in ("LoadFromXML", "LoadDefaultBinds", "LoadDefaultCommands", "LoadControlsFromGTA",
                                                               "ClearCommandsAndControls", "GetBindableFromKey", "GetBindableFromAction", "GetBindableFromGTARelative")]
        imports += [method(binds, "AddGTAControl", r"const SBindableKey\*[^)]*"), method(binds, "RemoveAllGTAControls", "")]
        for name in ("HasUserCommand", "ImportGTAControls", "AddDefaultCommandsTo"):
            if "CKeyBinds::" + name + "(" in binds:
                imports.append(method(binds, name))
        commands = source("Client/core/CCommandFuncs.cpp")
        begin = commands.index("void CCommandFuncs::CopyGTAControls(")
        imports.append(commands[begin:commands.index("\n}", begin) + 2])
        (work / "DefaultBindingMethods.inl").write_text("\n".join(imports + [pulse]))
        for name in ("Client/sdk/game/CControllerConfigManager.h", "Shared/sdk/CControllerActions.h"):
            (work / Path(name).name).write_text(source(name))
        env = dict(os.environ, TMP=tmp, TEMP=tmp, TMPDIR=tmp)
        env.pop("CL", None)
        env.pop("_CL_", None)
        if os.name == "nt":
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                          "-property", "installationPath"], text=True).strip()
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64\n'
                                            f'if errorlevel 1 exit /b 1\ncl /nologo /EHsc /std:c++17 /I. {fixture.name} /Fe:test.exe\n')
            compile_cmd, exe = [env["COMSPEC"], "/d", "/c", "build.cmd"], work / "test.exe"
        else:
            compile_cmd, exe = ["c++", "-std=c++17", "-I.", fixture.name, "-o", "test"], work / "test"

        def run(label, args):
            with (output / f"{label}.log").open("wb") as log:
                return subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        record["compile"] = run("build", compile_cmd)
        if not record["compile"]:
            for case in ("normal", "append-controls", "stale-defaults", "missing-game", "missing-controller", "interrupted", "invalid-joystick",
                         "command-defaults", "copy-unavailable", "copy-success"):
                record["cases"][case] = run(case, [exe, case])
            record["success"] = not any(record["cases"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(receipt)
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
