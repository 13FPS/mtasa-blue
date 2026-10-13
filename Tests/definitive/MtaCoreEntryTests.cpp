/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual Core startup rejection in an isolated host
 *****************************************************************************/

// Run the actual Core entry point in a named, non-game executable so the DE
// classifier is exercised while the exact-build gate must reject native access.
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{
    int  checks = 0;
    void Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    void NoGameModules()
    {
        for (const auto* name : {L"game_sa.dll", L"multiplayer_sa.dll", L"client.dll"})
            Check(GetModuleHandleW(name) == nullptr, "classic gameplay module must remain unloaded");
    }
    template <class Function>
    Function Resolve(HMODULE module, const char* name)
    {
        const auto address = GetProcAddress(module, name);
        Check(address != nullptr, "real Core export is required");
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
        Check(sizeof(void*) == 8 && argc == 2, "fixture requires x64 and its owned absolute directory");
        const auto root = std::filesystem::path(argv[1]);
        Check(root.is_absolute(), "fixture path must be explicit and absolute");
        const auto corePath = root / L"mta" / L"core.dll";
        NoGameModules();
        Check(GetModuleHandleW(L"game_de.dll") == nullptr, "backend starts unloaded");
        const auto core = LoadLibraryExW(corePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!core)
        {
            std::printf("{\"success\":false,\"stage\":\"LoadLibraryEx\",\"win32_error\":%lu}\n", GetLastError());
            return 2;
        }
        using SetDirectory = void (*)(const wchar_t*, std::size_t);
        using Initialize = int (*)();
        const auto         setMta = Resolve<SetDirectory>(core, "SetMTADirectory");
        const auto         setGta = Resolve<SetDirectory>(core, "SetGTADirectory");
        const auto         initialize = Resolve<Initialize>(core, "InitializeCore");
        std::array<int, 4> results{};
        results[0] = initialize();
        Check(results[0] == 3, "DE candidate must request MTA directory before construction");
        const auto mta = (root / L"mta").wstring();
        setMta(mta.c_str(), mta.size());
        results[1] = initialize();
        Check(results[1] == 4, "DE candidate must request GTA directory before construction");
        const auto gta = root.wstring();
        setGta(gta.c_str(), gta.size());
        for (std::size_t index = 2; index < results.size(); ++index)
        {
            results[index] = initialize();
            Check(results[index] == 5, "real incomplete adapter must reject construction and allow clean retry");
            Check(GetModuleHandleW(L"game_de.dll") == nullptr, "partial Core construction must release the backend DLL");
            NoGameModules();
        }
        Check(FreeLibrary(core) != FALSE, "real Core must unload normally");
        Check(GetModuleHandleW(L"core.dll") == nullptr && GetModuleHandleW(L"game_de.dll") == nullptr, "no fixture DLL remains after unload");
        NoGameModules();
        // Reaching this statement proves DE detach did not take legacy ExitProcess.
        std::printf(
            "{\"success\":true,\"checks\":%d,\"initialize_results\":[%d,%d,%d,%d],\"continued_after_free_library\":true,\"backend_unloaded_after_each_"
            "failure\":true,\"classic_modules_unloaded\":true}\n",
            checks, results[0], results[1], results[2], results[3]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Core entry fixture failed after %d checks: %s\n", checks, error.what());
        return 1;
    }
}
