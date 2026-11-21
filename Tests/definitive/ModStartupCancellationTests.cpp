/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Run the Core mod manager through an initializer that disconnects.
 *****************************************************************************/
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

// The runner copies the production manager and SDK declarations here. Only
// its platform services are replaced so a real DLL is never unloaded while
// testing callback ordering.
class SString;
class CExceptionInformation;
class CCoreInterface
{
};
#include "core/CClientBase.h"

struct Scenario
{
    bool     load = true, cancel = false, reject = false, inInitializer = false, refreshing = true;
    unsigned initialized = 0, shutdown = 0, released = 0, pulse = 0, networkPulse = 0, chatLoad = 0, consoleActivate = 0;
};
static Scenario scene;

static void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// Only the module-start diagnostic paths use these string operations.
class SString : public std::string
{
public:
    using std::string::string;
    SString(const char* text, const char*, const char*) : std::string(text) {}
    bool        CompareI(const SString& other) const { return *this == other; }
    const char* operator*() const { return c_str(); }
};
static SString GetSystemDllDirectory()
{
    return "mta";
}
static SString CalcMTASAPath(const char* path)
{
    return path;
}
static void SetDllDirectory(const SString&)
{
}
static void AddReportLog(int, const SString&)
{
}

static CClientBase* __cdecl InitClient();
class CModuleLoader
{
public:
    bool IsOk() const { return loaded; }
    bool LoadModule(const SString&) { return loaded = scene.load; }
    void UnloadModule()
    {
        Check(loaded && !scene.inInitializer, "module released while its initializer is running");
        loaded = false;
        ++scene.released;
    }
    const SString& GetLastErrorMessage() const
    {
        static SString value;
        return value;
    }
    auto GetFunctionPointer(const char*) { return &InitClient; }

private:
    bool loaded = false;
};

class Services : public CCoreInterface
{
public:
    static Services& GetSingleton()
    {
        static Services core;
        return core;
    }
    static Services* GetSingletonPtr() { return &GetSingleton(); }
    Services*        GetFPSLimiter() { return this; }
    Services*        GetNetwork() { return this; }
    Services*        GetConsole() { return this; }
    Services*        GetChat() { return this; }
    Services*        GetCommands() { return this; }
    Services*        GetScreenGrabber() { return this; }
    Services*        GetDebugView() { return this; }
    Services*        GetGUI() { return this; }
    Services*        GetGame() { return this; }
    Services*        GetMultiplayer() { return this; }
    Services*        GetMainMenu() { return this; }
    int              GetServerBitStreamVersion() { return 0; }
    bool             IsConsoleVisible() { return true; }
    void             SetRefreshMsgQueueEnabled(bool enabled) { scene.refreshing = enabled; }
    void             OnModLoad() { ++scene.chatLoad; }
    void             ActivateInput() { ++scene.consoleActivate; }
    void             DoPulse() { ++scene.networkPulse; }
    void             OnModUnload() { Check(!scene.inInitializer, "Core cleared input during client initialization"); }
    void             OnFrameStart() {}
    void             DoReliablePulse() {}
    void             Printf(const char*, ...) {}
    void             DeleteAll() {}
    void             ClearScreenShotQueue() {}
    void             SetInputVisible(bool) {}
    void             Clear() {}
    void             SetChatBoxVisible(bool) {}
    void             SetVisible(bool, bool) {}
    void             SetVisible(bool) {}
    void             SetDebugViewVisible(bool) {}
    void             SetClientMessageProcessor(void*) {}
    void             SetExecuteHandler(void*) {}
    void             SetCursorAlpha(float, bool) {}
    void             Reset() {}
    void             SetEnabled(bool) {}
    void             SetConnected(bool) {}
    void             SetIsIngame(bool) {}
};
using CCore = Services;
using CLocalGUI = Services;
using CGraphics = Services;
using CMessageLoopHook = Services;
#define TIMING_GRAPH(...)
#define dassert assert
#include "CModManager.cpp"

class TestClient final : public CClientBase
{
public:
    int ClientInitialize(const char*, CCoreInterface*) override
    {
        ++scene.initialized;
        scene.inInitializer = true;
        if (scene.cancel)
            CModManager::GetSingleton().RequestUnload();
        Check(!CModManager::GetSingleton().GetClient() && CModManager::GetSingleton().IsLoaded(), "client must stay unpublished during initialization");
        Check(!scene.shutdown && !scene.released, "cancel unloaded the module on the initializer stack");
        scene.inInitializer = false;
        return scene.reject ? 1 : 0;
    }
    void ClientShutdown() override
    {
        Check(!scene.inInitializer && CModManager::GetSingleton().IsLoaded(), "shutdown must follow initialization and precede DLL release");
        ++scene.shutdown;
    }
    void PostFrameExecutionHandler() override { ++scene.pulse; }
    void PreFrameExecutionHandler() override {}
    void PreHUDRenderExecutionHandler(bool, bool) override {}
    void IdleHandler() override {}
    void RestreamModel(std::uint16_t) override {}
    bool WebsiteRequestResultHandler(const std::unordered_set<SString>&) override { return false; }
    bool ProcessCommand(const char*, size_t, const void*, size_t) override { return false; }
    bool HandleException(CExceptionInformation*) override { return false; }
    void GetPlayerNames(std::vector<SString>&) override {}
    void OnWindowFocusChange(bool) override {}
};
static CClientBase* __cdecl InitClient()
{
    static TestClient client;
    return &client;
}

int main()
{
    try
    {
        for (const bool immediate : {true, false})
        {
            scene = {};
            scene.cancel = true;
            CModManager manager;
            if (immediate)
            {
                Check(manager.Load("invalid"), "existing client loaded and initialized successfully");
                Check(!manager.GetClient() && !manager.IsLoaded(), "synchronous load lost its initializer's unload request");
            }
            else
                manager.RequestLoad("invalid");
            manager.DoPulsePostFrame();
            Check(!manager.GetClient() && !manager.IsLoaded(), "frame-start load lost its initializer's unload request");
            Check(scene.initialized == 1 && scene.shutdown == 1 && scene.released == 1 && scene.refreshing, "cancel must retire the initialized client once");
            Check(!scene.pulse && scene.networkPulse == 1 && !scene.chatLoad && !scene.consoleActivate, "cancelled client received a frame or activated input");
            manager.DoPulsePostFrame();
            Check(scene.shutdown == 1 && scene.released == 1, "cleanup repeated on a later frame");
        }
        scene = {};
        {
            CModManager manager;
            scene.load = false;
            Check(!manager.Load("normal") && !scene.initialized && !manager.GetClient(), "DLL load failure must not initialize a client");
            scene.load = true;
            scene.cancel = scene.reject = true;
            Check(!manager.Load("reject") && !manager.IsLoaded() && !scene.shutdown && scene.released == 1, "failed initialization must not call shutdown");
            scene = {};
            Check(manager.Load("normal") && manager.GetClient() && scene.chatLoad == 1 && scene.consoleActivate == 1,
                  "fresh normal startup must deliver input");
            manager.DoPulsePostFrame();
            Check(scene.pulse == 1 && !scene.networkPulse, "normal client missed its first frame");
            manager.RequestUnload();
            manager.DoPulsePostFrame();
            Check(!manager.IsLoaded() && scene.shutdown == 1 && scene.released == 1 && scene.networkPulse == 1, "normal deferred stop failed");
        }
        std::puts("Passed synchronous and queued init cancellation, first frame, failed load/init, retry and ordinary unload");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
