"""Check the return types of the two sound queries using the packaged MTA Lua client."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from uuid import uuid4

# Isolated Python (-I) omits sibling test modules from the search path.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_lua_camera_bindings import ROOT, definition, msvc_environment, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=("x86", "x64"), default="x64")
    parser.add_argument("--source-commit", help="Compile the two getters from an earlier commit with the same Lua result handling")
    args = parser.parse_args()
    if args.source_commit and not re.fullmatch(r"[0-9a-fA-F]{7,40}", args.source_commit):
        parser.error("Expected a hexadecimal commit ID")
    output = ROOT / "Build/mta-lua-sound-queries" / uuid4().hex
    output.mkdir(parents=True)
    scratch = ROOT / "Build/tmp/lua-sound-queries"
    scratch.mkdir(parents=True, exist_ok=True)
    host = Path(__file__).with_name("LuaSoundQueryTests.cpp")
    record = {"success": False, "architecture": args.arch, "sourceCommit": args.source_commit, "currentSources": {},
              "scope": "Packaged Lua, actual sound getter bodies, CLuaFunctionParser variant/scalar PushResult and LuaBasic scalar pushes. "
                       "The sound pointer and its audio values are controlled. No native audio, game or network startup."}

    def current(path):
        record["currentSources"][path] = sha(ROOT / path)
        return (ROOT / path).read_text(encoding="utf-8-sig")

    try:
        env = msvc_environment(args.arch)
        with tempfile.TemporaryDirectory(dir=scratch) as tmp:
            work = Path(tmp)
            env.update(TMP=tmp, TEMP=tmp)
            audio_path = "Client/mods/deathmatch/logic/luadefs/CLuaAudioDefs.cpp"
            audio = current(audio_path)
            if args.source_commit:
                commit = subprocess.check_output(["git", "rev-parse", "--verify", args.source_commit + "^{commit}"], cwd=ROOT, text=True).strip()
                raw = subprocess.check_output(["git", "show", commit + ":" + audio_path], cwd=ROOT)
                audio = raw.decode("utf-8-sig").replace("\r\n", "\n")
                record.update(sourceCommit=commit, referenceSourceSha256=hashlib.sha256(raw).hexdigest())
            methods = []
            for name, t in (("GetSoundBPM", "float"), ("GetSoundBufferLength", "double")):
                if not re.search(r'\{"g' + name[1:] + r'",\s*ArgumentParserWarn<\w+,\s*' + name + r'>}', audio):
                    raise ValueError("Missing typed Lua registration for " + name)
                methods.append(definition(audio, f"std::variant<{t}, bool> CLuaAudioDefs::{name}("))
            (work / "SoundQueries.generated.inl").write_text("\n".join(methods))
            parser_code = current("Shared/mods/deathmatch/logic/lua/CLuaFunctionParser.h")
            variant = definition(parser_code, "int PushResult(lua_State* L, const std::variant<Ts...>& result)")
            scalar = definition(parser_code, "int PushResult(lua_State* L, const T& value)")
            (work / "PushResult.generated.inl").write_text("template <typename... Ts>\n" + variant + "\ntemplate <typename T>\n" + scalar)
            basic = current("Shared/mods/deathmatch/logic/lua/LuaBasic.h")
            (work / "ScalarPush.generated.inl").write_text("\n".join(definition(basic, f"inline void Push(lua_State* L, {t} value)")
                                                                   for t in ("bool", "float", "double")))
            current(host.relative_to(ROOT).as_posix())
            current(Path(__file__).with_name("test_lua_camera_bindings.py").relative_to(ROOT).as_posix())
            for header in ("lua.h", "luaconf.h", "lauxlib.h"):
                current("vendor/lua/src/" + header)
            lua_dir = ROOT / "Bin/mods/deathmatch" / ("x64" if args.arch == "x64" else "")
            dll, lib = lua_dir / "lua5.1c.dll", lua_dir / "lua5.1c.lib"
            record["packagedLua"] = {"dll": str(dll), "sha256": sha(dll), "importLibrarySha256": sha(lib)}
            shutil.copy2(dll, work / dll.name)
            compiler = Path(env["VCTOOLSINSTALLDIR"]) / "bin/Hostx64" / args.arch / "cl.exe"
            commands = {"compile": [compiler, "/nologo", "/std:c++20", "/EHsc", "/MT", "/O2", "/DNOMINMAX", "/DLUA_BUILD_AS_DLL",
                                    "/I" + str(ROOT / "vendor/lua/src"), "/I" + str(work), host, "/Fe:sound.exe", "/link", lib],
                        "execute": [work / "sound.exe"]}
            for stage, command in commands.items():
                result = subprocess.run(command, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
                (output / (stage + ".log")).write_bytes(result.stdout)
                record[stage] = result.returncode
                if stage == "compile" and result.returncode:
                    raise RuntimeError("Compilation failed")
            report = result.stdout.decode(errors="replace").replace("\r\n", "\n")
            observed = re.findall(r"^(PASS|FAIL) ([\w-]+) type=(\w+) truth=(true|false)$", report, re.M)
            record["cases"] = {name: {"passed": status == "PASS", "type": lua_type, "truth": truth == "true"} for status, name, lua_type, truth in observed}
            assert sha(work / dll.name) == sha(dll) == record["packagedLua"]["sha256"]
            assert all(sha(ROOT / name) == h for name, h in record["currentSources"].items())
            record.update(sourcesUnchanged=True, success=not record["execute"] and len(record["cases"]) == 8 and all(c["passed"] for c in record["cases"].values()))
    except (OSError, ValueError, RuntimeError, AssertionError) as exc:
        record["failure"] = str(exc)
    receipt = output / "result.json"
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(receipt)
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
