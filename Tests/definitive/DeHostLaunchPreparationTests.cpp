/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise host preparation without dispatching Steam or running GTA
 *****************************************************************************/

#include "../../Client/launch/DefinitivePreflight/Preflight.h"
#include "../../Client/launch/DefinitivePreflight/SteamInstall.h"
#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace
{
    unsigned int checks = 0;
    void         Check(bool valid)
    {
        ++checks;
        if (!valid)
            throw std::runtime_error("host_preparation_test_failed");
    }

    int ParserTests()
    {
        using namespace DefinitivePreflight::SteamInstall;
        const auto        library = std::filesystem::path(L"C:\\Steam library with spaces é");
        const auto        install = std::filesystem::path(L"GTA DE");
        const std::string libraries = R"("libraryfolders" { "0" { "path" "C:\\Steam library with spaces é" "apps" { "1547000" "42" } } })";
        const std::string app = R"("AppState" { "appid" "1547000" "StateFlags" "4" "installdir" "GTA DE" })";
        ValidateRegistration(library, install, libraries, app);
        Check(true);
        ValidateRegistration(L"c:\\STEAM library with spaces é", install, "// ignored app 1547000\n" + libraries, app);
        Check(true);
        const auto refuse = [&](const std::string& first, const std::string& second, const char* status)
        {
            try
            {
                ValidateRegistration(library, install, first, second);
            }
            catch (const std::runtime_error& error)
            {
                Check(std::strcmp(error.what(), status) == 0);
                return;
            }
            Check(false);
        };
        refuse(R"("libraryfolders" { "0" { "path" "C:\\other" "apps" { "1547000" "42" } } })", app, "steam_library_not_registered");
        refuse(R"("libraryfolders" { "0" { "path" "C:\\Steam library with spaces é" "apps" { "other" "1547000" } } })", app, "steam_library_not_registered");
        refuse(
            R"("libraryfolders" { "0" { "path" "C:\\Steam library with spaces é" "apps" { "1547000" "42" } } "1" { "path" "E:\\Steam" "apps" { "1547000" "42" } } })",
            app, "steam_library_not_registered");
        refuse(libraries, R"("AppState" { "appid" "1" "StateFlags" "4" "installdir" "GTA DE" })", "steam_app_install_mismatch");
        refuse(libraries, R"("AppState" { "appid" "1547000" "StateFlags" "6" "installdir" "GTA DE" })", "steam_app_install_mismatch");
        refuse(libraries, R"("AppState" { "appid" "1547000" "StateFlags" "4" "installdir" "other" })", "steam_app_install_mismatch");
        refuse(libraries, R"("AppState" { "appid" "1" "appid" "1547000" "StateFlags" "4" "installdir" "GTA DE" })", "steam_manifest_invalid");
        refuse(libraries + " {}", app, "steam_manifest_invalid");
        refuse(libraries.substr(0, libraries.size() - 2), app, "steam_manifest_invalid");
        refuse(R"("libraryfolders" { "0" { "path" "C:\\Steam library with spaces é" "apps" { "1547000" { "nested" "42" } } } })", app,
               "steam_manifest_invalid");
        refuse(R"("libraryfolders" { "0" { "path" "relative" "apps" { "1547000" "42" } } })", app, "steam_path_invalid");
        refuse(R"("libraryfolders" { "0" { "path" "C:\invalid" "apps" { "1547000" "42" } } })", app, "steam_manifest_invalid");
        Check(std::strcmp(DefinitivePreflight::SteamLaunchUri, "steam://run/1547000") == 0);
        std::printf("{\"success\":true,\"parserChecks\":%u,\"hostLaunchRequested\":false,\"gameLaunched\":false}\n", checks);
        return 0;
    }

    int InstalledDiscoveryTests()
    {
        using namespace DefinitivePreflight::SteamInstall;
        const auto        library = std::filesystem::path(L"C:\\Steam library with spaces é");
        const std::string libraries = R"("libraryfolders" { "0" { "path" "C:\\Steam library with spaces é" "apps" { "1547000" "42" } } })";
        const std::string app = R"("AppState" { "appid" "1547000" "StateFlags" "4" "installdir" "GTA DE" })";
        Check(SamePath(FindRegisteredLibrary(libraries), library));
        Check(SamePath(FindRegisteredLibrary("// unrelated 1547000 comment\n" + libraries), library));
        const auto expected = library / L"steamapps" / L"common" / L"GTA DE" / L"Gameface" / L"Binaries" / L"Win64";
        Check(SamePath(ResolveGameDirectory(library, app), expected));
        Check(SamePath(ResolveGameDirectory(library, R"("AppState" { "appid" "1547000" "StateFlags" "4" "installdir" "GTA DE é" })"),
                       library / L"steamapps" / L"common" / L"GTA DE é" / L"Gameface" / L"Binaries" / L"Win64"));
        const auto refuses = [&](const auto& operation, const char* status)
        {
            try
            {
                operation();
            }
            catch (const std::runtime_error& error)
            {
                Check(std::strcmp(error.what(), status) == 0);
                return;
            }
            Check(false);
        };
        const auto refuseLibrary = [&](std::string_view text, const char* status) { refuses([&] { FindRegisteredLibrary(text); }, status); };
        refuseLibrary(R"("libraryfolders" { "0" { "path" "C:\\Steam" "apps" { "1" "1547000" } } })", "steam_library_not_registered");
        refuseLibrary(R"("libraryfolders" { "0" { "path" "C:\\Steam" "apps" { "1547000" "42" } } "1" { "path" "E:\\Steam" "apps" { "1547000" "42" } } })",
                      "steam_library_not_registered");
        refuseLibrary(R"("libraryfolders" { "0" { "path" "C:\\Steam" "apps" { "1547000" "42" } } "1" { "path" "c:\\STEAM" "apps" { "1547000" "42" } } })",
                      "steam_library_not_registered");
        refuseLibrary(R"("libraryfolders" { "0" { "path" "C:\\Steam" "apps" { "1547000" { "nested" "42" } } } })", "steam_manifest_invalid");
        refuseLibrary(libraries + " {}", "steam_manifest_invalid");
        refuseLibrary(R"("libraryfolders" { "0" { "path" "relative" "apps" { "1547000" "42" } } })", "steam_path_invalid");
        const auto refuseApp = [&](std::string_view text, const char* status) { refuses([&] { ResolveGameDirectory(library, text); }, status); };
        refuseApp(R"("AppState" { "appid" "1" "StateFlags" "4" "installdir" "GTA DE" })", "steam_app_install_mismatch");
        refuseApp(R"("AppState" { "appid" "1547000" "StateFlags" "6" "installdir" "GTA DE" })", "steam_app_install_mismatch");
        refuseApp(R"("AppState" { "appid" "1" "appid" "1547000" "StateFlags" "4" "installdir" "GTA DE" })", "steam_manifest_invalid");
        // Steam's install name is one directory component. A syntactically
        // absolute library must not turn this no-argument route into a path
        // selector or escape the exact Gameface/Binaries/Win64 game layout.
        for (const auto name : {"", R"(..)", R"(.)", R"(C:\\other)", R"(/other)", R"(GTA/other)", R"(GTA\\other)", R"(GTA:other)", R"(GTA.)", R"(GTA )"})
        {
            const auto invalid = std::string(R"("AppState" { "appid" "1547000" "StateFlags" "4" "installdir" ")") + name + R"(" })";
            refuseApp(invalid, "steam_install_name_invalid");
        }
        refuses([&] { ResolveGameDirectory(L"relative", app); }, "steam_path_invalid");
        refuses([&] { ResolveGameDirectory(library / L"child" / L"..", app); }, "steam_path_invalid");
        std::printf("{\"success\":true,\"discoveryChecks\":%u,\"hostLaunchRequested\":false,\"gameLaunched\":false,\"registryWrites\":false}\n", checks);
        return 0;
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc == 2 && std::wstring_view(argv[1]) == L"--steam-parser-tests")
            return ParserTests();
        if (argc == 2 && std::wstring_view(argv[1]) == L"--installed-steam-discovery-tests")
            return InstalledDiscoveryTests();
        if (argc == 2 && std::wstring_view(argv[1]) == L"--hold")
        {
            Sleep(30000);
            return 0;
        }
        std::array<wchar_t, 32768> executable{};
        const auto                 length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        Check(length && length < executable.size());
        const auto path = std::filesystem::path(executable.data()).parent_path() / L"mta" / L"de_preflight.dll";
        const auto module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(module != nullptr);
        // The positive installed route calls only the actual preparation
        // export. This owned host contains no Steam shell dispatch path.
        const bool installed = argc == 2 && std::wstring_view(argv[1]) == L"--de-launch-installed";
        const auto prepare = reinterpret_cast<DefinitivePreflight::PrepareHostLaunchV1>(
            GetProcAddress(module, installed ? "DE_PrepareInstalledHostLaunchV1" : "DE_PrepareHostLaunchV1"));
        Check(prepare != nullptr);
        Check(prepare(nullptr) == 4);
        DefinitivePreflight::HostLaunchResultV1 invalid;
        invalid.version = 2;
        Check(prepare(&invalid) == 4);
        invalid.version = 1;
        invalid.size = sizeof(invalid) - 1;
        Check(prepare(&invalid) == 4);
        DefinitivePreflight::HostLaunchResultV1 result;
        const auto                              code = prepare(&result);
        Check(result.size == sizeof(result) && result.version == 1 && result.status[sizeof(result.status) - 1] == '\0');
        Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"xmll.dll"));
        FreeLibrary(module);
        return DefinitivePreflight::Status(result.status, code, result.win32Error, result.verifiedFiles, code == 0);
    }
    catch (const std::runtime_error& error)
    {
        return DefinitivePreflight::Status(error.what(), 8);
    }
}
