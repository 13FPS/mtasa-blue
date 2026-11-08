/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Load the actual x64 client with its real sibling dependencies
 *****************************************************************************/
#include "../../Client/core/StartupModule.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    unsigned checks = 0;

    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }

    constexpr std::array dependencies{L"bass.dll", L"bass_fx.dll", L"bassmix.dll", L"tags.dll", L"lua5.1c.dll", L"pcre2.dll"};
    constexpr std::array excluded{L"core.dll", L"netc.dll", L"net.dll", L"game_sa.dll", L"multiplayer_sa.dll", L"game_de.dll", L"cgui.dll"};

    void CheckExcluded()
    {
        for (const auto* name : excluded)
            Check(GetModuleHandleW(name) == nullptr, "loader-only check must not construct Core, network, game or GUI modules");
    }

    void CheckExactPath(HMODULE module, const std::filesystem::path& expected)
    {
        Check(module != nullptr, "the actual client's imported dependency is loaded");
        std::array<wchar_t, 32768> name{};
        const DWORD                length = GetModuleFileNameW(module, name.data(), static_cast<DWORD>(name.size()));
        Check(length != 0 && length < name.size(), "loaded module path is complete");
        Check(CompareStringOrdinal(name.data(), -1, expected.c_str(), -1, TRUE) == CSTR_EQUAL,
              "the client selected its own sibling dependency, not an ambient classic/server DLL");
    }
}

int wmain(int argc, wchar_t** argv)
{
    static_assert(sizeof(void*) == 8 && MtaStartup::Module::HostMachine == IMAGE_FILE_MACHINE_AMD64);
    try
    {
        Check(argc == 2, "supply the explicit actual Release/x64 client.dll path");
        const std::filesystem::path client(argv[1]);
        Check(client.is_absolute() && client.filename() == L"client.dll", "the actual client module path is absolute");
        Check(GetModuleHandleW(client.c_str()) == nullptr, "the actual client begins unloaded in a fresh process");
        for (const auto* name : dependencies)
            Check(GetModuleHandleW(name) == nullptr, "no prior dependency load may mask missing client staging");
        CheckExcluded();
        Check(SetDllDirectoryW(L"") != FALSE, "exclude the current directory from this isolated loader check");

        // This is the production architecture/path owner, loading the real
        // linked module. Do not call InitClient: its public factory can enter
        // the still-unavailable game/network startup through CClientGame.
        MtaStartup::Module owner;
        if (!owner.Load(client.wstring()))
            throw std::runtime_error("actual client module load failed, Windows error " + std::to_string(owner.Error()));
        CheckExactPath(owner.Get(), client);
        Check(GetProcAddress(owner.Get(), "InitClient") != nullptr, "the actual MTA client exports its unchanged InitClient ABI");
        for (const auto* name : dependencies)
            CheckExactPath(GetModuleHandleW(name), client.parent_path() / name);
        CheckExcluded();

        Check(owner.Unload(), "the exact client loader owner acknowledges retirement");
        Check(GetModuleHandleW(client.c_str()) == nullptr, "the actual client DLL retired without entering its game factory");
        for (const auto* name : dependencies)
            Check(GetModuleHandleW(name) == nullptr, "the loader releases the actual imported dependency references");
        CheckExcluded();
        std::cout << "Passed " << checks << " real client module loader checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Real client module loader check failed: " << error.what() << '\n';
        return 1;
    }
}
