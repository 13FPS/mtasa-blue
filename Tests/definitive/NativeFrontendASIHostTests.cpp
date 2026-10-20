/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client tests
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/
#include "NativeFrontendASIFixture.h"
#include "../../Client/definitive/NativeFrontendHost.h"
#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{
    int  checks = 0;
    void Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    struct Fixture
    {
        HANDLE         mapping = nullptr, entered = nullptr, completed = nullptr;
        volatile LONG* words = nullptr;
        Fixture()
        {
            namespace F = NativeFrontendASIFixture;
            const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
            const auto mapName = L"Local\\MtaAsiMap-" + suffix;
            const auto enteredName = L"Local\\MtaAsiEntered-" + suffix;
            const auto completedName = L"Local\\MtaAsiCompleted-" + suffix;
            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, F::FieldCount * sizeof(LONG), mapName.c_str());
            entered = CreateEventW(nullptr, TRUE, FALSE, enteredName.c_str());
            completed = CreateEventW(nullptr, TRUE, FALSE, completedName.c_str());
            Check(mapping && entered && completed, "owned ASI telemetry objects could not be created");
            words = static_cast<volatile LONG*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, F::FieldCount * sizeof(LONG)));
            Check(words != nullptr, "owned ASI telemetry mapping failed");
            Check(SetEnvironmentVariableW(F::MapVariable, mapName.c_str()) && SetEnvironmentVariableW(F::EnteredVariable, enteredName.c_str()) &&
                      SetEnvironmentVariableW(F::CompletedVariable, completedName.c_str()),
                  "owned ASI telemetry environment failed");
        }
        ~Fixture()
        {
            if (words)
                UnmapViewOfFile(const_cast<LONG*>(words));
            for (const auto handle : {mapping, entered, completed})
                if (handle)
                    CloseHandle(handle);
        }
        LONG Read(unsigned field) const { return InterlockedCompareExchange(words + field, 0, 0); }
    };
}

int wmain(int argc, wchar_t** argv)
{
    ULONGLONG firstMs = 0, repeatMs = 0, duringStartMs = 0;
    LONG      controlledStartMs = 0;
    try
    {
        namespace F = NativeFrontendASIFixture;
        namespace N = deonline::NativeFrontendHostProtocol;
        Check(argc == 2 && sizeof(void*) == 8, "owned x64 ASI host requires an absolute staged ASI path");
        const auto asiPath = std::filesystem::path(argv[1]);
        Check(asiPath.is_absolute() && asiPath.filename() == L"MtaDefinitive.asi", "ASI path must identify the owned staged shipping artifact");
        Check(std::filesystem::current_path() != asiPath.parent_path(), "ASI must resolve paths independently from the process working directory");
        Fixture    fixture;
        const auto asi = LoadLibraryExW(asiPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(asi != nullptr, "the exact staged shipping ASI did not load");
        using Initialize = void (*)();
        const auto initialize = reinterpret_cast<Initialize>(GetProcAddress(asi, "InitializeASI"));
        Check(initialize != nullptr, "shipping ASI export InitializeASI is missing");
        auto started = GetTickCount64();
        initialize();
        firstMs = GetTickCount64() - started;
        Check(firstMs < 250, "InitializeASI waited for the delayed controlled Bridge startup");
        started = GetTickCount64();
        initialize();
        repeatMs = GetTickCount64() - started;
        Check(repeatMs < 250, "repeat InitializeASI was not promptly idempotent");
        Check(WaitForSingleObject(fixture.entered, 5000) == WAIT_OBJECT_0, "ASI bootstrap never reached the controlled absolute-path Bridge");
        started = GetTickCount64();
        initialize();
        duringStartMs = GetTickCount64() - started;
        Check(duringStartMs < 250, "InitializeASI blocked while the bootstrap was starting Bridge");
        Check(WaitForSingleObject(fixture.completed, 5000) == WAIT_OBJECT_0, "controlled Bridge startup did not complete");
        controlledStartMs = fixture.Read(F::ElapsedMs);
        Check(controlledStartMs >= 1400 && fixture.Read(F::Complete) == 1 && fixture.Read(F::Calls) == 1,
              "shipping ASI must start exactly once despite repeated initialization during a slow Bridge call");
        Check(fixture.Read(F::RequestSize) == static_cast<LONG>(sizeof(N::RequestV1)) && fixture.Read(F::RequestVersion) == static_cast<LONG>(N::ApiVersion) &&
                  fixture.Read(F::RequestFlags) == static_cast<LONG>(N::BindingPersistence | N::LocalInput) && !fixture.Read(F::RequestReserved) &&
                  !fixture.Read(F::InvalidArgument),
              "shipping ASI did not deliver the coherent API1 durable/local-input request");
        Check(static_cast<DWORD>(fixture.Read(F::CallerThread)) != GetCurrentThreadId() && fixture.Read(F::CallerThread) != 0,
              "Bridge startup must run on a bootstrap thread independently from the ASI entry caller");
        const auto expectedBridge = asiPath.parent_path() / L"CLEO" / L"de-online" / L"de_client_bridge.dll";
        const auto bridge = GetModuleHandleW(expectedBridge.c_str());
        Check(bridge != nullptr, "ASI did not retain its successfully started controlled Bridge");
        std::array<wchar_t, 32768> actualBridge{};
        const auto                 length = GetModuleFileNameW(bridge, actualBridge.data(), static_cast<DWORD>(actualBridge.size()));
        Check(length && length < actualBridge.size() && std::filesystem::path(std::wstring(actualBridge.data(), length)) == expectedBridge,
              "shipping ASI loaded a Bridge outside its absolute package path");
        Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"cgui.dll"),
              "controlled ASI glue test must not claim or create actual MTA/native game services");
        std::printf(
            "{\"success\":true,\"checks\":%d,\"initialize_ms\":%llu,\"repeat_ms\":%llu,\"during_start_ms\":%llu,"
            "\"controlled_start_ms\":%ld,\"start_calls\":1,\"request_api\":1,\"request_flags\":3,\"bootstrap_thread_verified\":true,"
            "\"absolute_bridge_path_verified\":true,\"shipping_asi_executed\":true,\"bridge_is_controlled_fixture\":true,"
            "\"game_launched\":false,\"native_hooks_installed\":false,\"full_mta_startup_verified\":false}\n",
            checks, static_cast<unsigned long long>(firstMs), static_cast<unsigned long long>(repeatMs), static_cast<unsigned long long>(duringStartMs),
            controlledStartMs);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        std::printf("{\"success\":false,\"checks\":%d,\"initialize_ms\":%llu,\"repeat_ms\":%llu,\"during_start_ms\":%llu}\n", checks,
                    static_cast<unsigned long long>(firstMs), static_cast<unsigned long long>(repeatMs), static_cast<unsigned long long>(duringStartMs));
        return 1;
    }
}
