/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Host the actual configured-input Core fixture outside any game
 *****************************************************************************/

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace
{
    int  checks = 0;
    void Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    void NoGameplayModules()
    {
        for (const auto* name : {L"game_sa.dll", L"multiplayer_sa.dll", L"client.dll", L"cleo_redux64.asi", L"d3dx9_43.dll", L"d3d9.dll"})
            Check(GetModuleHandleW(name) == nullptr, "configured-input fixture cannot initialize a game, CLEO or legacy renderer");
    }
    template <class Function>
    Function Resolve(HMODULE module, const char* name)
    {
        const auto address = GetProcAddress(module, name);
        Check(address != nullptr, "test-only Core export is required");
        Function result = nullptr;
        static_assert(sizeof(result) == sizeof(address));
        std::memcpy(&result, &address, sizeof(result));
        return result;
    }
}
int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try
    {
        Check(sizeof(void*) == 8 && argc == 2, "fixture requires x64 and one explicit owned DLL path");
        const auto path = std::filesystem::path(argv[1]);
        Check(path.is_absolute(), "test DLL path must be explicit and absolute");
        NoGameplayModules();
        const auto module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(module != nullptr, "test-only Core DLL with normal objects/dependencies must load");
        NoGameplayModules();
        const auto    run = Resolve<int (*)(std::uint32_t*)>(module, "MTA_RunConfiguredInputTests");
        const auto    failure = Resolve<const char* (*)()>(module, "MTA_ConfiguredInputTestFailure");
        std::uint32_t bodyChecks = 0;
        if (run(&bodyChecks) != 0)
        {
            std::fprintf(stderr, "Configured-input body failed after %u checks: %s\n", bodyChecks, failure());
            return 1;
        }
        Check(bodyChecks > 100, "actual CKeyBinds and Game DE fixture must execute its complete behavioral checks");
        Check(GetModuleHandleW(L"game_de.dll") == nullptr, "actual consumer releases its backend before Core unload");
        Check(GetModuleHandleW(L"xmll.dll") == nullptr, "actual XML module releases its wrappers and singleton before Core unload");
        NoGameplayModules();
        Check(FreeLibrary(module) != FALSE, "test-only Core DLL releases normally");
        Check(GetModuleHandleW(path.filename().c_str()) == nullptr, "test DLL does not remain loaded");
        NoGameplayModules();
        std::printf(
            "{\"success\":true,\"host_checks\":%d,\"body_checks\":%u,\"actual_keybinds\":true,\"actual_game_adapter_module\":true,"
            "\"backend_unloaded\":true,\"continued_after_free_library\":true,\"xml_loaded\":true,\"native_input_written\":false,\"full_mta_boot_verified\":"
            "false}\n",
            checks, bodyChecks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Configured-input host failed after %d checks: %s (Win32 %lu)\n", checks, error.what(), GetLastError());
        return 1;
    }
}
