"""Run the Core startup settings pass with saved XML and recording SDK receivers."""
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
    parser.add_argument("--source-commit", type=str)
    parser.add_argument("--arch", choices=("x64", "x86"), default="x64", help="Windows target architecture")
    args = parser.parse_args()
    commit = args.source_commit
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-core-game-settings" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/core-game-settings"
    scratch.mkdir(parents=True, exist_ok=True)
    record = {"sourceCommit": commit, "inputs": {}, "cases": {}, "success": False, "architecture": args.arch if os.name == "nt" else "host"}

    def source(path):
        data = subprocess.check_output(["git", "show", f"{commit}:{path}"], cwd=ROOT) if commit else (ROOT / path).read_bytes()
        record["inputs"][path] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    def method(text, signature):
        begin = text.index(signature + "\n{")
        return text[begin:text.index("\n}", begin) + 2]

    with tempfile.TemporaryDirectory(dir=scratch) as temp:
        work = Path(temp)
        fixture = Path(__file__).with_name("CoreGameSettingsTests.cpp")
        shutil.copyfile(fixture, work / fixture.name)
        record["inputs"][fixture.relative_to(ROOT).as_posix()] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        dialog_fixture = fixture.with_name("MouseControlsDialogTests.inl")
        shutil.copyfile(dialog_fixture, work / dialog_fixture.name)
        record["inputs"][dialog_fixture.relative_to(ROOT).as_posix()] = hashlib.sha256(dialog_fixture.read_bytes()).hexdigest()
        core = source("Client/core/CCore.cpp")
        settings = source("Client/core/CSettings.cpp")
        load = method(settings, "void CSettings::LoadData()")
        controls_load = load.split("    // Controls\n", 1)[1].split("    // Audio\n", 1)[0]
        volume_snapshot = load.split('m_pComboUsertrackMode->SetText(_("Sequential"));', 1)[1]
        snapshot_end = 'm_pAudioVoiceVolume->SetScrollPosition(m_fOldVoiceVolume);'
        volume_snapshot = volume_snapshot[:volume_snapshot.index(snapshot_end) + len(snapshot_end)]
        save = method(settings, "void CSettings::SaveData()")
        controls = save[save.index("    CControllerConfigManager*"):save.index("    // Video\n")]
        mouse = next(line for line in save.splitlines() if "gameSettings->SetMouseSensitivity(" in line)
        (work / "DialogLoad.generated.inl").write_text(controls_load + volume_snapshot)
        (work / "DialogSave.generated.inl").write_text(controls + mouse)
        (work / "DialogButtons.generated.inl").write_text("\n".join(method(settings, signature) for signature in (
            "bool CSettings::OnControlsDefaultClick(CGUIElement* pElement)", "bool CSettings::OnOKButtonClick(CGUIElement* pElement)")))
        xml = source("Shared/XML/CXMLNodeImpl.cpp")
        variables = source("Client/core/CClientVariables.cpp")
        (work / "CClientVariables.h").write_text(source("Client/core/CClientVariables.h"))
        (work / "ClientVariables.generated.inl").write_text("\n".join(method(variables, sig) for sig in (
            "CClientVariables::CClientVariables()", "CClientVariables::~CClientVariables()",
            "CXMLNode* CClientVariables::Node(const std::string& strVariable)", "bool CClientVariables::Exists(const std::string& strVariable)",
            "bool CClientVariables::Get(const std::string& strVariable, CVector2D& val)", "void CClientVariables::Set(const std::string& strVariable, CVector2D val)",
            "void CClientVariables::ClampValue(const std::string& strVariable, CVector2D minValue, CVector2D maxValue)",
            "void CClientVariables::ClampValue(const std::string& strVariable, float fMinValue, float fMaxValue)",
            "void CClientVariables::ClampValue(const std::string& strVariable, int iMinValue, int iMaxValue)")))
        for header in ("CSingleton.h", "xml/CXMLNode.h", "xml/CXMLFile.h", "CChat.h"):
            (work / header).parent.mkdir(parents=True, exist_ok=True)
            (work / header).touch()
        for header in ("SharedUtil.Defines.h", "CVector.h", "CVector2D.h", "CVector4D.h"):
            (work / header).write_text(source("Shared/sdk/" + header), encoding="utf-8")
        (work / "ApplyGameSettings.generated.inl").write_text(method(core, "void CCore::ApplyGameSettings()"))
        (work / "ApplyConsoleSettings.generated.inl").write_text(method(core, "void CCore::ApplyConsoleSettings()"))
        (work / "XmlScalars.generated.inl").write_text("\n".join(method(xml, prefix) for prefix in (
            "bool CXMLNodeImpl::GetTagContent(bool& bContent)", "bool CXMLNodeImpl::GetTagContent(int& iContent)", "bool CXMLNodeImpl::GetTagContent(unsigned int& uiContent)",
            "bool CXMLNodeImpl::GetTagContent(float& fContent)", "void CXMLNodeImpl::SetTagContent(bool bContent)",
            "bool CXMLNodeImpl::StringToLong(const char* szString, long& lValue)")))
        for path in ("Client/sdk/core/CCVarsInterface.h", "Client/sdk/game/CSettings.h", "vendor/tinyxml2/tinyxml2.h", "vendor/tinyxml2/tinyxml2.cpp"):
            relative = path.removeprefix("Client/sdk/").removeprefix("vendor/tinyxml2/")
            (work / relative).parent.mkdir(parents=True, exist_ok=True)
            (work / relative).write_text(source(path), encoding="utf-8")
        (work / "game/CControllerConfigManager.h").write_text(source("Client/sdk/game/CControllerConfigManager.h"))
        (work / "CControllerActions.h").write_text(source("Shared/sdk/CControllerActions.h"))
        env = dict(os.environ, TMP=temp, TEMP=temp, TMPDIR=temp)
        env.pop("CL", None)
        env.pop("_CL_", None)
        if os.name == "nt":
            vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                          "-property", "installationPath"], text=True).strip()
            (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch={args.arch} -host_arch=x64\n'
                                             f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc /DWIN32 /I. {fixture.name} tinyxml2.cpp /Fe:test.exe\n')
            command, test = [env["COMSPEC"], "/d", "/c", "build.cmd"], work / "test.exe"
        else:
            command, test = ["c++", "-std=c++17", "-I.", fixture.name, "tinyxml2.cpp", "-o", "test"], work / "test"
        def run(label, args):
            with (output / f"{label}.log").open("wb") as log:
                return subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode
        record["compile"] = run("build", command)
        if not record["compile"]:
            record["cases"] = {case: run(case, [test, case]) for case in (
                "saved", "unreadable", "ranges", "migration", "profile-floats", "profile-integers", "integer-startup", "controls-load", "controls-numbers",
                "controls-defaults", "controls-invalid-nick", "controls-volumes", "console-layout", "profile-vector2d")}
            record["success"] = not any(record["cases"].values())
    receipt = output / "result.json"
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(receipt)
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
