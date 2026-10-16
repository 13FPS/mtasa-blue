/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the normal MTA client Lua DLL without Core or GTA
 *****************************************************************************/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace
{
    unsigned checks = 0;
    unsigned cases = 0;

    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }

    void Case(const char* name)
    {
        ++cases;
        std::printf("LUA_X64_CASE %s checks=%u\n", name, checks);
    }

    struct Allocator
    {
        std::size_t liveBytes = 0;
        std::size_t liveBlocks = 0;
        std::size_t allocations = 0;
        bool        accountingFailed = false;
    };

    void* Allocate(void* context, void* pointer, std::size_t oldSize, std::size_t newSize)
    {
        auto& state = *static_cast<Allocator*>(context);
        if (!newSize)
        {
            if (pointer)
            {
                if (!state.liveBlocks || state.liveBytes < oldSize)
                    state.accountingFailed = true;
                else
                {
                    --state.liveBlocks;
                    state.liveBytes -= oldSize;
                }
            }
            std::free(pointer);
            return nullptr;
        }
        // Lua allocates and frees through this same host callback. No allocation
        // is transferred between the DLL's CRT and the host's CRT.
        auto* next = std::realloc(pointer, newSize);
        if (!next)
            return nullptr;
        if (!pointer)
        {
            ++state.liveBlocks;
            oldSize = 0;
        }
        if (state.liveBytes < oldSize)
            state.accountingFailed = true;
        else
            state.liveBytes = state.liveBytes - oldSize + newSize;
        ++state.allocations;
        return next;
    }

    struct Owner
    {
        std::uint64_t marker = 0x1234567887654321;
        unsigned      preAllowed = 0, postAllowed = 0, preBlocked = 0, postBlocked = 0;
        unsigned      allowedBodies = 0, blockedBodies = 0, finalizers = 0, undumpCalls = 0;
        int           expectedResults = 0;
        bool          deny = true, undumpAllowed = true, callbackFailed = false;
    };

    Owner* current = nullptr;

    bool Owns(lua_State* state)
    {
        return current && lua_getmtasaowner(state) == current;
    }

    int Allowed(lua_State* state)
    {
        if (!Owns(state))
            current->callbackFailed = true;
        ++current->allowedBodies;
        current->expectedResults = lua_ncallresult(state);
        lua_pushlightuserdata(state, current);
        lua_pushnumber(state, 42);
        return 2;
    }

    int Blocked(lua_State*)
    {
        ++current->blockedBodies;
        return 0;
    }

    int Before(lua_CFunction function, lua_State* state)
    {
        if (!Owns(state))
            current->callbackFailed = true;
        if (function == &Allowed)
            ++current->preAllowed;
        if (function == &Blocked)
        {
            ++current->preBlocked;
            return !current->deny;
        }
        return 1;
    }

    void After(lua_CFunction function, lua_State* state)
    {
        if (!Owns(state))
            current->callbackFailed = true;
        if (function == &Allowed)
        {
            ++current->postAllowed;
            if (lua_touserdata(state, -2) != current || lua_tonumber(state, -1) != 42)
                current->callbackFailed = true;
        }
        if (function == &Blocked)
            ++current->postBlocked;
    }

    int Undump(const char* bytes, std::size_t size)
    {
        ++current->undumpCalls;
        if (!bytes || !size)
            current->callbackFailed = true;
        return current->undumpAllowed;
    }

    struct Payload
    {
        std::uint64_t marker;
    };

    int Finalize(lua_State* state)
    {
        auto* payload = static_cast<Payload*>(lua_touserdata(state, 1));
        if (!Owns(state) || !payload || payload->marker != current->marker)
            current->callbackFailed = true;
        else
            payload->marker = 0;
        ++current->finalizers;
        return 0;
    }

    void RunSource(lua_State* state, const char* script, int results)
    {
        Check(luaL_loadstring(state, script) == 0, "normal Lua source compiler failed");
        Check(lua_pcall(state, 0, results, 0) == 0, "normal Lua VM failed");
    }

    void CheckOwner(lua_State* state, Owner& owner)
    {
        // A real high address catches a truncated owner pointer. The token is
        // opaque to Lua, matching the ordinary CLuaMain ownership contract.
        Check(reinterpret_cast<std::uintptr_t>(&owner) > UINT32_MAX, "owner token must exercise upper pointer bits");
        Check(lua_getmtasaowner(state) == &owner, "main Lua owner pointer was truncated");
        auto* thread = lua_newthread(state);
        Check(thread != nullptr && lua_getmtasaowner(thread) == &owner, "coroutine owner differs from main state");
        Check(lua_getmainstate(thread) == state, "MTA coroutine main-state lookup differs");
        lua_pop(state, 1);
        lua_pushlightuserdata(state, &owner);
        Check(lua_touserdata(state, -1) == &owner, "light userdata lost pointer bits");
        lua_pop(state, 1);
        Case("owner_and_coroutine");
    }

    void CheckSourceAndHooks(lua_State* state, Owner& owner)
    {
        RunSource(state, "local t={} for i=1,1000 do t[i]=i*i end return t[1000],#t,string.reverse('mta')", 3);
        const auto* reversed = lua_tostring(state, -1);
        Check(lua_tonumber(state, -3) == 1000000 && lua_tointeger(state, -2) == 1000 && reversed && std::strcmp(reversed, "atm") == 0,
              "source VM/table/string libraries produced wrong results");
        lua_settop(state, 0);
        lua_pushcfunction(state, &Allowed);
        lua_setglobal(state, "allowed");
        lua_pushcfunction(state, &Blocked);
        lua_setglobal(state, "blocked");
        lua_registerPreCallHook(&Before);
        lua_registerPostCallHook(&After);
        RunSource(state, "local p,n=allowed(); local denied=blocked(); return p,n,denied", 3);
        Check(lua_touserdata(state, -3) == &owner && lua_tonumber(state, -2) == 42 && lua_isnil(state, -1), "MTA allowed/denied call return values differ");
        Check(owner.preAllowed == 1 && owner.postAllowed == 1 && owner.allowedBodies == 1 && owner.expectedResults == 2,
              "MTA allowed C-call hooks or expected-result count differ");
        Check(owner.preBlocked == 1 && !owner.postBlocked && !owner.blockedBodies, "denied C function ran or received post hook");
        lua_settop(state, 0);
        owner.deny = false;
        RunSource(state, "return blocked()", 0);
        Check(owner.preBlocked == 2 && owner.postBlocked == 1 && owner.blockedBodies == 1, "legitimate call after veto did not work");
        lua_registerPreCallHook(nullptr);
        lua_registerPostCallHook(nullptr);
        lua_pushcfunction(state, &Allowed);
        Check(lua_pcall(state, 0, 2, 0) == 0, "unhooked C call failed");
        Check(owner.preAllowed == 1 && owner.postAllowed == 1 && owner.allowedBodies == 2, "unregistered hooks were still called");
        lua_settop(state, 0);
        Check(!owner.callbackFailed, "hook callback owner/stack checks failed");
        Check(luaL_loadstring(state, "local =") == LUA_ERRSYNTAX, "source syntax error was not protected");
        lua_pop(state, 1);
        RunSource(state, "return 6*7", 1);
        Check(lua_tonumber(state, -1) == 42, "VM did not recover after syntax error");
        lua_pop(state, 1);
        Case("source_vm_and_mta_call_hooks");
    }

    void CheckGarbageCollection(lua_State* state, Owner& owner)
    {
        luaL_newmetatable(state, "MtaX64GcProbe");
        lua_pushcfunction(state, &Finalize);
        lua_setfield(state, -2, "__gc");
        lua_pop(state, 1);
        for (unsigned index = 0; index != 129; ++index)
        {
            auto* payload = static_cast<Payload*>(lua_newuserdata(state, sizeof(Payload)));
            payload->marker = owner.marker;
            luaL_getmetatable(state, "MtaX64GcProbe");
            lua_setmetatable(state, -2);
            if (index == 128)
                luaL_ref(state, LUA_REGISTRYINDEX);  // Remains rooted until close.
            else
                lua_pop(state, 1);
        }
        lua_gc(state, LUA_GCCOLLECT, 0);
        lua_gc(state, LUA_GCCOLLECT, 0);
        Check(owner.finalizers == 128, "unreachable userdata did not finalize exactly once while rooted userdata survived");
        Check(!owner.callbackFailed, "GC finalizer owner or payload was corrupted");
        Case("real_userdata_gc");
    }

    void Append32(std::vector<char>& bytes, std::uint32_t value)
    {
        for (unsigned shift = 0; shift != 32; shift += 8)
            bytes.push_back(static_cast<char>(value >> shift));
    }

    void AppendString32(std::vector<char>& bytes, const char* value)
    {
        const auto size = std::strlen(value) + 1;
        Append32(bytes, static_cast<std::uint32_t>(size));
        bytes.insert(bytes.end(), value, value + size);
    }

    std::vector<char> CompatibilityChunk()
    {
        // A genuine Lua 5.1 little-endian function returning a string and a
        // number. Explicit four-byte string lengths exercise MTA's retained
        // x86 chunk contract rather than dumping native x64 size_t values.
        std::vector<char> bytes{0x1b, 'L', 'u', 'a', 0x51, 0, 1, 4, 4, 4, 8, 0};
        AppendString32(bytes, "@mta-x86-compatible");
        Append32(bytes, 0);                       // linedefined
        Append32(bytes, 0);                       // lastlinedefined
        bytes.insert(bytes.end(), {0, 0, 2, 2});  // nups, parameters, vararg, maxstack
        Append32(bytes, 3);
        Append32(bytes, 1);                         // LOADK R0 K0 (Lua5.1 opcode1)
        Append32(bytes, 1 | (1 << 6) | (1 << 14));  // LOADK R1 K1
        Append32(bytes, 30 | (3 << 23));            // RETURN R0,2 results (opcode30)
        Append32(bytes, 2);
        bytes.push_back(LUA_TSTRING);
        AppendString32(bytes, "mta-x64");
        bytes.push_back(LUA_TNUMBER);
        const double number = 42;
        const auto*  raw = reinterpret_cast<const char*>(&number);
        bytes.insert(bytes.end(), raw, raw + sizeof(number));
        Append32(bytes, 0);  // nested prototypes
        Append32(bytes, 3);  // source lines
        for (unsigned index = 0; index != 3; ++index)
            Append32(bytes, 1);
        Append32(bytes, 0);  // local variables
        Append32(bytes, 0);  // upvalue names
        return bytes;
    }

    int LoadChunk(lua_State* state, const std::vector<char>& bytes)
    {
        return luaL_loadbuffer(state, bytes.data(), bytes.size(), "@compatibility-fixture");
    }

    void CheckBinaryCompatibility(lua_State* state, Owner& owner, const std::filesystem::path& fixture)
    {
        const auto    bytes = CompatibilityChunk();
        std::ofstream file(fixture, std::ios::binary | std::ios::trunc);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        file.close();
        Check(!file.fail(), "could not preserve exact compatibility chunk");
        lua_registerUndumpHook(&Undump);
        Check(LoadChunk(state, bytes) == 0, "x64 Lua rejected actual four-byte-size_t chunk");
        Check(lua_pcall(state, 0, 2, 0) == 0, "compatible bytecode did not execute");
        const auto* text = lua_tostring(state, -2);
        Check(text && std::strcmp(text, "mta-x64") == 0 && lua_tonumber(state, -1) == 42, "compatible string/number constants or instructions differ");
        lua_settop(state, 0);
        Check(owner.undumpCalls == 1, "MTA undump hook did not observe binary load");
        owner.undumpAllowed = false;
        Check(LoadChunk(state, bytes) == LUA_ERRSYNTAX, "MTA undump veto was bypassed");
        Check(std::strstr(lua_tostring(state, -1), "unexpected") != nullptr, "undump veto diagnostic differs");
        lua_pop(state, 1);
        owner.undumpAllowed = true;
        auto badHeader = bytes;
        badHeader[8] = 8;
        Check(LoadChunk(state, badHeader) == LUA_ERRSYNTAX, "native-eight-byte chunk header bypassed retained MTA format");
        lua_pop(state, 1);
        auto truncated = bytes;
        truncated.pop_back();
        Check(LoadChunk(state, truncated) == LUA_ERRSYNTAX, "truncated chunk was accepted");
        lua_pop(state, 1);
        lua_registerUndumpHook(nullptr);
        const auto calls = owner.undumpCalls;
        Check(LoadChunk(state, bytes) == 0 && lua_pcall(state, 0, 2, 0) == 0, "valid chunk failed after parser/hook refusal");
        Check(owner.undumpCalls == calls && !owner.callbackFailed, "unregistered undump hook or callback fault survived");
        lua_settop(state, 0);
        Case("x86_chunk_compatibility_and_rejection");
    }

    void CheckIsolatedDll()
    {
        wchar_t    modulePath[32768]{}, executablePath[32768]{};
        const auto module = GetModuleHandleW(L"lua5.1c.dll");
        Check(module && GetModuleFileNameW(module, modulePath, 32768), "normal client Lua DLL is not loaded");
        Check(GetModuleFileNameW(nullptr, executablePath, 32768) != 0, "owned executable path unavailable");
        Check(std::filesystem::path(modulePath).parent_path() == std::filesystem::path(executablePath).parent_path(),
              "Lua DLL loaded outside owned staging directory");
        Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"netc.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"game_sa.dll") &&
                  !GetModuleHandleW(L"multiplayer_sa.dll") && !GetModuleHandleW(L"client.dll"),
              "dependency test loaded Core, network or game modules");
        Case("isolated_normal_dll");
    }
}

int wmain(int argc, wchar_t** argv)
{
    static_assert(sizeof(void*) == 8 && sizeof(std::size_t) == 8 && sizeof(lua_Number) == 8);
    Allocator allocator;
    Owner     owner;
    current = &owner;
    lua_State* state = nullptr;
    try
    {
        Check(argc == 2, "pass a workspace path for the preserved compatibility chunk");
        CheckIsolatedDll();
        state = lua_newstate(&Allocate, &allocator, &owner);
        Check(state != nullptr, "normal client Lua state allocation failed");
        luaL_openlibs(state);
        CheckOwner(state, owner);
        CheckSourceAndHooks(state, owner);
        CheckGarbageCollection(state, owner);
        CheckBinaryCompatibility(state, owner, argv[1]);
        lua_close(state);
        state = nullptr;
        Check(owner.finalizers == 129 && !owner.callbackFailed, "rooted userdata was not finalized correctly by close");
        Check(!allocator.accountingFailed && !allocator.liveBlocks && !allocator.liveBytes && allocator.allocations > 100,
              "normal Lua state allocator retained resources after close");
        Case("close_finalizers_and_allocator_retirement");
        CheckIsolatedDll();
        std::printf(
            "{\"success\":true,\"checks\":%u,\"cases\":%u,\"pointerBytes\":8,\"chunkSizeTBytes\":4,"
            "\"finalizers\":129,\"allocations\":%zu,\"liveBlocks\":0,\"liveBytes\":0,\"gameplayReady\":false,\"fullMtaReady\":false}\n",
            checks, cases, allocator.allocations);
        return 0;
    }
    catch (const std::exception& error)
    {
        lua_registerPreCallHook(nullptr);
        lua_registerPostCallHook(nullptr);
        lua_registerUndumpHook(nullptr);
        if (state)
            lua_close(state);
        std::fprintf(stderr, "Lua x64 failure after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
