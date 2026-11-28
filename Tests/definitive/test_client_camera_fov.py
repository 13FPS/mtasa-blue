"""Compile Lua's camera matrix entry and its production client calls over controlled SDK receivers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[2]
LOGIC = "Client/mods/deathmatch/logic/"
TEST = Path(__file__).with_name("ClientCameraFovTests.cpp")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", help="Run historical production methods against the current fixture")
    parser.add_argument("--arch", choices=("x64", "x86"), default="x64")
    args = parser.parse_args()
    if args.source_commit and not re.fullmatch(r"[a-fA-F0-9]{7,40}", args.source_commit):
        parser.error("Expected a hexadecimal commit ID")
    out = ROOT / "Build/mta-client-camera-fov" / uuid4().hex
    out.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/client-camera-fov"
    scratch.mkdir(parents=True, exist_ok=True)
    record = {"success": False, "sourceCommit": args.source_commit, "branch": args.arch, "platform": platform.system(), "sourceHashes": {}}

    def source(path):
        raw = subprocess.check_output(["git", "show", f"{args.source_commit}:{path}"], cwd=ROOT) if args.source_commit else (ROOT / path).read_bytes()
        record["sourceHashes"][path] = hashlib.sha256(raw).hexdigest()
        return raw.decode("utf-8-sig").replace("\r\n", "\n")

    def method(src, signature):
        start = src.index(signature)
        return src[start:src.index("\n}", start) + 2]

    try:
        with tempfile.TemporaryDirectory(dir=scratch) as temp:
            work = Path(temp)
            env = dict(os.environ, TMP=temp, TEMP=temp, TMPDIR=temp)
            env.pop("CL", None)
            env.pop("_CL_", None)
            camera = source(LOGIC + "CClientCamera.cpp")
            camera_header = source(LOGIC + "CClientCamera.h")
            availability = [line.strip() for line in camera_header.splitlines() if "HasGameCamera() const" in line]
            (work / "CameraAvailability.inl").write_text("\n".join(availability))
            helpers = camera[camera.index("namespace\n{"):camera.index("    inline CMatrix& ScratchMatrix()")]
            degrees = method(source("Shared/mods/deathmatch/logic/Utils.h"), "inline float ConvertDegreesToRadiansNoWrap(float")
            (work / "FixedCameraMath.inl").write_text(degrees + "\n" + helpers + "}\n", encoding="utf-8")
            classic = source("Client/game_sa/CCameraSA.cpp")
            (work / "ClassicTransition.inl").write_text(method(classic, "bool CCameraSA::GetTransitionMatrix("), encoding="utf-8")
            fov = method(camera, "float CClientCamera::GetAccurateFOV() const")
            # Never compile the prior process read. A sentinel distinguishes
            # selection of the Classic field from any SDK receiver.
            old_read = "return *(float*)VAR_CurrentCameraFOV;"
            if fov.count(old_read) != 1:
                raise ValueError("Classic camera read changed; review the sentinel substitution")
            fov = fov.replace(old_read, "return classicFov;")
            fov = re.sub(r"^\s*constexpr[^\n]*VAR_CurrentCameraFOV[^\n]*$", "", fov, flags=re.M)
            if "0x" in fov or "VAR_CurrentCameraFOV" in fov:
                raise ValueError("Unexpected address in the isolated camera fixture")
            lua = source(LOGIC + "luadefs/CLuaCameraDefs.cpp")
            if '{"getCameraMatrix", ArgumentParserWarn<false, GetCameraMatrix>}' not in lua:
                raise ValueError("Lua no longer registers the selected function")
            functions = source(LOGIC + "CStaticFunctionDefinitions.cpp")
            rpc = source(LOGIC + "rpc/CCameraRPCs.cpp")
            gta_matrix = "bool CClientCamera::GetGtaMatrix(" if "bool CClientCamera::GetGtaMatrix(" in camera else "CMatrix CClientCamera::GetGtaMatrix() const"
            methods = ("bool CClientCamera::IsInCameraTransition() const", gta_matrix, "bool CClientCamera::GetMatrix(",
                       "const CMatrix& CClientCamera::AcquirePulseMatrix() const", "void CClientCamera::InvalidateCachedTransforms() const",
                       "void CClientCamera::PersistViewModes()", "void CClientCamera::SetFixedTarget(",
                       "void CClientCamera::GetFixedTarget(", "void CClientCamera::GetPosition(",
                       "void CClientCamera::FadeIn(", "void CClientCamera::FadeOut(",
                       "void CClientCamera::SetCameraVehicleViewMode(", "void CClientCamera::SetCameraPedViewMode(",
                       "eVehicleCamMode CClientCamera::GetCameraVehicleViewMode()", "ePedCamMode CClientCamera::GetCameraPedViewMode()")
            if args.source_commit and "CMatrix CClientCamera::GetInterpolatedCameraMatrix() const" in camera:
                methods += ("CMatrix CClientCamera::GetInterpolatedCameraMatrix() const",)
            if "bool CClientCamera::ApplyFade(" in camera:
                methods += ("bool CClientCamera::ApplyFade(",)
            code = [*(method(camera, signature) for signature in methods), fov,
                    method(functions, "bool CStaticFunctionDefinitions::GetCameraMatrix("),
                    method(functions, "bool CStaticFunctionDefinitions::FadeCamera("),
                    method(rpc, "void CCameraRPCs::FadeCamera("),
                    method(lua, "bool CLuaCameraDefs::SetCameraViewMode("),
                    method(lua, "CLuaMultiReturn<float, float, float, float, float, float, float, float> CLuaCameraDefs::GetCameraMatrix()")]
            (work / "CameraMethods.inl").write_text("\n".join(code), encoding="utf-8")
            for cls, exercised in (("CCamera", {"GetActiveCam", "GetCam", "GetTransitionFOV", "IsInTransition", "GetMatrix", "GetTransitionMatrix",
                                                "Fade", "SetFadeColor",
                                                "SetCameraVehicleViewMode", "SetCameraPedViewMode", "GetCameraVehicleViewMode", "GetCameraPedViewMode"}),
                                   ("CCam", {"GetFOV", "GetFront", "GetUp", "GetSource"})):
                sdk = source(f"Client/sdk/game/{cls}.h")
                signatures = re.findall(r"^\s*virtual\s+([^;]+?)\s*= 0;", sdk, re.M)
                bodies = [sig.strip() + " override { std::abort(); }" for sig in signatures if re.search(r"(\w+)\(", sig)[1] not in exercised]
                (work / f"Unused{cls}.inl").write_text("\n".join(bodies), encoding="utf-8")
            for path in (TEST, Path(__file__), TEST.with_name("CameraTransitionPoseChecks.h"), ROOT / "Client/sdk/game/CCameraTransitionPose.h"):
                record["sourceHashes"][path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
            ext = "Client/sdk/game/CCameraTransitionFOV.h"
            record["sourceHashes"][ext] = hashlib.sha256((ROOT / ext).read_bytes()).hexdigest()
            dirs = [work, ROOT / "Client/sdk", ROOT / "Shared/sdk", ROOT / "Shared/mods/deathmatch/logic/lua"]
            if os.name == "nt":
                vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
                vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                              "-property", "installationPath"], text=True).strip()
                include = " ".join(f'/I"{d}"' for d in dirs)
                (work / "build.cmd").write_text(f'@echo off\ncall "{vs}/Common7/Tools/VsDevCmd.bat" -no_logo -arch={args.arch} -host_arch=x64\n'
                                                f'if errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc {include} "{TEST}" /Fe:test.exe\n')
                compile_cmd = [os.environ["COMSPEC"], "/d", "/c", "build.cmd"]
                exe = work / "test.exe"
            else:
                # On Linux we compile the selected Windows preprocessor branch.
                exe = work / "test"
                compile_cmd = ["c++", "-std=c++20", *[f"-I{d}" for d in dirs], *(["-D_WIN64"] if args.arch == "x64" else []),
                               str(TEST), "-o", str(exe)]
            for phase, cmd in (("compile", compile_cmd), ("execute", [str(exe)])):
                with (out / (phase + ".log")).open("wb") as log:
                    record[phase] = subprocess.run(cmd, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode
                if record[phase]:
                    break
            record["success"] = record.get("execute") == 0
    finally:
        receipt = out / "result.json"
        receipt.write_text(json.dumps(record, indent=2) + "\n")
        print(receipt)
    return int(not record["success"])


if __name__ == "__main__":
    raise SystemExit(main())
