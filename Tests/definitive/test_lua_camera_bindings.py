"""Run legacy camera callbacks in the packaged Windows Lua client with recording SDK camera and HUD objects."""
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
HOST = Path(__file__).with_name("LuaCameraBindingTests.cpp")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def definition(source, signature):
    start = source.index(signature)
    # The production functions selected here have no braces in literal strings or comments.
    braces = 0
    for match in re.finditer(r"[{}]", source[start:]):
        braces += 1 if match[0] == "{" else -1
        if not braces:
            return source[start:start + match.end()]
    raise ValueError("Incomplete source definition: " + signature)


def stub_unrelated_methods(header, implemented):
    signatures = re.findall(r"virtual\s+([^;\n]+?\b(\w+)\([^;\n]*?\)(?:\s+const)?(?:\s+noexcept)?)\s*=\s*0;", header)
    if not implemented <= {name for _, name in signatures}:
        raise ValueError("Expected SDK methods are missing")
    return "\n".join(decl + " override { std::abort(); }" for decl, name in signatures if name not in implemented)


def msvc_environment(arch):
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([vswhere, "-latest", "-products", "*", "-requires",
                                  "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    dev = Path(vs) / "Common7/Tools/VsDevCmd.bat"
    raw = subprocess.check_output(f'call "{dev}" -no_logo -arch={arch} -host_arch=x64 >nul && set', shell=True, text=True, errors="replace")
    env = {key.upper(): value for key, value in (line.split("=", 1) for line in raw.splitlines() if "=" in line and not line.startswith("="))}
    env["VSLANG"] = "1033"
    for option in ("CL", "_CL_", "LINK", "_LINK_"):
        env.pop(option, None)
    return env


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=("x64", "x86"), default="x64")
    parser.add_argument("--clip-reference", help="Replace only the two Lua clip callbacks with their definitions from this Git commit")
    args = parser.parse_args()
    if args.clip_reference and not re.fullmatch(r"[0-9a-fA-F]{7,40}", args.clip_reference):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-lua-camera-bindings" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/lua-camera-bindings"
    scratch.mkdir(parents=True, exist_ok=True)
    record = {"success": False, "architecture": args.arch, "clipReference": args.clip_reference,
              "gameStarted": False, "networkStarted": False, "inputs": {},
              "scope": "Packaged Lua compiles and invokes production callbacks, boolean and number readers, manager lookup and camera methods. "
                       "Camera/HUD SDK receivers record calls. The host supplies session pointers and counts script errors without formatting diagnostic text."}

    def source(path):
        record["inputs"][path] = sha(ROOT / path)
        return (ROOT / path).read_text(encoding="utf-8-sig")

    def run(label, command):
        result = subprocess.run(command, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        (output / (label + ".log")).write_bytes(result.stdout)
        if result.returncode:
            raise RuntimeError(f"{label} exited {result.returncode}: {output / (label + '.log')}")
        return result.stdout.decode(errors="replace")

    try:
        env = msvc_environment(args.arch)
        with tempfile.TemporaryDirectory(dir=scratch) as tmp:
            work = Path(tmp)
            env.update(TMP=tmp, TEMP=tmp)
            lua_dir = ROOT / "Bin/mods/deathmatch"
            if args.arch == "x64":
                lua_dir /= "x64"
            dll, lib = lua_dir / "lua5.1c.dll", lua_dir / "lua5.1c.lib"
            shutil.copy2(dll, work / dll.name)
            record["runtime"] = {"module": str(dll), "sha256": sha(dll), "importLibrarySha256": sha(lib)}
            for header in ("lua.h", "luaconf.h", "lauxlib.h"):
                source("vendor/lua/src/" + header)
            record["inputs"][HOST.relative_to(ROOT).as_posix()] = sha(HOST)

            callback_path = "Client/mods/deathmatch/logic/luadefs/CLuaCameraDefs.cpp"
            callbacks = source(callback_path)
            clip = callbacks
            if args.clip_reference:
                revision = subprocess.check_output(["git", "rev-parse", "--verify", args.clip_reference + "^{commit}"], cwd=ROOT, text=True).strip()
                historical = subprocess.check_output(["git", "show", revision + ":" + callback_path], cwd=ROOT)
                clip = historical.decode("utf-8-sig").replace("\r\n", "\n")
                record.update(clipReference=revision, clipReferenceSourceSha256=hashlib.sha256(historical).hexdigest())
            for name, function in (("fadeCamera", "FadeCamera"), ("getCameraClip", "GetCameraClip"), ("setCameraClip", "SetCameraClip")):
                if f'{{"{name}", {function}}}' not in callbacks:
                    raise ValueError("Missing Lua registration for " + name)
            static = source("Client/mods/deathmatch/logic/CStaticFunctionDefinitions.cpp")
            camera = source("Client/mods/deathmatch/logic/CClientCamera.cpp")
            game = source("Client/mods/deathmatch/logic/CClientGame.h")
            manager = source("Client/mods/deathmatch/logic/CClientManager.h")
            reader = source("Shared/sdk/CScriptArgReader.h")
            color = source("Shared/sdk/SharedUtil.Misc.h")
            (work / "Color.inl").write_text("namespace SharedUtil {\n" + definition(color, "class SColor\n") + ";\n}\nusing SharedUtil::SColor;\n")
            (work / "ClientGame.inl").write_text("\n".join(definition(game, sig) for sig in ("void SetInitiallyFadedOut(", "bool GetHudAreaNameDisabled(")))
            (work / "Manager.inl").write_text(re.search(r"CClientCamera\*\s+GetCamera\(\) *\{[^}]+}", manager)[0])
            camera_methods = ("void CClientCamera::FadeIn(", "void CClientCamera::FadeOut(", "bool CClientCamera::ApplyFade(",
                              "bool CClientCamera::SetCameraClip(", "void CClientCamera::GetCameraClip(")
            (work / "Binding.inl").write_text("\n".join(
                [definition(camera, sig) for sig in camera_methods] + [definition(static, "bool CStaticFunctionDefinitions::FadeCamera("),
                 definition(callbacks, "int CLuaCameraDefs::FadeCamera("), definition(clip, "int CLuaCameraDefs::SetCameraClip("),
                 definition(clip, "int CLuaCameraDefs::GetCameraClip(")]))
            reader_methods = ("CScriptArgReader(lua_State* luaVM)", "~CScriptArgReader()", "template <typename T>\n    void ReadNumber(",
                              "template <typename T, typename U>\n    void ReadNumber(", "void ReadBool(bool& bOutValue)",
                              "void ReadBool(bool& bOutValue, const bool bDefaultValue)", "void SetTypeError(", "void SetCustomError(",
                              "bool IsReadFunctionPending() const", "bool HasErrors(")
            (work / "Reader.inl").write_text("\n".join(definition(reader, sig) for sig in reader_methods))
            for name, methods in (("Camera", {"Fade", "SetFadeColor", "GetCameraClip", "SetCameraClip"}), ("Hud", {"SetComponentVisible"})):
                (work / (name + ".inl")).write_text(stub_unrelated_methods(source(f"Client/sdk/game/C{name}.h"), methods))

            compiler = Path(env["VCTOOLSINSTALLDIR"]) / "bin/Hostx64" / args.arch / "cl.exe"
            includes = [work, ROOT / "Client/sdk", ROOT / "Shared/sdk", ROOT / "vendor/lua/src"]
            run("compile", [compiler, "/nologo", "/std:c++20", "/EHsc", "/MT", "/Gy", "/O2", "/DWIN32", "/DNOMINMAX",
                            "/DMTA_CLIENT", "/DLUA_BUILD_AS_DLL", "/DMTA_RELEASE", "/D_CRT_SECURE_NO_WARNINGS",
                            *["/I" + str(p) for p in includes], HOST, "/Fe:bindings.exe", "/link", lib, "/OPT:REF"])
            exe = work / "bindings.exe"
            imports = run("imports", [compiler.with_name("dumpbin.exe"), "/imports", exe]).lower()
            if "lua5.1c.dll" not in imports or "lual_loadstring" not in imports or "netc.dll" in imports:
                raise ValueError("Unexpected Lua host dependencies")
            record["hostSha256"] = sha(exe)
            result = run("execute", [exe])
            checks, calls, bits = re.fullmatch(r"passed (\d+) checks across (\d+) Lua scripts, (\d+)-bit", result.strip()).groups()
            assert int(bits) == (64 if args.arch == "x64" else 32)
            assert sha(work / dll.name) == sha(dll) == record["runtime"]["sha256"]
            assert all(sha(ROOT / path) == h for path, h in record["inputs"].items())
            record.update(success=True, checks=int(checks), scriptCalls=int(calls), sourcesUnchanged=True)
    except (OSError, ValueError, RuntimeError, AssertionError) as exc:
        record["failure"] = str(exc)
    receipt = output / "result.json"
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(receipt)
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
