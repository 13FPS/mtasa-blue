"""Exercise the DE CGame getters using objects from a normal Release x64 Game DE build."""
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
HOST = Path(__file__).with_name("GameCameraAudioDETests.cpp")


def msvc_environment():
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    install = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                       "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    command = f'call "{install}/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 >nul && set'
    result = subprocess.check_output(command, shell=True, text=True, errors="replace")
    env = {key.upper(): value for key, value in (line.split("=", 1) for line in result.splitlines() if "=" in line and not line.startswith("="))}
    env.update(VSLANG="1033")
    for name in ("CL", "_CL_", "LINK", "_LINK_", "MTA_DE_AUDIO_ROOT"):
        env.pop(name, None)
    return env


def missing_methods(diagnostics):
    # MSVC supplies the source declaration for each pure virtual. Only those
    # unimplemented slots get a rejecting override in the test receiver.
    errors = re.findall(r"\berror (C\d+):", diagnostics)
    if errors != ["C2259"]:
        raise RuntimeError("unexpected compiler diagnostics while enumerating SDK methods")
    decls = re.findall(r": is abstract\r?\n(.+?)\((\d+)(?:,\d+)?\): note: see declaration of '([^']+)'", diagnostics)
    if not decls or len(decls) != diagnostics.count(": is abstract"):
        raise RuntimeError("incomplete list of abstract methods")
    methods = []
    for filename, line, qualified_name in decls:
        lines = Path(filename.strip()).read_text(encoding="utf-8-sig").splitlines()
        signature = " ".join(lines[int(line) - 1:]).split(";", 1)[0].strip()
        match = re.fullmatch(r"virtual\s+(.+?)\s*=\s*0", signature)
        if not match:
            raise ValueError(signature)
        name = qualified_name.split("::")[-1]
        methods.append((name, match[1]))
    return methods


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("objects", type=Path, help="Game DE Release x64 intermediate directory")
    args = parser.parse_args()
    objects = sorted(args.objects.resolve().glob("*.obj"))
    required_objects = {"CGameCameraAudioDE.obj", "CCameraTransitionDE.obj", "CPoolsDE.obj", "GameSdkStrings.obj"}
    if required_objects.difference(obj.name for obj in objects):
        parser.error("build the current Game DE Release x64 project first")
    output = ROOT / "Build/definitive-recovery" / ("game-camera-audio-" + uuid4().hex)
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/game-camera-audio"
    scratch.mkdir(parents=True, exist_ok=True)
    env = msvc_environment()
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        work = Path(temporary)
        env.update(TEMP=temporary, TMP=temporary)

        def run(name, args, check=True):
            if args[0] in ("lib", "cl"):
                args[0] = Path(env["VCTOOLSINSTALLDIR"]) / "bin/Hostx64/x64" / (args[0] + ".exe")
            proc = subprocess.run(args, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
            (output / (name + ".log")).write_bytes(proc.stdout)
            if check and proc.returncode:
                raise RuntimeError(f"{name} failed: {output / (name + '.log')}")
            return proc.stdout.decode(errors="replace")

        includes = [work, ROOT / "Shared/sdk", ROOT / "Client/sdk", ROOT / "Client/game_de",
                    ROOT / "vendor/sparsehash/src/windows", ROOT / "vendor/sparsehash/src"]
        options = ["/nologo", "/std:c++20", "/EHsc", "/MT", "/Gy", "/O2", "/DWIN32", "/DNOMINMAX",
                   "/DMTA_RELEASE", "/D_CRT_SECURE_NO_WARNINGS", "/D_SCL_SECURE_NO_WARNINGS", *["/I" + str(path) for path in includes]]
        types = {"Game": "mta::game_de::CGameCameraAudioDE", "Audio": "mta::game_de::CAudioEngineDE",
                 "Camera": "mta::game_de::CCameraTransitionDE", "Pools": "mta::game_de::CPoolsDE"}
        required_methods = {"Game": {"GetCamera", "GetAudioEngine", "GetAESoundManager"},
                            "Audio": {"SetWorldSoundEnabled", "SetWorldSoundHandler"},
                            "Camera": {"GetTransitionState", "GetTransitionFOV", "GetTransitionMatrix", "IsInTransition"}}
        outstanding = {}
        for label, typename in types.items():
            probe = work / "probe.cpp"
            probe.write_text('#include "CGameCameraAudioDE.h"\n#include "CAudioEngineDE.h"\n#include "CCameraTransitionDE.h"\n#include "CPoolsDE.h"\n'
                             + f"struct Receiver : {typename} {{}};\nvoid CheckAbstract(Receiver) {{}}\n")
            methods = missing_methods(run("inspect-" + label, ["cl", *options, "/Zs", probe], check=False))
            outstanding[label] = [name for name, _ in methods]
            forbidden = required_methods.get(label, set())
            if forbidden.intersection(outstanding[label]):
                raise RuntimeError(f"missing production {label} methods: {forbidden.intersection(outstanding[label])}")
            (work / f"{label}.generated.inl").write_text("\n".join(sig + " override { std::abort(); }" for _, sig in methods))

        (work / "objects.rsp").write_text("\n".join(f'"{obj}"' for obj in objects))
        run("archive", ["lib", "/nologo", "/out:game.lib", "@objects.rsp"])
        # Deathmatch discovers the optional interface across the game DLL boundary.
        probe = work / "camera_client.cpp"
        probe.write_text('#include <cstdint>\n#include <SharedUtil.IntTypes.h>\n#include <game/CCamera.h>\n#include <game/CCameraTransitionFOV.h>\n'
                         'extern "C" __declspec(dllexport) const CCameraTransitionFOV* FindFovService(const CCamera* camera) {\n'
                         '    return dynamic_cast<const CCameraTransitionFOV*>(camera);\n}\n')
        run("client-dll", ["cl", *options, "/LD", probe, "/Fe:camera_client.dll"])
        run("compile", ["cl", *options, HOST, "/Fe:host.exe", "/link", "game.lib", "camera_client.lib", "bcrypt.lib", "dxgi.lib",
                        "d3d12.lib", "user32.lib", "/OPT:REF"])
        run("execute", [work / "host.exe"])
        sources = [HOST, Path(__file__), ROOT / "Client/game_de/CGameCameraAudioDE.h", ROOT / "Client/game_de/CGameCameraAudioDE.cpp",
                   ROOT / "Client/game_de/CAudioEngineDE.h", ROOT / "Client/sdk/game/CAudioEngineSoundPolicy.h",
                   ROOT / "Client/game_de/GameSdkStrings.cpp", ROOT / "Shared/sdk/SString.hpp", ROOT / "Client/game_de/CCameraTransitionDE.h",
                   ROOT / "Client/game_de/CCameraTransitionDE.cpp", ROOT / "Client/game_de/CCameraSphereVisibleDE.cpp",
                   ROOT / "Client/game_de/CPoolsDE.h", ROOT / "Client/game_de/CPoolsDE.cpp",
                   ROOT / "Client/sdk/game/CCameraTransitionPose.h", ROOT / "Client/sdk/game/CCameraTransitionFOV.h", ROOT / "Shared/sdk/CMatrix.h"]
        record = {"success": True, "abstractMethods": outstanding, "normalObjects": str(args.objects.resolve()),
                  "sources": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources},
                  "linkedObjects": {obj.name: hashlib.sha256(obj.read_bytes()).hexdigest() for obj in objects if obj.name in required_objects}}
        (output / "verification.json").write_text(json.dumps(record, indent=2) + "\n")
    print(output / "verification.json")


if __name__ == "__main__":
    main()
