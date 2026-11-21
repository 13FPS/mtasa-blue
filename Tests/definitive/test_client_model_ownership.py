"""Compile client model/TXD ownership against a controlled game and texture pool."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[2]
LOGIC = ROOT / "Client/mods/deathmatch/logic"


def main():
    output = ROOT / "Build/mta-client-model-ownership" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/client-model-ownership"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        work = Path(temporary)
        model = (LOGIC / "CClientModel.cpp").read_text(encoding="utf-8-sig")
        manager = (LOGIC / "CClientModelManager.cpp").read_text(encoding="utf-8-sig")
        lua = (LOGIC / "luadefs/CLuaEngineDefs.cpp").read_text(encoding="utf-8-sig")
        model = model[model.index("CClientModel::CClientModel("):]
        model = model[:model.index("void CClientModel::RestoreDFF(")] + model[model.index("bool CClientModel::AllocateTXD("):]
        manager = manager[manager.index("CClientModelManager::CClientModelManager("):]
        request = lua[lua.index("int CLuaEngineDefs::EngineRequestModel("):lua.index("int CLuaEngineDefs::EngineFreeModel(")]
        txd = lua[lua.index("std::variant<bool, uint> CLuaEngineDefs::EngineRequestTXD("):lua.index("size_t CLuaEngineDefs::EngineGetPoolCapacity(")]
        (work / "ModelOwnership.generated.inl").write_text("\n".join([model, manager, request, txd]), encoding="utf-8")
        shutil.copy(Path(__file__).with_name("ClientModelOwnershipTests.cpp"), work)
        for header in ("CClientModelManager.h", "CClientModel.h"):
            shutil.copy(LOGIC / header, work)
        if os.name == "nt":
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            installation = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
            developer = Path(installation) / "Common7/Tools/VsDevCmd.bat"
            (work / "build.cmd").write_text(f'@echo off\ncall "{developer}" -no_logo -arch=x64 -host_arch=x64\n'
                                            'if errorlevel 1 exit /b 1\ncl /nologo /EHsc /std:c++17 ClientModelOwnershipTests.cpp /Fe:test.exe\n')
            build = [os.environ["COMSPEC"], "/d", "/c", "build.cmd"]
            exe = work / "test.exe"
        else:
            build = ["c++", "-std=c++17", "ClientModelOwnershipTests.cpp", "-o", "test"]
            exe = work / "test"
        record = {}
        env = dict(os.environ, TMP=temporary, TEMP=temporary, TMPDIR=temporary)
        env.pop("CL", None)
        env.pop("_CL_", None)
        for phase, args in (("compile", build), ("execute", [str(exe)])):
            result = subprocess.run(args, cwd=work, env=env, capture_output=True, timeout=120)
            (output / (phase + ".log")).write_bytes(result.stdout + result.stderr)
            record[phase] = result.returncode
            if result.returncode:
                break
        record["success"] = record.get("execute") == 0
        (output / "result.json").write_text(json.dumps(record, indent=2) + "\n")
        print(output / "result.json")
        return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
