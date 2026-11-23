"""Compile the DE file-audio SDK tests with MSVC. Optionally sample explicitly supplied compatible archives."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audio-root", type=Path, help="directory with CONFIG, SFX and STREAMS")
    selected = parser.parse_args().audio_root
    if selected and not selected.is_absolute():
        parser.error("--audio-root must identify an absolute directory")
    out = ROOT / "Build/mta-game-file-audio" / uuid4().hex
    out.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/game-file-audio"
    scratch.mkdir(parents=True, exist_ok=True)
    fixture = Path(__file__).with_name("GameFileAudioDETests.cpp")
    reader = ROOT / "Client/sdk/game/CAudioContainerFiles.cpp"
    header = ROOT / "Client/sdk/game/CGame.h"
    inputs = [fixture, Path(__file__), fixture.with_name("AudioContainerSyntheticArchives.h"), reader, reader.with_suffix(".h"), header,
              ROOT / "Client/game_de/CGameFileAudioDE.h", ROOT / "Client/game_de/CGameModelLodResetDE.h"]
    result = {"sources": {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}, "cases": {}, "success": False}
    with tempfile.TemporaryDirectory(dir=scratch) as folder:
        work = Path(folder)
        # Leave the audio method to the production DE override. Fail on any unexpected SDK call.
        methods = []
        for signature in re.findall(r"virtual\s+([^;{}]+);", header.read_text()):
            if "GetAudioContainer(" not in signature:
                signature = re.sub(r"\s*=\s*0\s*$", "", signature)
                methods.append(f"{signature} override {{ std::abort(); }}")
        (work / "OtherGameMethods.generated.h").write_text("class OtherGameMethods : public CGame { public:\n" + "\n".join(methods) + "\n};")
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                      "-property", "installationPath"], text=True).strip()
        options = ["/nologo", "/std:c++17", "/EHsc", "/W4", "/WX", "/MD", "/DWIN32", "/DNOMINMAX", "/DWIN32_LEAN_AND_MEAN",
                   "/D_CRT_SECURE_NO_WARNINGS", "/D_SCL_SECURE_NO_WARNINGS", "/D_CRT_NONSTDC_NO_DEPRECATE",
                   "/I" + str(ROOT / "Shared/sdk"), "/I" + str(ROOT / "Client/sdk"), "/I" + str(work),
                   "/external:I" + str(ROOT / "vendor/sparsehash/src/windows"), "/external:I" + str(ROOT / "vendor/sparsehash/src"),
                   "/external:W0", str(fixture), str(reader), "/Fe:audio-test.exe"]
        (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64\n'
                                        "if errorlevel 1 exit /b 1\n" + subprocess.list2cmdline(["cl", *options]) + "\n")
        env = dict(os.environ, TMP=folder, TEMP=folder)
        env.pop("CL", None)
        env.pop("_CL_", None)

        def run(name, args):
            with (out / (name + ".log")).open("wb") as log:
                return subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        result["compile"] = run("compile", [env["COMSPEC"], "/d", "/c", "build.cmd"])
        if result["compile"] == 0:
            exe = work / "audio-test.exe"
            result["cases"]["synthetic"] = run("synthetic", [exe])
            if selected:
                result["selectedArchiveRoot"] = str(selected)
                result["cases"]["selectedArchives"] = run("selectedArchives", [exe, selected])
            result["success"] = not any(result["cases"].values())
    receipt = out / "test-result.json"
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(receipt)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
