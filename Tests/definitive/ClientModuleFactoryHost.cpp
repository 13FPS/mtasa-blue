/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the Deathmatch client factory before Core starts a session
 *****************************************************************************/
#include "../../Client/core/StartupModule.h"
#include <SharedUtil.h>
#include <unordered_set>
#include <core/CClientBase.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* reason)
    {
        if (!condition)
            throw std::runtime_error(reason);
    }

    void CheckSessionModulesAbsent()
    {
        for (const auto* name : {L"core.dll", L"game_sa.dll", L"game_de.dll", L"multiplayer_sa.dll", L"netc.dll", L"net.dll", L"cgui.dll"})
            Check(GetModuleHandleW(name) == nullptr, "a session module was loaded");
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 2, "supply the staged Deathmatch client.dll");
        const std::filesystem::path path(argv[1]);
        Check(path.is_absolute() && path.filename() == L"client.dll", "expected an absolute client.dll path");
        Check(GetModuleHandleW(L"client.dll") == nullptr, "client was loaded before the factory test");
        Check(SetDllDirectoryW(L"") != FALSE, "could not exclude the current directory from DLL search");
        CheckSessionModulesAbsent();

        MtaStartup::Module module;
        Check(module.Load(path.wstring()), "could not load the client for this host architecture");
        using InitClientFn = CClientBase*(__cdecl*)();
        // GetProcAddress erases the signature. Core uses this exact cdecl type.
#pragma warning(suppress : 4191)
        const auto initClient = reinterpret_cast<InitClientFn>(GetProcAddress(module.Get(), "InitClient"));
        Check(initClient != nullptr, "InitClient export missing");
        CClientBase* client = initClient();
        Check(client != nullptr, "first factory call returned null");
        Check(initClient() == client, "second factory call returned a different client");

        // These frame callbacks accept an absent session.
        client->PreFrameExecutionHandler();
        client->PreHUDRenderExecutionHandler(false, false);
        client->PostFrameExecutionHandler();
        client->IdleHandler();
        CheckSessionModulesAbsent();

        // The producing DLL owns the singleton and destroys it on detach.
        Check(module.Unload(), "client DLL failed to unload");
        Check(GetModuleHandleW(L"client.dll") == nullptr, "client remained loaded after releasing the module");
        CheckSessionModulesAbsent();
        std::cout << "Two InitClient calls returned the same non-null client; four pre-session frame callbacks returned; client unloaded.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
