/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise Deathmatch world sound dispatch with the MTA Lua runtime
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <game/CAudioEngine.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

struct CEvents
{
    std::vector<int> m_CancelledList;
    bool             m_bEventCancelled = false, m_bWasEventCancelled = false;
    void             PreEventPulse();
    void             PostEventPulse();
    void             CancelEvent(bool);
    bool             WasEventCancelled();
};
static CEvents events;
// The entity receiver exposes the callback's argument order and veto to a
// real Lua VM. The included Deathmatch methods below drive event production.
struct CLuaArguments
{
    std::vector<double> numbers;
    void                PushNumber(double number) { numbers.push_back(number); }
};

struct CClientEntity
{
    lua_State*  state;
    const char* name;
    unsigned    calls = 0;

    bool CallEvent(const char* event, const CLuaArguments& arguments, bool children)
    {
        if (!children)
            throw std::runtime_error("world sound disabled child event propagation");
        ++calls;
        events.PreEventPulse();
        lua_pushstring(state, name);
        lua_setglobal(state, "source");
        lua_getglobal(state, event);
        for (double value : arguments.numbers)
            lua_pushnumber(state, value);
        if (lua_pcall(state, static_cast<int>(arguments.numbers.size()), 0, 0) != 0)
            throw std::runtime_error(lua_tostring(state, -1));
        events.PostEventPulse();
        return !events.WasEventCancelled();
    }
};

struct CPools
{
    DWORD*         expected;
    CClientEntity* resolved = nullptr;
    unsigned       calls = 0;
    CClientEntity* GetClientEntity(DWORD* entity)
    {
        if (entity != expected)
            throw std::runtime_error("pool received a different entity identity");
        ++calls;
        return resolved;
    }
};
struct Game
{
    CPools*  pools = nullptr;
    unsigned calls = 0;
    CPools*  GetPools()
    {
        ++calls;
        return pools;
    }
};
struct Logger
{
    unsigned calls = 0;
    void     LogInformation(void*, const char*, const char*, unsigned group, unsigned index)
    {
        if (group != 0xffffffffu || index != 17)
            throw std::runtime_error("wrong debug sound identifiers");
        ++calls;
    }
};
static const char* const* GetLocalTimeString(bool, bool)
{
    static const char* time = "12:00";
    return &time;
}
struct CClientGame
{
    CClientEntity* root = nullptr;
    Logger*        m_pScriptDebugging = nullptr;
    bool           m_bShowSound = false;
    CClientEntity* GetRootEntity() { return root; }
    static bool    StaticWorldSoundHandler(const SWorldSoundEvent&);
    bool           WorldSoundHandler(const SWorldSoundEvent&);
};
static Game*        g_pGame;
static CClientGame* g_pClientGame;
#include "WorldSoundMethods.inl"

int main()
{
    try
    {
        std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(nullptr), &lua_close);
        if (!state)
            throw std::runtime_error("Lua initialization failed");
        lua_State* lua = state.get();
        luaL_openlibs(lua);
        lua_register(lua, "cancelEvent",
                     [](lua_State*)
                     {
                         events.CancelEvent(true);
                         return 0;
                     });
        const char* script = R"(
            function onClientWorldSound(...)
                assert(select('#', ...) == 5)
                local group, index, x, y, z = ...
                assert(group == 4294967295 and index == 17 and x == -7.25 and y == 2.5 and z == 600)
                assert(source == expectedSource)
                if veto then cancelEvent() end
            end
        )";
        if (luaL_dostring(lua, script))
            throw std::runtime_error(lua_tostring(lua, -1));

        DWORD            nativeIdentity = 0;
        auto*            native = reinterpret_cast<CEntitySAInterface*>(&nativeIdentity);
        SWorldSoundEvent event{0xffffffffu, 17, native, CVector(-7.25f, 2.5f, 600)};
        CClientEntity    root{lua, "root"}, selected{lua, "entity"};
        CPools           pools{&nativeIdentity};
        Game             game{&pools};
        CClientGame      client{&root};
        g_pGame = &game;
        g_pClientGame = &client;
        WorldSoundHandler* handler = &CClientGame::StaticWorldSoundHandler;
        unsigned           scenarios = 0;
        const auto         check = [&](CClientEntity* expected, bool veto, unsigned gameReads, unsigned poolReads)
        {
            const auto roots = root.calls, entities = selected.calls, games = game.calls, lookups = pools.calls;
            lua_pushstring(lua, expected ? expected->name : "unexpected");
            lua_setglobal(lua, "expectedSource");
            lua_pushboolean(lua, veto);
            lua_setglobal(lua, "veto");
            if (handler(event) != (!expected || !veto) || root.calls != roots + (expected == &root) || selected.calls != entities + (expected == &selected) ||
                game.calls != games + gameReads || pools.calls != lookups + poolReads)
                throw std::runtime_error("incorrect world sound delivery or veto");
            ++scenarios;
        };
        pools.resolved = &selected;
        Logger log;
        client.m_bShowSound = true;
        client.m_pScriptDebugging = &log;
        check(&selected, false, 1, 1);
        if (log.calls != 1)
            throw std::runtime_error("missing world sound diagnostic");
        client.m_bShowSound = false;
        check(&selected, true, 1, 1);
        event.pGameEntity = nullptr;
        check(&root, false, 0, 0);
        check(&root, true, 0, 0);
        event.pGameEntity = native;
        pools.resolved = nullptr;
        check(&root, true, 1, 1);
        game.pools = nullptr;
        check(&root, false, 1, 0);
        g_pGame = nullptr;
        check(&root, true, 0, 0);
        client.root = nullptr;
        check(nullptr, true, 0, 0);
        g_pGame = &game;
        game.pools = &pools;
        check(nullptr, true, 1, 1);
        pools.resolved = &selected;
        check(&selected, false, 1, 1);
        std::printf("Passed %u Deathmatch source, Lua arguments and veto scenarios\n", scenarios);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
