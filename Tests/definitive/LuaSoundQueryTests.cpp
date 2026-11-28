// The Lua script controls the call. Supply only the sound values at the edge
// of the audio subsystem and use the shipping binding and result marshalling.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <variant>
#include <windows.h>
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}

struct CClientSound
{
    bool   stream = false;
    double length = 0;
    float  bpm = 0;
    int    lengthReads = 0;
    bool   IsSoundStream() { return stream; }
    double GetBufferLength()
    {
        ++lengthReads;
        return length;
    }
    float GetSoundBPM() { return bpm; }
};

struct CLuaAudioDefs
{
    static std::variant<double, bool> GetSoundBufferLength(CClientSound*);
    static std::variant<float, bool>  GetSoundBPM(CClientSound*);
};
#include "SoundQueries.generated.inl"

namespace lua
{
#include "ScalarPush.generated.inl"
}
struct TypedResult
{
#include "PushResult.generated.inl"
};
template <auto Query>
static int Invoke(lua_State* vm)
{
    // The element manager supplies this pointer in the client. This test
    // supplies one known sound and exercises the typed return path only.
    if (lua_gettop(vm) != 1 || lua_type(vm, 1) != LUA_TLIGHTUSERDATA)
        return luaL_error(vm, "fixture requires a sound");
    return TypedResult{}.PushResult(vm, Query(static_cast<CClientSound*>(lua_touserdata(vm, 1))));
}

static bool Execute(lua_State* vm, const std::string& chunk)
{
    lua_settop(vm, 0);
    if (luaL_loadstring(vm, chunk.c_str()) == 0 && lua_pcall(vm, 0, LUA_MULTRET, 0) == 0 && lua_gettop(vm) == 1)
        return true;
    std::fprintf(stderr, "Lua script failed or returned an unexpected result count: %s\n", chunk.c_str());
    return false;
}

static int Check(lua_State* vm, const char* scenario, const std::string& call, bool expectNumber, double expected = 0)
{
    if (!Execute(vm, "return " + call))
        return 1;
    const int  actual = lua_type(vm, -1);
    const bool valueMatches = expectNumber ? (actual == LUA_TNUMBER && lua_tonumber(vm, -1) == expected) : (actual == LUA_TBOOLEAN && !lua_toboolean(vm, -1));
    if (!Execute(vm, "return not not " + call))
        return 1;
    // In Lua, a number is truthy even when its value is zero.
    const bool truthMatches = lua_type(vm, -1) == LUA_TBOOLEAN && (lua_toboolean(vm, -1) != 0) == expectNumber;
    std::printf("%s %s type=%s truth=%s\n", valueMatches && truthMatches ? "PASS" : "FAIL", scenario, lua_typename(vm, actual),
                lua_toboolean(vm, -1) ? "true" : "false");
    return valueMatches && truthMatches ? 0 : 1;
}

int main()
{
    wchar_t     loaded[32768]{};
    const DWORD size = GetModuleFileNameW(GetModuleHandleW(L"lua5.1c.dll"), loaded, 32768);
    if (!size || size >= 32768 || !std::filesystem::equivalent(loaded, L"lua5.1c.dll"))
        return 2;
    auto* vm = luaL_newstate(nullptr);
    if (!vm)
        return 2;
    CClientSound sound;
    lua_pushlightuserdata(vm, &sound);
    lua_setglobal(vm, "sound");
    lua_register(vm, "getSoundBPM", Invoke<CLuaAudioDefs::GetSoundBPM>);
    lua_register(vm, "getSoundBufferLength", Invoke<CLuaAudioDefs::GetSoundBufferLength>);

    const std::string buffer = "getSoundBufferLength(sound)";
    const std::string bpm = "getSoundBPM(sound)";
    sound.length = 65.75;
    int errors = Check(vm, "nonstream", buffer, false);
    if (sound.lengthReads)
        ++errors;
    sound.stream = true;
    errors += Check(vm, "positive-buffer", buffer, true, 65.75);
    sound.length = 0;
    errors += Check(vm, "empty-buffer", buffer, true, 0);
    if (sound.lengthReads != 4)
        ++errors;
    sound.bpm = 127.5f;
    errors += Check(vm, "positive-bpm", bpm, true, 127.5);
    sound.bpm = 0;
    errors += Check(vm, "unavailable-bpm", bpm, false);
    sound.bpm = -1;
    errors += Check(vm, "negative-bpm", bpm, false);
    sound.bpm = std::numeric_limits<float>::quiet_NaN();
    errors += Check(vm, "nan-bpm", bpm, false);
    sound.bpm = std::numeric_limits<float>::infinity();
    errors += Check(vm, "infinite-bpm", bpm, false);
    lua_close(vm);
    return errors ? 1 : 0;
}
