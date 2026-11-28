// The runner compiles the shipping Lua callbacks and camera methods with
// the packaged Lua DLL. Only owner lookup, rendering and logging are controlled.
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <array>
#include <type_traits>
#include <filesystem>
#include <windows.h>
#include <SharedUtil.IntTypes.h>
#include <SharedUtil.Defines.h>
#include <game/CCamera.h>
#include "Color.inl"
#include <game/CHud.h>
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}

static unsigned int checks = 0;
static void         Check(bool ok, const char* message)
{
    ++checks;
    if (!ok)
    {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::exit(1);
    }
}
using SString = std::string;
struct Debug
{
    int  errors = 0;
    void LogWarning(lua_State*, const SString&) { std::abort(); }
    void LogCustom(lua_State*, const SString&) { ++errors; }
} debug;
struct CLuaFunctionDefs
{
    static inline Debug* m_pScriptDebugging = &debug;
};
class CScriptArgReader
{
public:
#include "Reader.inl"
    // Error text presentation is outside this fixture. Only count its delivery.
    SString    GetFullErrorMessage() { return m_bHasCustomMessage ? m_strCustomMessage : m_strErrorExpectedType; }
    bool       m_bError, m_bResolvedErrorGotArgumentTypeAndValue, m_bHasCustomMessage;
    int        m_iErrorIndex, m_iIndex, m_pPendingFunctionIndex;
    void*      m_pPendingFunctionOutValue;
    lua_State* m_luaVM;
    SString    m_strErrorExpectedType, m_strErrorCategory, m_strCustomMessage, m_strCustomWarning;
};

static std::string order;
struct Camera : CCamera
{
    float                        time{};
    int                          direction{-3};
    std::array<unsigned char, 3> color{};
    void                         SetFadeColor(unsigned char r, unsigned char g, unsigned char b) override
    {
        order += 'C';
        color = {r, g, b};
    }
    void Fade(float t, int dir) override
    {
        order += 'F';
        time = t;
        direction = dir;
    }
    bool     objects = false, vehicles = false;
    unsigned clipWrites = 0, clipReads = 0;
    void     SetCameraClip(bool objectClip, bool vehicleClip) override
    {
        ++clipWrites;
        objects = objectClip;
        vehicles = vehicleClip;
    }
    void GetCameraClip(bool& objectClip, bool& vehicleClip) override
    {
        ++clipReads;
        objectClip = objects;
        vehicleClip = vehicles;
    }
#include "Camera.inl"
} sdkCamera;
struct Hud : CHud
{
    bool shown = false;
    void SetComponentVisible(eHudComponent what, bool visible) override
    {
        Check(what == HUD_AREA_NAME, "area-name component");
        order += 'H';
        shown = visible;
    }
#include "Hud.inl"
} sdkHud;
struct CClientGame
{
    bool m_bInitiallyFadedOut = true, m_bHudAreaNameDisabled = false;
#include "ClientGame.inl"
} clientGame;
struct Game
{
    CHud* hud = &sdkHud;
    CHud* GetHud() { return hud; }
} game;
static CClientGame* g_pClientGame = &clientGame;
static Game*        g_pGame = &game;
class CClientCamera
{
public:
    CCamera* m_pCamera = &sdkCamera;
    void     FadeIn(float);
    void     FadeOut(float, unsigned char, unsigned char, unsigned char);
    bool     ApplyFade(bool, float, unsigned char, unsigned char, unsigned char);
    bool     SetCameraClip(bool, bool);
    void     GetCameraClip(bool&, bool&);
} clientCamera;
struct Manager
{
    CClientCamera* m_pCamera = &clientCamera;
#include "Manager.inl"
} manager;
class CStaticFunctionDefinitions
{
public:
    static inline Manager* m_pManager = &manager;
    static bool            FadeCamera(bool, float, unsigned char, unsigned char, unsigned char);
};
class CLuaCameraDefs : public CLuaFunctionDefs
{
public:
    static inline Manager* m_pManager = &manager;
    static int             FadeCamera(lua_State*);
    static int             SetCameraClip(lua_State*);
    static int             GetCameraClip(lua_State*);
};
#include "Binding.inl"

static unsigned scriptCalls = 0;
static void     Run(lua_State* lua, const char* text)
{
    ++scriptCalls;
    Check(lua_gettop(lua) == 0, "clean Lua stack");
    Check(luaL_loadstring(lua, text) == 0, "packaged Lua compiles script");
    int status = lua_pcall(lua, 0, LUA_MULTRET, 0);
    if (status != 0)
        std::fprintf(stderr, "Lua: %s\n", lua_tostring(lua, -1));
    Check(status == 0, "packaged Lua invokes registered callback");
}
static void Call(lua_State* lua, const char* text, bool expected)
{
    Run(lua, text);
    Check(lua_gettop(lua) == 1 && lua_type(lua, -1) == LUA_TBOOLEAN, "one Lua boolean result");
    Check((lua_toboolean(lua, -1) != 0) == expected, text);
    lua_pop(lua, 1);
}

static void CheckClip(lua_State* lua, bool expectedObjects, bool expectedVehicles)
{
    Run(lua, "return getCameraClip()");
    Check(lua_gettop(lua) == 2 && lua_type(lua, 1) == LUA_TBOOLEAN && lua_type(lua, 2) == LUA_TBOOLEAN, "two Lua boolean results");
    Check((lua_toboolean(lua, 1) != 0) == expectedObjects && (lua_toboolean(lua, 2) != 0) == expectedVehicles, "object and vehicle clipping");
    lua_pop(lua, 2);
}
static void Register(lua_State* lua, const char* name, lua_CFunction function)
{
    lua_pushstring(lua, name);
    lua_pushcclosure(lua, function, 1);
    lua_setglobal(lua, name);
}

int main()
{
    std::array<wchar_t, 32768> loaded{};
    const DWORD                length = GetModuleFileNameW(GetModuleHandleW(L"lua5.1c.dll"), loaded.data(), static_cast<DWORD>(loaded.size()));
    Check(length > 0 && length < loaded.size() && std::filesystem::equivalent(loaded.data(), L"lua5.1c.dll"), "load the exact staged MTA Lua DLL");
    lua_State* lua = luaL_newstate(nullptr);
    Check(lua != nullptr, "create MTA Lua state");
    Register(lua, "fadeCamera", CLuaCameraDefs::FadeCamera);
    Register(lua, "setCameraClip", CLuaCameraDefs::SetCameraClip);
    Register(lua, "getCameraClip", CLuaCameraDefs::GetCameraClip);
    Call(lua, "return fadeCamera(true)", true);
    Check(order == "FH" && sdkCamera.time == 1.f && sdkCamera.direction == FADE_IN && !clientGame.m_bInitiallyFadedOut && sdkHud.shown,
          "default fade in then HUD show");
    order.clear();
    clientGame.m_bInitiallyFadedOut = true;
    Call(lua, "return fadeCamera(false, 2.5, 17, 31, 255)", true);
    Check(order == "CFH" && sdkCamera.time == 2.5f && sdkCamera.direction == FADE_OUT && sdkCamera.color == std::array<unsigned char, 3>{17, 31, 255} &&
              !clientGame.m_bInitiallyFadedOut && !sdkHud.shown,
          "explicit fade out changes color before fade and hides the area name");
    order.clear();
    clientGame.m_bHudAreaNameDisabled = true;
    Call(lua, "return fadeCamera(true, '0.5')", true);
    Check(order == "FH" && sdkCamera.time == 0.5f && !sdkHud.shown, "honor disabled area name and numeric strings");
    order.clear();
    clientGame.m_bInitiallyFadedOut = true;
    sdkHud.shown = true;
    clientCamera.m_pCamera = nullptr;
    Call(lua, "return fadeCamera(true, 0)", false);
    Call(lua, "return fadeCamera(false, nil, 33, 44, 55)", false);
    Check(order.empty() && clientGame.m_bInitiallyFadedOut && sdkHud.shown && sdkCamera.color == std::array<unsigned char, 3>{17, 31, 255},
          "missing SDK camera preserves initial fade, existing HUD and color");
    clientCamera.m_pCamera = &sdkCamera;
    manager.m_pCamera = nullptr;
    Call(lua, "return fadeCamera(true)", false);
    manager.m_pCamera = &clientCamera;
    CStaticFunctionDefinitions::m_pManager = nullptr;
    Call(lua, "return fadeCamera(false)", false);
    CStaticFunctionDefinitions::m_pManager = &manager;
    g_pClientGame = nullptr;
    Call(lua, "return fadeCamera(true)", false);
    g_pClientGame = &clientGame;
    Check(order.empty() && clientGame.m_bInitiallyFadedOut && sdkHud.shown, "unavailable owners preserve state");
    Call(lua, "return fadeCamera('no')", false);
    Call(lua, "return fadeCamera(false, 'bad')", false);
    Call(lua, "return fadeCamera(false, 1, -1)", false);
    Check(debug.errors == 3 && order.empty() && clientGame.m_bInitiallyFadedOut && sdkHud.shown, "invalid Lua arguments produce errors without mutations");
    game.hud = nullptr;
    Call(lua, "return fadeCamera(false)", true);
    Check(order == "CF" && !clientGame.m_bInitiallyFadedOut && sdkCamera.time == 1.f && sdkCamera.color == std::array<unsigned char, 3>{0, 0, 0},
          "camera still accepts a fade with no HUD and default color");
    Call(lua, "return setCameraClip()", true);
    CheckClip(lua, true, true);
    Call(lua, "return setCameraClip(false, true)", true);
    CheckClip(lua, false, true);
    Call(lua, "return setCameraClip(nil, false)", true);
    CheckClip(lua, true, false);
    Call(lua, "return setCameraClip(false)", true);
    CheckClip(lua, false, true);
    clientCamera.m_pCamera = nullptr;
    Call(lua, "return setCameraClip(false,false)", false);
    CheckClip(lua, true, true);
    clientCamera.m_pCamera = &sdkCamera;
    Call(lua, "return setCameraClip(0, false)", false);
    Call(lua, "return setCameraClip(true, 'false')", false);
    CheckClip(lua, false, true);
    Check(debug.errors == 5 && sdkCamera.clipWrites == 4 && sdkCamera.clipReads == 5, "bad clip arguments leave SDK state intact");
    manager.m_pCamera = nullptr;
    Call(lua, "return setCameraClip()", false);
    CheckClip(lua, true, true);
    manager.m_pCamera = &clientCamera;
    CLuaCameraDefs::m_pManager = nullptr;
    Call(lua, "return setCameraClip(false,true)", false);
    CheckClip(lua, true, true);
    CLuaCameraDefs::m_pManager = &manager;
    Check(sdkCamera.clipWrites == 4 && sdkCamera.clipReads == 5 && !sdkCamera.objects && sdkCamera.vehicles,
          "absent SDK camera, client camera or manager never changes clipping");
    lua_close(lua);
    std::printf("passed %u checks across %u Lua scripts, %zu-bit\n", checks, scriptCalls, sizeof(void*) * 8);
}
