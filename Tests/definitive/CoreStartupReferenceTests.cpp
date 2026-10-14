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
        HANDLE         mapping{}, entered{}, release{}, inputRelease{}, movementRelease{};
        volatile LONG* counters{};
        explicit Fixture(unsigned serial)
        {
            const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(serial);
            const auto mapName = L"Local\\MtaCoreMap-" + suffix;
            const auto enteredName = L"Local\\MtaCoreEntered-" + suffix;
            const auto releaseName = L"Local\\MtaCoreRelease-" + suffix;
            const auto inputReleaseName = L"Local\\MtaCoreInputRelease-" + suffix;
            const auto movementReleaseName = L"Local\\MtaCoreMovementRelease-" + suffix;
            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 64 * sizeof(LONG), mapName.c_str());
            entered = CreateEventW(nullptr, TRUE, FALSE, enteredName.c_str());
            release = CreateEventW(nullptr, TRUE, FALSE, releaseName.c_str());
            inputRelease = CreateEventW(nullptr, TRUE, FALSE, inputReleaseName.c_str());
            movementRelease = CreateEventW(nullptr, TRUE, FALSE, movementReleaseName.c_str());
            Check(mapping && entered && release && inputRelease && movementRelease, "fixture kernel objects failed");
            counters = static_cast<volatile LONG*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 64 * sizeof(LONG)));
            Check(counters != nullptr, "fixture shared telemetry failed");
            Set(L"MTA_CORE_FIXTURE_MAP", mapName);
            Set(L"MTA_CORE_FIXTURE_ENTERED", enteredName);
            Set(L"MTA_CORE_FIXTURE_RELEASE", releaseName);
            Set(L"MTA_CORE_FIXTURE_INPUT_RELEASE", inputReleaseName);
            Set(L"MTA_CORE_FIXTURE_MOVEMENT_RELEASE", movementReleaseName);
            for (const auto* name :
                 {L"MTA_CORE_FIXTURE_DELAY_MS", L"MTA_CORE_FIXTURE_BAD_VERSION", L"MTA_CORE_FIXTURE_BAD_STATE", L"MTA_CORE_FIXTURE_BAD_INITIALIZE",
                  L"MTA_CORE_FIXTURE_FAIL_ATTACH", L"MTA_CORE_FIXTURE_INPUT_MODE", L"MTA_CORE_FIXTURE_RENDER_MODE"})
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
            if (inputRelease)
                CloseHandle(inputRelease);
            if (movementRelease)
                CloseHandle(movementRelease);
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
        if (std::wstring(argv[2]).find(L"render") == 0)
        {
            using namespace DefinitiveRenderReference;
            Check(argc == 4, "render worker fixture requires its controlled Core DLL");
            const std::wstring mode = argv[2];
            const bool         v4 = mode.find(L"render-v4") == 0;
            const bool         v3 = mode.find(L"render-v3") == 0;
            const bool         v2 = mode.find(L"render-v2") == 0;
            const bool         explicitRoute = v2 || v3 || v4;
            const bool         cancel = mode.find(L"cancel") != std::wstring::npos;
            const bool         deadlineFailure = mode.find(L"deadline") != std::wstring::npos;
            const bool         noCallbacks = mode.find(L"missing") != std::wstring::npos;
            const unsigned     malformed = mode.find(L"cross-route") != std::wstring::npos     ? 2u
                                           : mode.find(L"mixed-version") != std::wstring::npos ? 3u
                                           : mode.find(L"wrong-mask") != std::wstring::npos    ? 4u
                                                                                               : 0u;
            const auto         route = mode.find(L"d3d11") != std::wstring::npos ? 1u : 2u;
            Fixture            fixture(110);
            CopyCore(std::filesystem::absolute(argv[3]), root);
            SetEvent(fixture.release);
            Set(L"MTA_CORE_FIXTURE_RENDER_MODE", std::to_wstring(noCallbacks                                             ? 1u
                                                                 : mode.find(L"query-failed") != std::wstring::npos      ? 5u
                                                                 : mode.find(L"unknown-presenter") != std::wstring::npos ? 6u
                                                                                                                         : malformed));
            Check(!worker.BeginRenderReferenceV2(&anchor, 0) && !worker.BeginRenderReferenceV2(&anchor, 3) &&
                      !worker.BeginRenderReferenceV2(&anchor, 0xffffffffu),
                  "unknown route must reject before scheduling a worker");
            Check(!worker.BeginRenderReferenceV3(&anchor, 0) && !worker.BeginRenderReferenceV3(&anchor, 1) && !worker.BeginRenderReferenceV3(&anchor, 3) &&
                      !worker.BeginRenderReferenceV3(&anchor, 0xffffffffu),
                  "API3 requires explicit D3D12 before scheduling or acquiring COM references");
            Check(!worker.BeginRenderReferenceV4(&anchor, 0) && !worker.BeginRenderReferenceV4(&anchor, 1) && !worker.BeginRenderReferenceV4(&anchor, 3) &&
                      !worker.BeginRenderReferenceV4(&anchor, 0xffffffffu),
                  "API4 classification must explicitly select D3D12 before scheduling");
            Check(v4   ? worker.BeginRenderReferenceV4(&anchor, route)
                  : v3 ? worker.BeginRenderReferenceV3(&anchor, route)
                  : v2 ? worker.BeginRenderReferenceV2(&anchor, route)
                       : worker.BeginRenderReference(&anchor),
                  "Core render worker failed to begin");
            Check(!worker.BeginInputReference(&anchor) && !worker.BeginMovementReference(&anchor), "render job must exclude input and movement lifetimes");
            Check(!worker.BeginRenderReference(&anchor) && !worker.BeginRenderReferenceV2(&anchor, route) && !worker.BeginRenderReferenceV3(&anchor, 2) &&
                      !worker.BeginRenderReferenceV4(&anchor, 2),
                  "all renderer protocols must share one retained owner");
            const auto capture = [&]
            {
                return v4 ? worker.CaptureRenderV4() : v3 ? worker.CaptureRenderV3() : v2 ? worker.CaptureRenderV2() : worker.CaptureRender();
            };
            const auto read = [&](std::uint32_t field)
            {
                return v4 ? worker.ReadRenderV4(field) : v3 ? worker.ReadRenderV3(field) : v2 ? worker.ReadRenderV2(field) : worker.ReadRender(field);
            };
            const auto deadline = GetTickCount64() + 6000;
            while (!capture())
            {
                Check(worker.Poll() == 0 && GetTickCount64() < deadline, "render callback failed to publish while pending");
                Sleep(5);
            }
            Check(read(Phase) == 1 && read(GenerationHigh) == 2 && read(Width) == (explicitRoute ? 0u : 1280u) && read(BackendPinnedExpected) == 1 &&
                      read(v4   ? FieldCountV4
                           : v3 ? FieldCountV3
                           : v2 ? FieldCountV2
                                : FieldCount) == 0,
                  "render cache must preserve the coherent numeric publication and high counter halves");
            Check(v4 ? !worker.CaptureRender() && !worker.CaptureRenderV2() && !worker.CaptureRenderV3() && !worker.ReadRender(Version) &&
                           !worker.ReadRenderV2(Version) && !worker.ReadRenderV3(Version)
                  : v3 ? !worker.CaptureRender() && !worker.CaptureRenderV2() && !worker.ReadRender(Version) && !worker.ReadRenderV2(Version) &&
                             !worker.CaptureRenderV4()
                  : v2 ? !worker.CaptureRender() && !worker.ReadRender(Version) && !worker.CaptureRenderV3()
                       : !worker.CaptureRenderV2() && !worker.ReadRenderV2(Version) && !worker.CaptureRenderV3(),
                  "a cache cannot be read through the other protocol");
            if (!v4)
                Check(!worker.CaptureRenderV4() && !worker.ReadRenderV4(Version), "legacy caches cannot expose classification tail fields");
            else
                Check(read(80) == 0 && read(81) == 0 && read(82) == 0 && read(83) == 0,
                      "warmup cannot invent a classified custom presenter before a publication");
            if (explicitRoute)
                Check(read(Version) == (v4   ? 4u
                                        : v3 ? 3u
                                             : 2u) &&
                          read(RequestedRoute) == route && !read(ObservedRoute) && !read(PresenceKnownMask) && !read(QueuePresent),
                      "warmup must preserve the explicit request without inventing a naturally observed route");
            bool        foreignRejected = false;
            std::thread foreign(
                [&]
                {
                    foreignRejected = !capture() && read(0) == 0 && !worker.BeginRenderReference(&anchor) && !worker.BeginRenderReferenceV2(&anchor, route) &&
                                      !worker.BeginRenderReferenceV3(&anchor, 2) && !worker.BeginRenderReferenceV4(&anchor, 2);
                });
            foreign.join();
            Check(foreignRejected, "render cache and lifecycle must stay with the script owner");
            if (cancel)
                Check(Fast([&] { return worker.Cancel(); }), "render cancellation must remain value-only");
            SetEvent(fixture.inputRelease);
            while (read(Phase) != 3)
            {
                Check(worker.Poll() == 0 && GetTickCount64() < deadline, "render cleanup must retain its Core owner");
                capture();
                Sleep(5);
            }
            Check(read(CleanupPending) == 1 && GetModuleHandleW(L"core.dll"), "render cleanup cannot release loaded Core");
            if (deadlineFailure)
            {
                while (!read(CleanupDeadlineExceeded))
                {
                    Check(Fast([&] { return worker.Poll(); }) == 0 && GetTickCount64() < deadline, "deadline must not authorize unsafe Core unload");
                    capture();
                    Sleep(10);
                }
                Check(read(Reason) == static_cast<unsigned>(StopReason::CleanupError) && GetModuleHandleW(L"core.dll"),
                      "cleanup deadline must report failure while retaining the actual owner");
            }
            SetEvent(fixture.movementRelease);
            Wait(worker);
            Check(read(Phase) == 3, "worker completion cannot mutate the owner's captured render array");
            Check(capture() && read(Phase) == 2 && read(CleanupComplete) == 1 && read(CleanupPending) == 0 && read(NativeCleanupPending) == 0 &&
                      read(Active) == 0,
                  "terminal render state must be captured only after safe cleanup");
            Check(worker.Read(13) == 1 && worker.Read(15) == 1 &&
                      worker.Read(16) == (cancel                           ? 12u
                                          : (deadlineFailure || malformed) ? 15u
                                                                           : 0u),
                  "render backend PIN must remain separate from the unloaded Core owner");
            if (explicitRoute && !malformed)
                Check(read(RequestedRoute) == route && read(ObservedRoute) == (noCallbacks ? 0u : route) &&
                          read(PresenceKnownMask) == (noCallbacks  ? 0u
                                                      : route == 2 ? 19u
                                                                   : 15u) &&
                          read(QueuePresent) == (!noCallbacks && route == 2 ? 1u : 0u),
                      "terminal provenance must reflect publication, including clean zero-callback completion");
            if (v3 && !malformed && !noCallbacks)
            {
                const bool failed = mode.find(L"query-failed") != std::wstring::npos;
                Check(read(ProbeAttemptsLow) == 10 && read(ProbeAcceptedLow) == (failed ? 0u : 10u) && read(ProbeRejectedLow) == (failed ? 10u : 0u),
                      "Worker must copy GPU counters coherently without converting a query failure to acceptance");
                Check(read(QueryStatus) == (failed ? 3u : 1u) && read(QueryStage) == (failed ? 8u : 19u) && read(MetadataValid) == (failed ? 0u : 1u),
                      "Full query failure metadata must survive retained cleanup and Core unload");
                Check(!worker.CaptureRenderV2() && !worker.ReadRenderV2(Version) && read(ReservedV3) == 0 && read(ReservedHighV3) == 0,
                      "API3 cache cannot be reinterpreted as a legacy observation");
            }
            if (v4 && !malformed && !noCallbacks)
            {
                const bool failed = mode.find(L"query-failed") != std::wstring::npos;
                const bool unknown = mode.find(L"unknown-presenter") != std::wstring::npos;
                Check(read(ProbeAttemptsLow) == 10 && read(ProbeAcceptedLow) == 0 && read(ProbeRejectedLow) == 10 && !read(SupportPredicate),
                      "API4 classification must preserve the custom-present GPU exclusion");
                Check(read(QueryStatus) == (failed ? 3u : 2u) && read(QueryStage) == (failed ? 8u : 19u) && read(MetadataValid) == (failed ? 0u : 1u),
                      "COM failure and classification metadata must remain separate through cleanup");
                Check(
                    read(80) == (unknown ? 3u : 2u) && read(81) == (unknown ? 0u : 1u) && read(82) == (unknown ? 0u : 127u) && read(83) == (unknown ? 0u : 15u),
                    "complete classification tail must survive coherent capture and Core unload without converting unknown to known");
                Check(!worker.CaptureRenderV3() && !worker.ReadRenderV3(Version) && !read(ReservedV3) && !read(ReservedHighV3),
                      "API4 cannot reinterpret API3 caches or use their reserved fields");
            }
            Check(fixture.counters[2] == 0 && fixture.counters[1] == fixture.counters[0], "render callbacks and unload must share the worker thread");
        }
        else if (std::wstring(argv[2]).find(L"movement") == 0)
        {
            Check(argc == 4, "movement worker fixture requires controlled Core DLL");
            const bool cancel = std::wstring(argv[2]) == L"movement-cancel";
            Fixture    fixture(100);
            CopyCore(std::filesystem::absolute(argv[3]), root);
            SetEvent(fixture.release);
            Check(worker.BeginMovementReference(&anchor), "Core movement worker failed to begin");
            const auto deadline = GetTickCount64() + 5000;
            while (!worker.CaptureMovement())
            {
                Check(worker.Poll() == 0 && GetTickCount64() < deadline, "movement callback failed to publish while pending");
                Sleep(5);
            }
            Check(worker.ReadMovement(DefinitiveInputReference::Phase) == 1 && worker.ReadMovement(DefinitiveMovementReference::WriterStarted) == 1 &&
                      worker.ReadMovement(DefinitiveMovementReference::ContextKnownMask) == 15 &&
                      worker.ReadMovement(DefinitiveMovementReference::BackendPinnedExpected) == 1,
                  "configured movement admission/PIN cache mismatch");
            bool        foreignRejected = false;
            std::thread foreign([&] { foreignRejected = !worker.CaptureMovement() && worker.ReadMovement(0) == 0 && !worker.BeginMovementReference(&anchor); });
            foreign.join();
            Check(foreignRejected, "movement capture and lifecycle must stay with the script owner");
            if (cancel)
                Check(Fast([&] { return worker.Cancel(); }), "movement cancellation must remain a value-only request");
            SetEvent(fixture.inputRelease);
            while (worker.ReadMovement(DefinitiveInputReference::Phase) != 3)
            {
                Check(worker.Poll() == 0 && GetTickCount64() < deadline, "movement cleanup must remain pending with its Core owner");
                worker.CaptureMovement();
                Sleep(5);
            }
            Check(worker.ReadMovement(DefinitiveMovementReference::CleanupPending) == 1 && GetModuleHandleW(L"core.dll") != nullptr,
                  "a busy writer must retain the actual worker-owned Core module");
            const auto held = GetTickCount64();
            while (GetTickCount64() - held < 100)
            {
                Check(Fast([&] { return worker.Poll(); }) == 0, "pending native cleanup must not publish worker completion");
                Sleep(5);
            }
            SetEvent(fixture.movementRelease);
            Wait(worker);
            Check(worker.ReadMovement(DefinitiveInputReference::Phase) == 3, "completion must not mutate the owner's captured movement array");
            Check(worker.CaptureMovement() && worker.ReadMovement(DefinitiveInputReference::Phase) == 2 &&
                      worker.ReadMovement(DefinitiveMovementReference::CleanupPending) == 0 &&
                      worker.ReadMovement(DefinitiveInputReference::InputCleanupComplete) == 1,
                  "terminal movement cleanup cache missing");
            Check(worker.Read(13) == 1 && worker.Read(15) == 1 && worker.Read(16) == (cancel ? 12u : 0u),
                  "a pinned backend must be distinguished from a live Core owner");
            Check(
                worker.ReadMovement(DefinitiveMovementReference::NativeMetrics) == 0 &&
                    worker.ReadMovement(DefinitiveMovementReference::NativeMetrics + 2) == 0 &&
                    worker.ReadMovement(DefinitiveMovementReference::NativeMetrics + 6) == worker.ReadMovement(DefinitiveMovementReference::NativeMetrics + 12),
                "final movement metrics must prove inactive restored scopes");
            Check(fixture.counters[2] == 0 && fixture.counters[1] == fixture.counters[0], "movement callback/unload must share the worker thread");
        }
        else if (std::wstring(argv[2]) == L"actual")
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

            // The controlled DLL below is explicitly not Core. It exercises the
            // production worker's callback ownership and coherent cache while a
            // real Core/actual-class fixture separately proves MTA semantics.
            for (unsigned scenario = 0; scenario < 4; ++scenario)
            {
                Fixture input(30 + scenario);
                CopyCore(normal, root);
                Set(L"MTA_CORE_FIXTURE_INPUT_MODE", std::to_wstring(scenario));
                SetEvent(input.release);
                Check(worker.BeginInputReference(&anchor), "configured input worker failed to begin");
                Check(worker.ReadInput(0) == 0, "a new attempt must not leak its predecessor's captured sample");
                if (scenario < 2)
                {
                    const auto deadline = GetTickCount64() + 5000;
                    while (!worker.CaptureInput())
                    {
                        Check(worker.Poll() == 0 && GetTickCount64() < deadline, "configured callback failed to publish while pending");
                        Sleep(5);
                    }
                    Check(worker.ReadInput(DefinitiveInputReference::Version) == 1 && worker.ReadInput(DefinitiveInputReference::Phase) == 1 &&
                              worker.ReadInput(DefinitiveInputReference::Sequence) == 1,
                          "first live callback must be captured before completion");
                    Check(
                        worker.ReadInput(DefinitiveInputReference::Current + 1) == 0xffffff80u && worker.ReadInput(DefinitiveInputReference::Previous + 1) == 0,
                        "signed controller values and actual previous callback state must survive the cache");
                    bool        rejected = false;
                    std::thread foreignInput([&] { rejected = !worker.CaptureInput() && worker.ReadInput(0) == 0 && !worker.BeginInputReference(&anchor); });
                    foreignInput.join();
                    Check(rejected, "foreign configured cache reads and Begin must be rejected");
                    if (scenario)
                        Check(Fast([&] { return worker.Cancel(); }), "pending configured input cancellation must stay nonblocking");
                    SetEvent(input.inputRelease);
                    Wait(worker);
                    // Completion can replace the worker's published sample, but
                    // individual field reads stay on one previously captured copy.
                    Check(worker.ReadInput(DefinitiveInputReference::Phase) == 1 && worker.ReadInput(DefinitiveInputReference::Sequence) == 1,
                          "worker publication must not tear or mutate the owner's captured sample");
                    Check(worker.CaptureInput(), "terminal configured snapshot must survive Core unload");
                    Check(worker.ReadInput(DefinitiveInputReference::Phase) == 2 && worker.ReadInput(DefinitiveInputReference::Sequence) == 2 &&
                              worker.ReadInput(DefinitiveInputReference::InputCleanupComplete) == 1,
                          "terminal callback must advance sequence and retain cleanup evidence");
                    for (unsigned field = DefinitiveInputReference::Current; field < DefinitiveInputReference::FieldCount; ++field)
                        Check(worker.ReadInput(field) == 0, "terminal configured current/previous must be neutral");
                    Check(worker.ReadInput(DefinitiveInputReference::Reason) == (scenario ? 2u : 1u), "normal/cancelled callback reason mismatch");
                    Check(worker.ReadInput(DefinitiveInputReference::FieldCount) == 0, "unknown configured cache field must be neutral");
                    VerifyCleanup(worker, scenario ? 12u : 0u);
                }
                else
                {
                    SetEvent(input.inputRelease);
                    Wait(worker);
                    VerifyCleanup(worker, scenario == 2 ? 15u : 13u);
                    Check(worker.Read(17) == 0, "bad configured API or cleanup must not claim completion");
                }
                Check(input.counters[2] == 0 && input.counters[1] == input.counters[0], "configured Core callbacks and unload must stay on their worker");
            }
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
