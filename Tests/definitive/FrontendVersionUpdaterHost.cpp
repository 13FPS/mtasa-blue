/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Load and unload the actual owned updater fixture outside any game
 *****************************************************************************/
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace
{
    std::uint32_t checks = 0;
    void          Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    void NoGameplayModules()
    {
        for (const auto* name : {L"game_sa.dll", L"multiplayer_sa.dll", L"game_de.dll", L"client.dll", L"cgui.dll", L"xmll.dll", L"cleo_redux64.asi",
                                 L"d3dx9_43.dll", L"d3d9.dll"})
            Check(GetModuleHandleW(name) == nullptr, "updater ownership fixture never initializes game, XML, GUI or legacy renderer");
    }
    template <typename Function>
    Function Resolve(HMODULE module, const char* name)
    {
        const auto address = GetProcAddress(module, name);
        Check(address != nullptr, "test-only updater entry point exists");
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
        Check(sizeof(void*) == 8 && argc == 2, "owned host requires x64 and one explicit fixture DLL");
        const std::filesystem::path path(argv[1]);
        Check(path.is_absolute(), "fixture DLL path is absolute");
        NoGameplayModules();
        const auto module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(module != nullptr, "load fixture relinked from actual normal Core objects");
        NoGameplayModules();
        const auto    run = Resolve<int (*)(std::uint32_t*)>(module, "MTA_RunFrontendVersionUpdaterTests");
        const auto    failure = Resolve<const char* (*)()>(module, "MTA_FrontendVersionUpdaterTestFailure");
        std::uint32_t bodyChecks = 0;
        if (run(&bodyChecks))
        {
            std::fprintf(stderr, "Actual frontend updater failed after %u checks: %s\n", bodyChecks, failure());
            return 1;
        }
        // The body has 24 explicit contract assertions; the host adds its own
        // module-isolation and unload assertions after successful completion.
        Check(bodyChecks == 24, "complete real-updater ownership and thread-rejection checks executed");
        NoGameplayModules();
        Check(FreeLibrary(module) != FALSE && !GetModuleHandleW(path.filename().c_str()), "owned updater leaves actual fixture DLL unloadable");
        NoGameplayModules();
        std::printf(
            "{\"success\":true,\"host_checks\":%u,\"body_checks\":%u,\"actual_updater\":true,\"continued_after_free_library\":true,"
            "\"updater_worker_started\":false,\"http_admitted\":false,\"full_mta_startup_verified\":false}\n",
            checks, bodyChecks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Frontend updater host failed after %u checks: %s (Win32 %lu)\n", checks, error.what(), GetLastError());
        return 1;
    }
}
