"""Run the DE sound policy and file-playback SDK tests. Optionally decode installed compatible archives."""
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
    parser.add_argument("--audio-root", type=Path, help="directory with CONFIG, SFX and STREAMS")
    parser.add_argument("--decode", action="store_true", help="decode selected archive samples with the staged x64 BASS library")
    args = parser.parse_args()
    selected = args.audio_root
    if args.decode and not selected:
        parser.error("--decode requires an explicit --audio-root")
    if selected and not selected.is_absolute():
        parser.error("--audio-root must identify an absolute directory")
    out = ROOT / "Build/mta-game-file-audio" / uuid4().hex
    out.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/game-file-audio"
    scratch.mkdir(parents=True, exist_ok=True)
    fixture = Path(__file__).with_name("GameFileAudioDETests.cpp")
    policy_fixture = Path(__file__).with_name("AudioEngineDETests.cpp")
    reader = ROOT / "Client/sdk/game/CAudioContainerFiles.cpp"
    header = ROOT / "Client/sdk/game/CGame.h"
    manager = ROOT / "Client/mods/deathmatch/logic/CClientSoundManager.cpp"
    bass_dll = ROOT / "Bin/mods/deathmatch/x64/bass.dll"
    bass_lib = ROOT / "vendor/bass/lib/x64/bass.lib"
    inputs = [fixture, policy_fixture, Path(__file__), fixture.with_name("GTASfxPlaybackFixture.h"), fixture.with_name("AudioContainerSyntheticArchives.h"),
              manager, reader, reader.with_suffix(".h"), header, ROOT / "Client/game_de/CGameFileAudioDE.h", ROOT / "Client/game_de/CGameModelLodResetDE.h",
              ROOT / "Client/sdk/game/CAudioEngine.h", ROOT / "Client/sdk/game/CAudioEngineSoundPolicy.h", ROOT / "Client/sdk/game/CAESoundManager.h",
              ROOT / "Client/game_de/CAudioEngineDE.h"]
    if args.decode:
        inputs.extend([bass_dll, bass_lib, ROOT / "vendor/bass/bass.h"])
    result = {"sources": {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}, "cases": {}, "success": False}
    with tempfile.TemporaryDirectory(dir=scratch) as folder:
        work = Path(folder)
        # Compile the production public methods against the test's captured sound output.
        source = manager.read_text()
        start = source.index("static void SetInitialGTASFXVolume(")
        end = source.index("int CClientSoundManager::GetFxEffectFromName(", start)
        (work / "GTASfxPlayback.generated.inl").write_text(source[start:end])
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
                   "/external:W0"]
        archive_options = [*options, str(fixture), str(reader), "/Fe:audio-test.exe"]
        if args.decode:
            shutil.copyfile(bass_dll, work / "bass.dll")
            archive_options.extend(["/DMTA_TEST_BASS_DECODE", "/external:I" + str(ROOT / "vendor/bass"), str(bass_lib)])
        policy_options = [*options, str(policy_fixture), "/Fe:sound-policy-test.exe"]
        (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64\n'
                                        "if errorlevel 1 exit /b 1\n" + subprocess.list2cmdline(["cl", *archive_options]) +
                                        "\nif errorlevel 1 exit /b 1\n" + subprocess.list2cmdline(["cl", *policy_options]) + "\n")
        env = dict(os.environ, TMP=folder, TEMP=folder)
        env.pop("CL", None)
        env.pop("_CL_", None)

        def run(name, args):
            with (out / (name + ".log")).open("wb") as log:
                return subprocess.run(args, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode

        result["compile"] = run("compile", [env["COMSPEC"], "/d", "/c", "build.cmd"])
        if result["compile"] == 0:
            exe = work / "audio-test.exe"
            result["cases"]["soundPolicy"] = run("soundPolicy", [work / "sound-policy-test.exe"])
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
