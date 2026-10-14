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
#include <thread>
#include "../../Client/core/DefinitiveInputReference.h"
#include "../../Client/core/DefinitiveRenderReference.h"

namespace
{
    int          checks = 0;
    unsigned int inputPublications = 0;
    bool         CancelInput()
    {
        return false;
    }
    void PublishInput(const std::uint32_t*, std::uint32_t)
    {
        ++inputPublications;
    }
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
    void NoClassicRendererModules()
    {
        // DE startup cannot require the legacy renderer runtime before any
        // graphics backend is selected. A real module check catches eager
        // imports even on a machine where the old runtime happens to exist.
        for (const auto* name : {L"d3dx9_43.dll", L"d3d9.dll"})
            Check(GetModuleHandleW(name) == nullptr, "DE startup must not load a classic D3D9 renderer dependency");
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
    using ReadStartup = unsigned int (*)(unsigned int);
    void CheckStartup(ReadStartup read, unsigned int result, unsigned int phase, unsigned int capabilities)
    {
        const std::array<unsigned int, 8> expected{1, result, phase, capabilities, 0, 0, 0, 1};
        for (unsigned int field = 0; field < expected.size(); ++field)
            Check(read(field) == expected[field], "actual Core startup snapshot must preserve its completed attempt and cleanup");
    }
}
int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try
    {
        Check(sizeof(void*) == 8 && (argc == 2 || (argc == 3 && std::wcscmp(argv[2], L"nonclassic") == 0)),
              "fixture requires x64 and its owned absolute directory/layout");
        const auto root = std::filesystem::path(argv[1]);
        Check(root.is_absolute(), "fixture path must be explicit and absolute");
        const bool nonclassic = argc == 3;
        const auto moduleDirectory = nonclassic ? root / L"CLEO" / L"de-online" : root / L"mta";
        const auto corePath = moduleDirectory / L"core.dll";
        NoGameModules();
        NoClassicRendererModules();
        Check(GetModuleHandleW(L"game_de.dll") == nullptr, "backend starts unloaded");
        const auto core = LoadLibraryExW(corePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!core)
        {
            std::printf("{\"success\":false,\"stage\":\"LoadLibraryEx\",\"win32_error\":%lu}\n", GetLastError());
            return 2;
        }
        NoClassicRendererModules();
        using SetDirectory = void (*)(const wchar_t*, std::size_t);
        using Initialize = int (*)();
        const auto setMta = Resolve<SetDirectory>(core, "SetMTADirectory");
        const auto setGta = Resolve<SetDirectory>(core, "SetGTADirectory");
        const auto initialize = Resolve<Initialize>(core, "InitializeCore");
        const auto startupVersion = Resolve<unsigned int (*)()>(core, "MTA_DEStartupApiVersion");
        const auto readStartup = Resolve<ReadStartup>(core, "MTA_ReadDEStartupState");
        const auto inputVersion = Resolve<std::uint32_t (*)()>(core, "MTA_DEInputReferenceApiVersion");
        using ConfigureInput = std::uint32_t (*)(std::uint32_t, DefinitiveInputReference::CancelCallback, DefinitiveInputReference::PublishCallback);
        const auto configureInput = Resolve<ConfigureInput>(core, "MTA_ConfigureDEInputReference");
        const auto movementVersion = Resolve<std::uint32_t (*)()>(core, "MTA_DEMovementReferenceApiVersion");
        const auto configureMovement = Resolve<ConfigureInput>(core, "MTA_ConfigureDEMovementReference");
        const auto renderVersion = Resolve<std::uint32_t (*)()>(core, "MTA_DERenderReferenceApiVersion");
        const auto configureRender = Resolve<ConfigureInput>(core, "MTA_ConfigureDERenderReference");
        const auto renderVersionV2 = Resolve<std::uint32_t (*)()>(core, "MTA_DERenderReferenceApiVersionV2");
        using ConfigureRenderV2 =
            std::uint32_t (*)(std::uint32_t, std::uint32_t, DefinitiveInputReference::CancelCallback, DefinitiveInputReference::PublishCallback);
        const auto configureRenderV2 = Resolve<ConfigureRenderV2>(core, "MTA_ConfigureDERenderReferenceV2");
        const auto renderVersionV3 = Resolve<std::uint32_t (*)()>(core, "MTA_DERenderReferenceApiVersionV3");
        const auto configureRenderV3 = Resolve<ConfigureRenderV2>(core, "MTA_ConfigureDERenderReferenceV3");
        const auto renderVersionV4 = Resolve<std::uint32_t (*)()>(core, "MTA_DERenderReferenceApiVersionV4");
        const auto configureRenderV4 = Resolve<ConfigureRenderV2>(core, "MTA_ConfigureDERenderReferenceV4");
        Check(renderVersionV4() == 4 && DefinitiveRenderReference::FieldCountV4 == 84, "API4 exports bind a separate additive classification contract");
        for (const auto route : {0u, 1u, 3u, 0xffffffffu})
            Check(configureRenderV4(1, route, CancelInput, PublishInput) == 0, "API4 classification cannot fall back to a legacy renderer route");
        Check(configureRenderV4(0, 2, CancelInput, PublishInput) == 0 && configureRenderV4(10001, 2, CancelInput, PublishInput) == 0,
              "API4 retains the bounded renderer diagnostic duration");
        Check(configureRenderV4(1, 2, nullptr, PublishInput) == 0 && configureRenderV4(1, 2, CancelInput, nullptr) == 0,
              "API4 requires owner cancellation and value publication callbacks");
        Check(renderVersionV3() == 3 && DefinitiveRenderReference::FieldCountV3 == 80, "API3 exports bind the separate 80-field scalar ownership contract");
        for (const auto route : {0u, 1u, 3u, 0xffffffffu})
            Check(configureRenderV3(1, route, CancelInput, PublishInput) == 0, "API3 requires explicit D3D12 without an earlier-version fallback");
        Check(configureRenderV3(0, 2, CancelInput, PublishInput) == 0 && configureRenderV3(10001, 2, CancelInput, PublishInput) == 0,
              "API3 retains the existing ten-second duration bound");
        Check(configureRenderV3(1, 2, nullptr, PublishInput) == 0 && configureRenderV3(1, 2, CancelInput, nullptr) == 0,
              "API3 requires owner cancellation and value publication callbacks");
        Check(renderVersionV2() == 2, "explicit route contract must use an additive API2 export");
        for (const auto route : {0u, 3u, 0xffffffffu})
            Check(configureRenderV2(1, route, CancelInput, PublishInput) == 0, "unknown renderer route must fail before Core activation");
        Check(configureRenderV2(0, 2, CancelInput, PublishInput) == 0 && configureRenderV2(10001, 2, CancelInput, PublishInput) == 0,
              "explicit renderer route must keep the existing ten-second bound");
        Check(configureRenderV2(1, 2, nullptr, PublishInput) == 0 && configureRenderV2(1, 2, CancelInput, nullptr) == 0,
              "API2 must retain owned cancellation and publication callbacks");
        Check(renderVersion() == 1, "Core-owned render observation ABI must be explicit");
        Check(configureRender(0, CancelInput, PublishInput) == 0, "zero-duration render observation must be rejected");
        Check(configureRender(10001, CancelInput, PublishInput) == 0, "render observation must stay within ten seconds");
        Check(configureRender(1, nullptr, PublishInput) == 0 && configureRender(1, CancelInput, nullptr) == 0,
              "render observation must retain owned cancellation and publication callbacks");
        Check(movementVersion() == 1, "Core-owned configured movement ABI must be explicit");
        Check(configureMovement(0, CancelInput, PublishInput) == 0, "zero-duration movement request must be rejected");
        Check(configureMovement(30001, CancelInput, PublishInput) == 0, "movement request must stay below the native writer deadline");
        Check(configureMovement(1, nullptr, PublishInput) == 0, "movement request must carry cancellation");
        Check(configureMovement(1, CancelInput, nullptr) == 0, "movement request must carry owned snapshots");
        Check(inputVersion() == 1, "configured input reference ABI must be explicit");
        Check(configureInput(0, CancelInput, PublishInput) == 0, "zero-duration input request must be rejected");
        Check(configureInput(60001, CancelInput, PublishInput) == 0, "unbounded input request must be rejected");
        Check(configureInput(1, nullptr, PublishInput) == 0, "input request must carry worker cancellation");
        Check(configureInput(1, CancelInput, nullptr) == 0, "input request must carry value-only publication");
        Check(configureInput(1, CancelInput, PublishInput) == 1, "bounded configured input request must be accepted");
        Check(configureRender(1, CancelInput, PublishInput) == 0, "render observation cannot replace pending configured input");
        Check(configureRenderV2(1, 2, CancelInput, PublishInput) == 0, "API2 route cannot replace a pending input request");
        Check(configureRenderV3(1, 2, CancelInput, PublishInput) == 0, "API3 cannot replace a pending configured input owner");
        Check(configureRenderV4(1, 2, CancelInput, PublishInput) == 0, "API4 cannot replace a pending configured input owner");
        Check(configureMovement(1, CancelInput, PublishInput) == 0, "movement cannot replace a pending read-only request");
        Check(configureInput(1, CancelInput, PublishInput) == 0, "a pending input request must not be overwritten");
        unsigned int foreignConfigure = 1;
        std::thread  requester([&] { foreignConfigure = configureInput(1, CancelInput, PublishInput); });
        requester.join();
        Check(foreignConfigure == 0, "a foreign caller must not replace the owned input request");
        Check(startupVersion() == 1, "the additive DE startup diagnostics ABI must be explicit");
        Check(readStartup(0) == 0xffffffffu, "startup state has no read owner before InitializeCore");
        std::array<int, 4> results{};
        results[0] = initialize();
        Check(results[0] == 3, "DE candidate must request MTA directory before construction");
        CheckStartup(readStartup, 3, 1, 0);
        Check(readStartup(8) == 0xffffffffu, "unknown startup fields must be rejected");
        NoClassicRendererModules();
        // The legacy loader supplies the MTA root, while the DE diagnostic
        // supplies its owned adapter directory. Neither requires registry state.
        const auto mta = (nonclassic ? moduleDirectory : root).wstring();
        setMta(mta.c_str(), mta.size());
        results[1] = initialize();
        Check(results[1] == 4, "DE candidate must request GTA directory before construction");
        CheckStartup(readStartup, 4, 1, 0);
        NoClassicRendererModules();
        const auto gta = root.wstring();
        setGta(gta.c_str(), gta.size());
        for (std::size_t index = 2; index < results.size(); ++index)
        {
            if (index == 3)
            {
                Check(configureRenderV2(1, 2, CancelInput, PublishInput) == 1, "failed binding must consume old input and permit an explicit render route");
                Check(configureRender(1, CancelInput, PublishInput) == 0 && configureRenderV2(1, 1, CancelInput, PublishInput) == 0,
                      "API1 and another API2 route must not replace the pending D3D12 request");
                Check(configureMovement(1, CancelInput, PublishInput) == 0, "pending render observation must exclude movement writes");
            }
            results[index] = initialize();
            Check(results[index] == 5, "real incomplete adapter must reject construction and allow clean retry");
            CheckStartup(readStartup, 5, 3, 33);
            Check(GetModuleHandleW(L"game_de.dll") == nullptr, "partial Core construction must release the backend DLL");
            NoGameModules();
            NoClassicRendererModules();
            Check(inputPublications == 0, "wrong-image rejection must happen before configured input or XML access");
        }
        // Exercise the new request through the same real Core entry point. The
        // exact-image gate must still reject before any callback or COM query.
        Check(configureRenderV3(1, 2, CancelInput, PublishInput) == 1, "consumed API2 request permits an explicit API3 owner");
        Check(configureRender(1, CancelInput, PublishInput) == 0 && configureRenderV2(1, 2, CancelInput, PublishInput) == 0 &&
                  configureMovement(1, CancelInput, PublishInput) == 0 && configureInput(1, CancelInput, PublishInput) == 0,
              "pending API3 excludes earlier observers, configured input and writes");
        std::uint32_t foreignGpuRequest = 1;
        std::thread   gpuRequester([&] { foreignGpuRequest = configureRenderV3(1, 2, CancelInput, PublishInput); });
        gpuRequester.join();
        Check(foreignGpuRequest == 0, "foreign API3 request cannot replace the owning worker's request");
        const int api3Result = initialize();
        Check(api3Result == 5, "actual API3 request still fails the nongame exact-image gate");
        CheckStartup(readStartup, 5, 3, 33);
        Check(GetModuleHandleW(L"game_de.dll") == nullptr, "failed API3 construction releases its backend lease");
        NoGameModules();
        NoClassicRendererModules();
        Check(inputPublications == 0, "wrong-image API3 never reaches native ownership queries or publication");
        // The actual new export must consume and release its request through
        // the real Core, while the exact-image gate prevents native inspection.
        Check(configureRenderV4(1, 2, CancelInput, PublishInput) == 1, "consumed API3 request permits an explicit API4 owner");
        Check(configureRender(1, CancelInput, PublishInput) == 0 && configureRenderV2(1, 2, CancelInput, PublishInput) == 0 &&
                  configureRenderV3(1, 2, CancelInput, PublishInput) == 0 && configureMovement(1, CancelInput, PublishInput) == 0 &&
                  configureInput(1, CancelInput, PublishInput) == 0,
              "pending classification retains one owner and excludes earlier observers, input and writes");
        std::uint32_t foreignClassificationRequest = 1;
        std::thread   classificationRequester([&] { foreignClassificationRequest = configureRenderV4(1, 2, CancelInput, PublishInput); });
        classificationRequester.join();
        Check(foreignClassificationRequest == 0, "foreign classification request cannot replace its owner");
        const int api4Result = initialize();
        Check(api4Result == 5, "actual API4 request still fails the nongame exact-image gate");
        CheckStartup(readStartup, 5, 3, 33);
        Check(GetModuleHandleW(L"game_de.dll") == nullptr, "failed API4 construction releases the backend before return");
        NoGameModules();
        NoClassicRendererModules();
        Check(inputPublications == 0, "wrong-image API4 cannot inspect custom presenter memory or publish evidence");
        // A diagnostic reader cannot accidentally become a cross-thread Core
        // control channel or erase the completed owner-thread failure snapshot.
        std::array<unsigned int, 8> foreign{};
        std::thread                 reader(
            [&]
            {
                for (unsigned int field = 0; field < foreign.size(); ++field)
                    foreign[field] = readStartup(field);
            });
        reader.join();
        for (const auto value : foreign)
            Check(value == 0xffffffffu, "foreign startup reads must be rejected");
        CheckStartup(readStartup, 5, 3, 33);
        Check(FreeLibrary(core) != FALSE, "real Core must unload normally");
        Check(GetModuleHandleW(L"core.dll") == nullptr && GetModuleHandleW(L"game_de.dll") == nullptr, "no fixture DLL remains after unload");
        NoGameModules();
        NoClassicRendererModules();
        // Reaching this statement proves DE detach did not take legacy ExitProcess.
        std::printf(
            "{\"success\":true,\"checks\":%d,\"initialize_results\":[%d,%d,%d,%d],\"continued_after_free_library\":true,\"backend_unloaded_after_each_"
            "failure\":true,\"classic_modules_unloaded\":true,\"legacy_renderer_modules_unloaded\":true,\"startup_diagnostics_verified\":true,\"api3_"
            "initialize_result\":%d,\"api4_initialize_result\":%d}\n",
            checks, results[0], results[1], results[2], results[3], api3Result, api4Result);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Core entry fixture failed after %d checks: %s\n", checks, error.what());
        return 1;
    }
}
