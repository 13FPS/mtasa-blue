/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise production startup module admission and loader ownership
 *****************************************************************************/
#include "../../Client/core/StartupModule.h"

#if defined(MTA_TEST_STARTUP_DEPENDENCY)
extern "C" __declspec(dllexport) unsigned StartupDependencyMarker()
{
    return static_cast<unsigned>(sizeof(void*) * 8);
}
#elif defined(MTA_TEST_STARTUP_PROVIDER)
extern "C" __declspec(dllimport) unsigned StartupDependencyMarker();
extern "C" __declspec(dllexport) unsigned StartupProviderMarker()
{
    return 100 + StartupDependencyMarker();
}
#else
    #include <array>
    #include <cstdint>
    #include <cstddef>
    #include <cstring>
    #include <filesystem>
    #include <fstream>
    #include <iostream>
    #include <iterator>
    #include <stdexcept>
    #include <type_traits>
    #include <vector>

namespace
{
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }
    void Save(const std::filesystem::path& path, const std::vector<char>& bytes)
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        Check(stream.good(), "write owned malformed module fixture");
    }
    unsigned Marker(HMODULE module)
    {
        using Read = unsigned (*)();
        Read       read = nullptr;
        const auto entry = GetProcAddress(module, "StartupProviderMarker");
        static_assert(sizeof(read) == sizeof(entry));
        std::memcpy(&read, &entry, sizeof(read));
        Check(read != nullptr, "loaded exact provider exposes its own marker export");
        return read();
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        static_assert(!std::is_copy_constructible_v<MtaStartup::Module> && !std::is_copy_assignable_v<MtaStartup::Module>);
        Check(argc == 4, "supply explicit matching provider, opposite architecture, and output root");
        const std::filesystem::path provider(argv[1]), other(argv[2]), output(argv[3]);
        const auto                  dependency = provider.parent_path() / L"startup-dependency.dll";
        Check(provider.is_absolute() && other.is_absolute() && output.is_absolute(), "all fixture paths are explicit absolute paths");
        Check(!GetModuleHandleW(provider.c_str()) && !GetModuleHandleW(dependency.c_str()), "provider and sibling dependency begin unloaded");
        MtaStartup::Module module;
        Check(!module.Load(L"startup-provider.dll") && module.Error() == ERROR_INVALID_NAME && !module.Get(),
              "relative startup module never searches ambient paths");
        auto nul = provider.wstring();
        nul.push_back(L'\0');
        nul += L"ignored";
        Check(!module.Load(nul) && module.Error() == ERROR_INVALID_NAME && !module.Get(), "embedded NUL cannot select a shorter startup path");
        Check(!module.Load((output / L"absent.dll").wstring()) && module.Error() == ERROR_FILE_NOT_FOUND && !module.Get(),
              "missing module records loader error without an owned handle");
        Check(!module.Load(other.wstring()) && module.Error() == ERROR_BAD_EXE_FORMAT && module.Machine() != MtaStartup::Module::HostMachine && !module.Get() &&
                  !GetModuleHandleW(other.c_str()),
              "opposite architecture is identified before module execution or dependency loading");
        std::ifstream           source(provider, std::ios::binary);
        const std::vector<char> original{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
        Check(original.size() > sizeof(IMAGE_DOS_HEADER), "actual provider supplies an owned PE corruption baseline");
        for (unsigned mode = 0; mode != 5; ++mode)
        {
            auto bytes = original;
            if (mode == 0)
                bytes.resize(3);
            else if (mode == 1)
                bytes[0] = 0;
            else if (mode == 2)
            {
                const LONG invalid = -1;
                std::memcpy(bytes.data() + offsetof(IMAGE_DOS_HEADER, e_lfanew), &invalid, sizeof(invalid));
            }
            else if (mode == 3)
            {
                const LONG invalid = INT32_MAX;
                std::memcpy(bytes.data() + offsetof(IMAGE_DOS_HEADER, e_lfanew), &invalid, sizeof(invalid));
            }
            else
            {
                IMAGE_DOS_HEADER dos{};
                std::memcpy(&dos, bytes.data(), sizeof(dos));
                bytes[dos.e_lfanew] = 0;
            }
            const auto malformed = output / (L"malformed-" + std::to_wstring(mode) + L".dll");
            Save(malformed, bytes);
            Check(!module.Load(malformed.wstring()) && module.Error() == ERROR_BAD_EXE_FORMAT && !module.Get() && !GetModuleHandleW(malformed.c_str()),
                  "malformed image cannot reach executable startup loading");
        }
        Check(module.Load(provider.wstring()) && module.Error() == ERROR_SUCCESS && module.Machine() == MtaStartup::Module::HostMachine,
              "exact matching-architecture provider loads after previous admission refusals");
        const auto first = module.Get();
        Check(first && Marker(first) == 100 + sizeof(void*) * 8 && GetModuleHandleW(dependency.c_str()),
              "module-directory dependency resolves through the existing startup search policy");
        Check(!module.Load(other.wstring()) && module.Error() == ERROR_ALREADY_EXISTS && module.Get() == first && Marker(first) == 100 + sizeof(void*) * 8,
              "second Load retains the original callback code and does not leak or replace its loader reference");
        Check(module.Unload(), "explicit owned module unload succeeds");
        Check(!module.Get() && module.Error() == ERROR_SUCCESS && !module.Machine() && !GetModuleHandleW(provider.c_str()) &&
                  !GetModuleHandleW(dependency.c_str()),
              "explicit Unload releases exactly the owned provider and its sibling dependency");
        Check(module.Unload(), "repeated empty unload is idempotent");
        Check(module.Load(provider.wstring()), "explicitly retired owner can load a fresh module lifetime");
        Check(module.Unload(), "fresh module lifetime unload succeeds");
        {
            MtaStartup::Module scoped;
            Check(scoped.Load(provider.wstring()) && Marker(scoped.Get()) == 100 + sizeof(void*) * 8,
                  "scoped owner executes only the explicit compatible provider");
        }
        Check(!GetModuleHandleW(provider.c_str()) && !GetModuleHandleW(dependency.c_str()),
              "destructor releases the final module ownership without a second reference");
        std::cout << "Passed " << checks << " startup module checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Startup module failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
#endif
