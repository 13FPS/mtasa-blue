/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#include "../../Client/definitive/CoreStartupReference.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
    int       checks = 0;
    int       anchor = 0;
    ULONGLONG worstApiMs = 0;
    void      Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    template <class Function>
    auto Fast(Function function)
    {
        const auto begin = GetTickCount64();
        const auto result = function();
        const auto elapsed = GetTickCount64() - begin;
        worstApiMs = (std::max)(worstApiMs, elapsed);
        Check(elapsed < 500, "value-only worker API blocked near the loader delay");
        return result;
    }
    void Wait(deonline::CoreStartupReference& worker)
    {
        const auto begin = GetTickCount64();
        while (Fast([&] { return worker.Poll(); }) == 0)
        {
            Check(GetTickCount64() - begin < 15000, "worker failed to finish in fixture budget");
            Sleep(10);
        }
        Check(worker.Poll() == 1 && !worker.IsPending(), "completion must mean the worker thread exited");
        Check(GetModuleHandleW(L"core.dll") == nullptr, "worker must unload Core before publication");
    }
    void Set(const wchar_t* name, const std::wstring& value)
    {
        Check(SetEnvironmentVariableW(name, value.c_str()) != FALSE, "fixture environment setup failed");
    }
    struct Fixture
    {
        HANDLE         mapping{}, entered{}, release{};
        volatile LONG* counters{};
        explicit Fixture(unsigned serial)
        {
            const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(serial);
            const auto mapName = L"Local\\MtaCoreMap-" + suffix;
            const auto enteredName = L"Local\\MtaCoreEntered-" + suffix;
            const auto releaseName = L"Local\\MtaCoreRelease-" + suffix;
            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 64 * sizeof(LONG), mapName.c_str());
            entered = CreateEventW(nullptr, TRUE, FALSE, enteredName.c_str());
            release = CreateEventW(nullptr, TRUE, FALSE, releaseName.c_str());
            Check(mapping && entered && release, "fixture kernel objects failed");
            counters = static_cast<volatile LONG*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 64 * sizeof(LONG)));
            Check(counters != nullptr, "fixture shared telemetry failed");
            Set(L"MTA_CORE_FIXTURE_MAP", mapName);
            Set(L"MTA_CORE_FIXTURE_ENTERED", enteredName);
            Set(L"MTA_CORE_FIXTURE_RELEASE", releaseName);
            for (const auto* name : {L"MTA_CORE_FIXTURE_DELAY_MS", L"MTA_CORE_FIXTURE_BAD_VERSION", L"MTA_CORE_FIXTURE_BAD_STATE",
                                     L"MTA_CORE_FIXTURE_BAD_INITIALIZE", L"MTA_CORE_FIXTURE_FAIL_ATTACH"})
                Set(name, L"0");
        }
        ~Fixture()
        {
            if (counters)
                UnmapViewOfFile(const_cast<LONG*>(counters));
            if (mapping)
                CloseHandle(mapping);
            if (entered)
                CloseHandle(entered);
            if (release)
                CloseHandle(release);
        }
    };
    void CopyCore(const std::filesystem::path& source, const std::filesystem::path& root)
    {
        Check(GetModuleHandleW(L"core.dll") == nullptr, "cannot replace a loaded fixture Core");
        Check(CopyFileW(source.c_str(), (root / L"core.dll").c_str(), FALSE) != FALSE, "fixture Core copy failed");
    }
    void VerifyCleanup(deonline::CoreStartupReference& worker, unsigned error)
    {
        Check(worker.Read(13) == 1 && worker.Read(15) == 0 && worker.Read(16) == error, "terminal failure/cleanup snapshot mismatch");
        Check(worker.Read(19) == 0 && worker.Read(0xffffffffu) == 0, "unknown fields must remain neutral");
    }
    struct JoinForeign
    {
        std::thread& thread;
        HANDLE       go, release;
        ~JoinForeign()
        {
            // A failed assertion must release the controlled loader hold before
            // joining the pre-existing callback thread during stack unwinding.
            if (thread.joinable())
            {
                SetEvent(go);
                SetEvent(release);
                thread.join();
            }
        }
    };
}

int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try
    {
        Check(sizeof(void*) == 8 && argc >= 3, "x64 fixture requires owned root and mode");
        const auto root = std::filesystem::absolute(argv[1]);
        wchar_t    executable[32768]{};
        GetModuleFileNameW(nullptr, executable, 32768);
        Check(std::filesystem::path(executable).parent_path() == root, "worker anchor must be the owned host adjacent to Core");
        // Mirror the pinned bridge lifetime even on an assertion failure while
        // a worker is still finishing its controlled loader cleanup.
        static deonline::CoreStartupReference worker;
        Check(worker.Poll() == -1 && !worker.Cancel() && !worker.IsPending(), "fresh worker is idle");
        Check(!worker.Begin(nullptr), "null anchor must be rejected");
        for (unsigned field = 0; field < 20; ++field)
            Check(worker.Read(field) == 0, "idle cache must be neutral");
        if (std::wstring(argv[2]) == L"actual")
        {
            Check(Fast([&] { return worker.Begin(&anchor); }), "actual Core worker failed to begin");
            Wait(worker);
            const std::array<unsigned, 11> expected{1, 5, 3, 33, 0, 0, 0, 1, 3, 4, 5};
            for (unsigned field = 0; field < expected.size(); ++field)
                Check(worker.Read(field) == expected[field], "actual wrong-image Core diagnostic mismatch");
            VerifyCleanup(worker, 8);
            Check(worker.Read(17) == 0 && worker.Read(18) == 0, "actual wrong-image rejection cannot certify the game or report a loader failure");
            Check(GetModuleHandleW(L"game_de.dll") == nullptr, "actual backend must be released");
        }
        else
        {
            Check(argc == 5, "synthetic mode requires normal and missing-export fixtures");
            const auto normal = std::filesystem::absolute(argv[3]);
            const auto missing = std::filesystem::absolute(argv[4]);
            for (unsigned scenario = 0; scenario < 6; ++scenario)
            {
                Fixture fixture(scenario);
                CopyCore(scenario == 4 ? missing : normal, root);
                if (scenario == 1)
                    Set(L"MTA_CORE_FIXTURE_BAD_VERSION", L"1");
                if (scenario == 2)
                    Set(L"MTA_CORE_FIXTURE_BAD_STATE", L"1");
                if (scenario == 3)
                    Set(L"MTA_CORE_FIXTURE_BAD_INITIALIZE", L"1");
                if (scenario == 5)
                    Set(L"MTA_CORE_FIXTURE_FAIL_ATTACH", L"1");
                SetEvent(fixture.release);
                Check(Fast([&] { return worker.Begin(&anchor); }), "fixture begin failed");
                Wait(worker);
                const unsigned error = scenario == 0 ? 0u : scenario == 1 ? 6u : scenario == 2 ? 8u : scenario == 3 ? 7u : scenario == 4 ? 5u : 4u;
                VerifyCleanup(worker, error);
                Check(fixture.counters[0] != static_cast<LONG>(GetCurrentThreadId()) && fixture.counters[2] == 0,
                      "Core entry/diagnostics/setters must stay on one separate worker");
                Check(fixture.counters[1] == fixture.counters[0], "Core unload must stay on the loading worker");
                if (scenario == 0)
                {
                    Check(worker.Read(17) == 1 && worker.Read(18) == 0, "valid synthetic Core must publish success");
                    Check(fixture.counters[3] == 3 && fixture.counters[4] == 1 && fixture.counters[5] == 1 && fixture.counters[6] == 8,
                          "production worker must execute the real three-step directory/diagnostics sequence");
                }
                if (scenario == 5)
                    Check(worker.Read(18) == ERROR_DLL_INIT_FAILED, "DllMain rejection must preserve exact loader error1114");
            }
            for (unsigned scenario = 0; scenario < 2; ++scenario)
            {
                Check(DeleteFileW((root / L"core.dll").c_str()) != FALSE, "owned Core removal failed");
                if (scenario)
                {
                    const auto file = CreateFileW((root / L"core.dll").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                    Check(file != INVALID_HANDLE_VALUE, "broken image fixture creation failed");
                    DWORD      written = 0;
                    const char invalid[] = "Not a PE image";
                    Check(WriteFile(file, invalid, sizeof(invalid), &written, nullptr) != FALSE, "broken image fixture write failed");
                    CloseHandle(file);
                }
                Check(worker.Begin(&anchor), "loader failure attempt failed to begin");
                Wait(worker);
                VerifyCleanup(worker, 4);
                const auto expectedError = static_cast<std::uint32_t>(scenario ? ERROR_BAD_EXE_FORMAT : ERROR_MOD_NOT_FOUND);
                Check(worker.Read(18) == expectedError, "missing/broken image error was not captured immediately");
                if (!scenario)
                    CopyCore(normal, root);
            }
            Fixture slow(20);
            CopyCore(normal, root);
            Set(L"MTA_CORE_FIXTURE_DELAY_MS", L"2200");
            HANDLE foreignReady = CreateEventW(nullptr, TRUE, FALSE, nullptr), foreignGo = CreateEventW(nullptr, TRUE, FALSE, nullptr),
                   foreignDone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Check(foreignReady && foreignGo && foreignDone, "foreign callback fixture events failed");
            bool foreignCancel = false, foreignRejected = false;
            // Start this thread before the DLL holds the loader lock: creating
            // and joining a new thread inside that hold would test Windows,
            // rather than the worker's value-only foreign cancellation path.
            std::thread foreign(
                [&]
                {
                    SetEvent(foreignReady);
                    WaitForSingleObject(foreignGo, 10000);
                    foreignCancel = worker.Cancel();
                    foreignRejected = worker.Poll() == -1 && worker.Read(0) == 0 && !worker.Begin(&anchor);
                    SetEvent(foreignDone);
                });
            JoinForeign foreignCleanup{foreign, foreignGo, slow.release};
            Check(WaitForSingleObject(foreignReady, 3000) == WAIT_OBJECT_0, "foreign callback thread not ready");
            Check(Fast([&] { return worker.Begin(&anchor); }), "slow Core attempt failed to begin");
            Check(WaitForSingleObject(slow.entered, 3000) == WAIT_OBJECT_0, "controlled DllMain was not entered");
            const auto hold = GetTickCount64();
            SetEvent(foreignGo);
            Check(WaitForSingleObject(foreignDone, 500) == WAIT_OBJECT_0, "foreign Cancel blocked on the loader lock");
            while (GetTickCount64() - hold < 2250)
            {
                Check(Fast([&] { return worker.Poll(); }) == 0, "slow load must remain pending");
                Check(!Fast([&] { return worker.Begin(&anchor); }), "second attempt must be rejected without blocking");
                for (unsigned field = 0; field < 19; ++field)
                    Check(worker.Read(field) == 0, "pending cache must not expose worker writes");
                Sleep(25);
            }
            SetEvent(slow.release);
            Wait(worker);
            foreign.join();
            Check(foreignCancel && foreignRejected, "foreign callback cancellation/owner guards failed");
            VerifyCleanup(worker, 12);
            Check(worker.Read(17) == 0 && slow.counters[3] == 0, "cancelled load must not enter Core initialization");
            Check(slow.counters[1] == slow.counters[0], "cancelled DLL must unload on its worker");
            CloseHandle(foreignReady);
            CloseHandle(foreignGo);
            CloseHandle(foreignDone);
            Set(L"MTA_CORE_FIXTURE_DELAY_MS", L"0");
            Check(worker.Begin(&anchor), "completed cancellation must permit another owned attempt");
            Wait(worker);
            VerifyCleanup(worker, 0);
            Check(worker.Read(17) == 1, "successful retry must replace the cancelled cache");
        }
        std::printf("{\"success\":true,\"checks\":%d,\"worst_api_ms\":%llu,\"actual_mode\":%s,\"game_loaded\":false}\n", checks,
                    static_cast<unsigned long long>(worstApiMs), std::wstring(argv[2]) == L"actual" ? "true" : "false");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("{\"success\":false,\"checks\":%d,\"error\":\"%s\"}\n", checks, error.what());
        return 2;
    }
}
