/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#include "../../Client/definitive/CoreStartupReference.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
    std::atomic<DWORD>          observedDeleteThread{0};
    std::atomic<volatile LONG*> observedDeleteCounters{nullptr};
    void                        ObserveWorkerDelete(void* memory) noexcept
    {
        if (!memory || GetCurrentThreadId() != observedDeleteThread.load())
            return;
        const auto counters = observedDeleteCounters.load();
        if (counters && counters[25])
            InterlockedIncrement(counters + 26);
    }
}

// Observe consumer-owned path/string destruction only after the controlled
// provider arms its pending-exception boundary. Nothing blocks the allocator;
// the fixture verifies quarantine occurs before heap ownership is unwound.
void operator delete(void* memory) noexcept
{
    ObserveWorkerDelete(memory);
    std::free(memory);
}
void operator delete[](void* memory) noexcept
{
    ::operator delete(memory);
}
void operator delete(void* memory, std::size_t) noexcept
{
    ::operator delete(memory);
}
void operator delete[](void* memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

static_assert(DefinitiveGuiStartupReference::ApiVersion == 1);
static_assert(DefinitiveGuiStartupReference::FieldCount == 64);
static_assert(sizeof(DefinitiveGuiStartupReference::Snapshot) == 64 * sizeof(std::uint32_t));
static_assert(DefinitiveGuiStartupReference::BackendPinnedExpected == 57);
static_assert(DefinitiveGuiStartupReference::LeaseMetadataValid == 58);
static_assert(DefinitiveGuiStartupReference::DeviceReleaseFault == 59);
static_assert(DefinitiveGuiSurfaceReference::ApiVersion == 1 && DefinitiveGuiSurfaceReference::FieldCount == 96);
static_assert(sizeof(DefinitiveGuiSurfaceReference::Snapshot) == 96 * sizeof(std::uint32_t));
static_assert(DefinitiveGuiSurfaceReference::ChildRetained == 75 && DefinitiveGuiSurfaceReference::ClassUnregistered == 80);
static_assert(DefinitiveClockReference::ApiVersion == 1 && DefinitiveClockReference::FieldCount == 96);
static_assert(DefinitiveClockSetReference::ApiVersion == 1 && DefinitiveClockSetReference::FieldCount == 128);
static_assert(DefinitiveClockSetReference::InitialReadAcknowledged == 88 && DefinitiveClockSetReference::ProviderCapturedSnapshotNotCompared == 121);

namespace deonline
{
    struct CoreStartupReferenceTestAccess
    {
        static SRWLOCK&      InputLock(CoreStartupReference& worker) noexcept { return worker.inputLock; }
        static std::uint32_t PublishedPhase(const CoreStartupReference& worker) noexcept { return worker.publishedRenderV6[DefinitiveRenderReference::Phase]; }
        static std::uint32_t PublishedGuiPhase(const CoreStartupReference& worker) noexcept
        {
            return worker.publishedGui[DefinitiveGuiStartupReference::Phase];
        }
        static std::uint32_t PublishedSurfacePhase(const CoreStartupReference& worker) noexcept
        {
            return worker.publishedGuiSurface[DefinitiveGuiStartupReference::Phase];
        }
        static bool ConflictingSurfaceStart(CoreStartupReference& worker, const void* address, unsigned mode) noexcept
        {
            return worker.Start(address, true, mode == 0, mode == 1, 2, 6, mode == 2, true);
        }
        static bool ConflictingGuiStart(CoreStartupReference& worker, const void* address, bool movement) noexcept
        {
            return worker.Start(address, true, movement, !movement, 2, 6, true);
        }
        static bool ConflictingClockStart(CoreStartupReference& worker, const void* address) noexcept
        {
            return worker.Start(address, true, true, false, 0, 1, false, false, false, false, false, false, true);
        }
        static bool ConflictingClockSetStart(CoreStartupReference& worker, const void* address) noexcept
        {
            return worker.Start(address, true, false, false, 0, 1, false, false, false, false, false, false, true, true);
        }
        static bool ClockBeforeWithoutFrontend(CoreStartupReference& worker, const void* address) noexcept
        {
            return worker.Start(address, true, false, false, 0, 1, false, false, false, false, false, false, false, false, true);
        }
    };
}

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
    template <class Capture, class Read>
    bool ContendedClockProgress(Capture capture, Read read, volatile LONG* counters)
    {
        const auto previousCount = counters[9];
        const auto previousSequence = read(DefinitiveClockReference::Sequence);
        const auto deadline = GetTickCount64() + 1000;
        bool       lastCapture = false;
        // A bounded seqlock capture may legitimately refuse one concurrent
        // publication. Observe a later coherent publication while the real
        // input lock stays held, without assuming a fixed scheduler interval.
        do
        {
            lastCapture = Fast(capture);
            if (lastCapture && counters[9] > previousCount && read(DefinitiveClockReference::Phase) == 3 &&
                read(DefinitiveClockReference::Sequence) > previousSequence)
                return true;
            Sleep(1);
        } while (GetTickCount64() < deadline);
        std::fprintf(stderr, "Clock cache contention timeout: count_before=%ld count_after=%ld sequence_before=%u sequence_after=%u phase=%u captured=%u\n",
                     static_cast<long>(previousCount), static_cast<long>(counters[9]), previousSequence, read(DefinitiveClockReference::Sequence),
                     read(DefinitiveClockReference::Phase), lastCapture ? 1u : 0u);
        return false;
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
                  L"MTA_CORE_FIXTURE_FAIL_ATTACH", L"MTA_CORE_FIXTURE_INPUT_MODE", L"MTA_CORE_FIXTURE_RENDER_MODE", L"MTA_CORE_FIXTURE_GUI_MODE",
                  L"MTA_CORE_FIXTURE_SURFACE_MODE", L"MTA_CORE_FIXTURE_CLOCK_MODE", L"MTA_CORE_FIXTURE_FRONTEND_CLOCK_MODE", L"MTA_CORE_FIXTURE_RUNTIME_MODE"})
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

    void TestRuntimeWorker(deonline::CoreStartupReference& worker, const std::filesystem::path& root, const std::wstring& mode,
                           const std::filesystem::path& fixturePath)
    {
        namespace R = deonline::FrontendRuntimeReport;
        const std::array<const wchar_t*, 20> modes{L"runtime-two-views",
                                                   L"runtime-stop-hidden",
                                                   L"runtime-stop-showing",
                                                   L"runtime-stop-visible",
                                                   L"runtime-host-expired",
                                                   L"runtime-visible-expired",
                                                   L"runtime-focus-hide",
                                                   L"runtime-gui-hide",
                                                   L"runtime-wrong-api",
                                                   L"runtime-refused",
                                                   L"runtime-view-fault",
                                                   L"runtime-cursor-begin-fault",
                                                   L"runtime-cursor-renew-fault",
                                                   L"runtime-retained-stop",
                                                   L"runtime-shutdown-busy",
                                                   L"runtime-cache-contention",
                                                   L"runtime-returned-pending",
                                                   L"runtime-malformed-first-failed",
                                                   L"runtime-active-without-cleanup-debt",
                                                   L"runtime-active-already-complete"};
        const auto                           found = std::find(modes.begin(), modes.end(), mode);
        Check(found != modes.end(), "known runtime fixture mode");
        const auto scenario = static_cast<unsigned>(found - modes.begin());
        Fixture    fixture(500 + scenario);
        Set(L"MTA_CORE_FIXTURE_RUNTIME_MODE", std::to_wstring(scenario));
        CopyCore(fixturePath, root);
        SetEvent(fixture.release);
        const auto generation = Fast([&] { return worker.BeginFrontendRuntime(&anchor); });
        Check(generation && !worker.BeginPersistentFrontend(&anchor) && !worker.Begin(&anchor), "runtime exclusively retains its worker");
        const auto deadline = GetTickCount64() + 14000;
        const auto read = [&](std::uint32_t field) { return worker.ReadFrontendRuntime(field); };
        const auto capture = [&] { return Fast([&] { return worker.CaptureFrontendRuntime(); }); };
        const auto wait = [&](const auto& predicate, bool heartbeat = true)
        {
            while (!capture() || !predicate())
            {
                if (heartbeat)
                    worker.HeartbeatFrontendRuntime(generation);
                Check(GetTickCount64() < deadline, "runtime state failed to reach controlled boundary");
                Sleep(5);
            }
        };
        const auto command = [&](std::uint32_t sequence, R::Command value)
        { Check(Fast([&] { return worker.CommandFrontendRuntime(generation, sequence, static_cast<std::uint32_t>(value)); }), "runtime command admitted"); };
        bool        foreignRefused = false;
        std::thread foreign(
            [&]
            {
                foreignRefused = worker.PollFrontendRuntime() == -1 && !worker.CaptureFrontendRuntime() && !worker.ReadFrontendRuntime(0) &&
                                 !worker.HeartbeatFrontendRuntime(generation) && !worker.CommandFrontendRuntime(generation, 1, 3);
            });
        foreign.join();
        Check(foreignRefused, "runtime mailbox/cache/heartbeat retain the script owner");
        if (scenario == 8)
        {
            Wait(worker);
            Check(capture() && read(R::LastFailure) == static_cast<std::uint32_t>(R::Failure::Exports) && !fixture.counters[44],
                  "wrong runtime API never constructs Core");
            return;
        }
        wait([&] { return read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Hidden); });
        Check(read(R::CoreSampleStage) == 1 && read(R::CoreOffset + 33) == 2 && read(R::CoreOffset + 43) == 1 && read(R::CoreOffset + 44) == 1 &&
                  read(R::CoreOffset + 45) == 1,
              "runtime retains one actual configuration/binding owner in Hidden");
        Check(!worker.CommandFrontendRuntime(generation + 1, 1, 1) && !worker.CommandFrontendRuntime(generation, 1, 99) &&
                  !worker.CommandFrontendRuntime(generation, UINT32_MAX, 1),
              "stale session, unknown verb and nonwrapping Show sequence are refused");
        if (scenario == 1 || scenario == 14)
            command(1, R::Command::Stop);
        else if (scenario != 4)
        {
            command(1, R::Command::Show);
            if (scenario == 16 || scenario == 17)
            {
                wait([&] { return read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::RetainedCleanup); });
                Check(worker.PollFrontendRuntime() == 0 && worker.IsPending() && GetModuleHandleW(L"core.dll") && fixture.counters[45] == 2 &&
                          !fixture.counters[48] && !fixture.counters[49] && !fixture.counters[1],
                      "unpublished Busy or malformed-first Failed retains code/TLS/owner before any later Core read or shutdown");
                if (scenario == 17)
                    Check(fixture.counters[50] == 1 && !read(R::ViewSamples),
                          "rejected first publication cannot qualify the no-publication InputBind exception");
                return;
            }
            if (scenario == 2)
            {
                wait([&] { return read(R::ViewSamples) > 0; });
                Check(!read(R::ShowAcknowledgements), "native-only partial cursor admission cannot acknowledge Show");
                command(2, R::Command::Stop);
            }
            else if (scenario == 18 || scenario == 19)
            {
                wait([&] { return read(R::ViewSamples) > 0 && read(R::ViewOffset + 1) == 1; });
                Check(!read(R::ShowAcknowledgements) && read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Showing) &&
                          read(R::ViewOffset + 5) == (scenario == 18 ? 0u : 1u) && read(R::ViewOffset + 6) == (scenario == 19 ? 1u : 0u),
                      "a live sample without cleanup debt or already marked complete cannot acknowledge Show");
                Check(read(R::ViewOffset + 68) && read(R::ViewOffset + 70) && read(R::ViewOffset + 161) == 1 && read(R::ViewOffset + 162) == 1 &&
                          read(R::ViewOffset + 236) == 1 && read(R::ViewOffset + 237) == 1 && read(R::ViewOffset + 262) == 1,
                      "ownership-word rejection is independent of otherwise admitted GUI/native/cursor evidence");
                SetEvent(fixture.inputRelease);
            }
            else if (scenario != 9 && scenario != 10 && scenario != 11 && scenario != 12)
            {
                wait([&] { return read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Visible); });
                Check(read(R::AcknowledgedSequence) == 1 && read(R::AcknowledgementCode) == 1 && read(R::CoreSampleStage) == 2 &&
                          read(R::CoreOffset + 33) == 2 && !fixture.counters[52] && read(R::ViewOffset + 5) == 1 && !read(R::ViewOffset + 6),
                      "Show acknowledges real active cleanup ownership and GUI admission while keeping the historical Core sample");
                Check(!worker.CommandFrontendRuntime(generation, 1, 2) && !worker.CommandFrontendRuntime(generation, 2, 1),
                      "stale sequence and Show-while-visible refused");
                if (scenario == 3 || scenario == 13)
                    command(2, R::Command::Stop);
                if (scenario == 13)
                {
                    wait([&] { return read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::RetainedCleanup); });
                    Check(worker.PollFrontendRuntime() == 0 && !fixture.counters[48] && !fixture.counters[49] && GetModuleHandleW(L"core.dll"),
                          "Stop is not acknowledged while per-view native cleanup remains retained");
                    SetEvent(fixture.movementRelease);
                }
                if (scenario == 0 || scenario == 15)
                {
                    if (scenario == 15)
                    {
                        AcquireSRWLockExclusive(&deonline::CoreStartupReferenceTestAccess::InputLock(worker));
                        const auto sequence = read(R::SequenceLow);
                        const auto until = GetTickCount64() + 100;
                        while (GetTickCount64() < until)
                        {
                            worker.HeartbeatFrontendRuntime(generation);
                            capture();
                            Sleep(5);
                        }
                        ReleaseSRWLockExclusive(&deonline::CoreStartupReferenceTestAccess::InputLock(worker));
                        Check(read(R::SequenceLow) > sequence, "runtime416 atomic cache never takes historical input lock");
                    }
                    command(2, R::Command::Hide);
                    wait([&] { return read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Hidden) && read(R::AcknowledgedSequence) == 2; });
                    Check(read(R::ViewsRetired) == 1 && read(R::CoreOffset + 46) == read(R::CoreOffset + 47) && read(R::CoreOffset + 37) == 1 &&
                              read(R::CoreOffset + 38) == 1 && !fixture.counters[48],
                          "Hide retires a fresh view while retaining Core/backend/session services");
                    if (scenario == 0)
                    {
                        command(3, R::Command::Show);
                        wait([&] { return read(R::AcknowledgedSequence) == 3 && read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Visible); });
                        command(4, R::Command::Hide);
                        wait([&] { return read(R::AcknowledgedSequence) == 4 && read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Hidden); });
                        Check(read(R::ViewGenerationLow) == 2 && read(R::ViewsRetired) == 2 && read(R::CoreOffset + 43) == 1 && read(R::CoreOffset + 44) == 1 &&
                                  read(R::CoreOffset + 45) == 1,
                              "second Show reuses one retained owner and fresh native generation");
                        command(5, R::Command::Stop);
                    }
                    else
                        command(3, R::Command::Stop);
                }
                if (scenario == 6 || scenario == 7)
                {
                    wait([&] { return read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Hidden); });
                    Check(read(scenario == 6 ? R::FocusHideCount : R::GuiHideCount) == 1 && !fixture.counters[48],
                          "focus/GUI closure hides instead of shutting down Core");
                    command(2, R::Command::Stop);
                }
            }
        }
        while (worker.PollFrontendRuntime() == 0)
        {
            if (scenario != 4 && scenario != 5)
                worker.HeartbeatFrontendRuntime(generation);
            Check(GetTickCount64() < deadline, "runtime final cleanup remains pending beyond fixture budget");
            Sleep(5);
        }
        Check(capture() && read(R::CleanupComplete) == 1 && !read(R::CleanupPending) && read(R::CoreStoppedVerified) == 1 &&
                  read(R::CoreReleasedVerified) == 1 && read(R::CoreUnloadedVerified) == 1 && !read(R::CoreLoaded) && !read(R::CoreOwnerRetained),
              "runtime Stop/failure proves same-owner shutdown/release/unload");
        Check(fixture.counters[44] == 1 && fixture.counters[49] == 1 && !fixture.counters[52] && !fixture.counters[2] &&
                  fixture.counters[1] == fixture.counters[0],
              "no foreign or fictitious live Core reads; one completed owner is unloaded by its producer");
        if (scenario == 4 || scenario == 5)
            Check(read(R::HostExpiryObserved) == 1 && read(R::LastFailure) == 12, "host expiry is irrevocable and fail-closed");
        if (scenario == 11 || scenario == 12)
            Check(!read(R::ShowAcknowledgements), "cursor admission fault never acknowledges Show");
        if (scenario == 18 || scenario == 19)
            Check(!read(R::ShowAcknowledgements) && read(R::AcknowledgementCode) == 2 && read(R::LastFailure) == 5 && !read(R::ViewOffset + 5) &&
                      read(R::ViewOffset + 6) == 1 && read(R::ViewsRetired) == 1,
                  "invalid active ownership is refused without changing the independently qualified terminal retirement contract");
        if (scenario == 2)
            Check(read(R::SupersededRequests) == 1 && read(R::AcknowledgedSequence) == 2,
                  "Stop supersedes pending Show and acknowledges only after retirement");
        for (std::uint32_t field = 0; field < 19; ++field)
            Check(!worker.Read(field), "runtime never reinterprets Common19");
        Check(!worker.ReadFrontendRuntime(R::FieldCount) && !worker.CapturePersistentFrontend() && !worker.CaptureGuiFrontendStartupClockV2(),
              "runtime keeps every old cache separate");
    }

    struct WorkerDeleteObservation
    {
        void Arm(volatile LONG* counters) noexcept
        {
            observedDeleteCounters.store(counters);
            observedDeleteThread.store(static_cast<DWORD>(counters[0]));
        }
        ~WorkerDeleteObservation()
        {
            observedDeleteThread.store(0);
            observedDeleteCounters.store(nullptr);
        }
    };

    void TestFrontendClockWorker(deonline::CoreStartupReference& worker, const std::filesystem::path& root, const std::wstring& mode,
                                 const std::filesystem::path& coreFixture)
    {
        namespace Clock = DefinitiveClockReference;
        namespace Startup = DefinitiveFrontendStartup;
        namespace Report = deonline::FrontendStartupReport;
        const std::array<const wchar_t*, 25> modes = {L"frontend-clock",
                                                      L"frontend-clock-cancel",
                                                      L"frontend-clock-wrong-api",
                                                      L"frontend-clock-refused",
                                                      L"frontend-clock-workload-incomplete",
                                                      L"frontend-clock-short-workload",
                                                      L"frontend-clock-owner-corrupt",
                                                      L"frontend-clock-returned-pending",
                                                      L"frontend-clock-thrown-pending",
                                                      L"frontend-clock-stale-terminal",
                                                      L"frontend-clock-retained-terminal",
                                                      L"frontend-clock-cache-contention",
                                                      L"frontend-clock-thrown-retired",
                                                      L"frontend-clock-missing-export",
                                                      L"frontend-clock-no-publication-busy",
                                                      L"frontend-clock-no-publication-failed-unqualified",
                                                      L"frontend-clock-no-publication-thrown",
                                                      L"frontend-clock-mismatched-producer",
                                                      L"frontend-clock-wrong-clock-count",
                                                      L"frontend-clock-prebegin-cancel",
                                                      L"frontend-clock-reserved-terminal",
                                                      L"frontend-clock-view-failed",
                                                      L"frontend-clock-owner-read-thrown",
                                                      L"frontend-clock-binding-missing",
                                                      L"frontend-clock-view-caps-changed"};
        const auto                           found = std::find_if(modes.begin(), modes.end(), [&](const auto* value) { return mode == value; });
        Check(found != modes.end(), "completed clock worker fixture mode must be explicit");
        const auto              scenario = static_cast<unsigned>(found - modes.begin());
        Fixture                 fixture(400 + scenario);
        WorkerDeleteObservation deleteObservation;
        CopyCore(coreFixture, root);
        Set(L"MTA_CORE_FIXTURE_FRONTEND_CLOCK_MODE", std::to_wstring(scenario));
        Check(!deonline::CoreStartupReferenceTestAccess::ClockBeforeWithoutFrontend(worker, &anchor),
              "clock-before-view is an additive completed-owner policy, never an independent Start mode");
        Check(!worker.CaptureGuiFrontendStartupClockV2() && !worker.CaptureGuiFrontendStartupClockOwnerV2() && !worker.ReadGuiFrontendStartupClockV2(0) &&
                  !worker.ReadGuiFrontendStartupClockOwnerV2(0),
              "V2 clock and retained owner caches begin neutral");
        Check(Fast([&] { return worker.BeginGuiFrontendStartupReferenceV2(&anchor); }), "V2 queues the same asynchronous completed owner");
        Check(WaitForSingleObject(fixture.entered, 5000) == WAIT_OBJECT_0, "V2 fixture loaded on its worker");
        deleteObservation.Arm(fixture.counters);
        Check(!worker.BeginGuiFrontendStartupReference(&anchor) && !worker.BeginClockReference(&anchor) && !worker.BeginClockSetReference(&anchor),
              "pending V2 excludes historical view and standalone clock protocols");
        SetEvent(fixture.release);
        const auto deadline = GetTickCount64() + 10000;
        if (scenario == 2 || scenario == 13)
        {
            Wait(worker);
            Check(worker.Read(16) == 26 && !worker.Read(17) && !fixture.counters[33] && !fixture.counters[34] && !fixture.counters[35] &&
                      !worker.CaptureGuiFrontendStartupClockV2() && !worker.CaptureGuiFrontendStartupClockOwnerV2(),
                  "missing or wrong optional completed clock API refuses before Core Begin or callbacks");
            return;
        }
        const bool noPublication = scenario == 3 || scenario == 14 || scenario == 15 || scenario == 16 || scenario == 18;
        if (!noPublication && scenario != 19)
        {
            while (!Fast([&] { return worker.CaptureGuiFrontendStartupClockV2(); }))
            {
                Check(GetTickCount64() < deadline, "V2 clock publication did not reach the script cache");
                Sleep(5);
            }
            Check(worker.ReadGuiFrontendStartupClockV2(Clock::Version) == 1 && worker.ReadGuiFrontendStartupClockV2(Clock::Phase) == 1 &&
                      !worker.CaptureGuiFrontendStartupClockOwnerV2() && fixture.counters[35] == 0 && fixture.counters[36] == 1,
                  "clock executes before view or a post-clock owner read, with independently captured Read96");
            Check(!worker.CaptureClock() && !worker.ReadClock(0) && !worker.CaptureClockSet() && !worker.CaptureGuiFrontendInput(),
                  "V2 cannot expose its clock through historical Read/Set or premature GUI caches");
            bool        foreignRefused = false;
            std::thread foreign(
                [&]
                {
                    foreignRefused = !worker.CaptureGuiFrontendStartupClockV2() && !worker.CaptureGuiFrontendStartupClockOwnerV2() &&
                                     !worker.ReadGuiFrontendStartupClockV2(0) && !worker.ReadGuiFrontendStartupClockOwnerV2(0);
                });
            foreign.join();
            Check(foreignRefused, "V2 caches retain the script owner's foreign-thread refusal");
        }
        if (scenario == 1)
            Check(Fast([&] { return worker.Cancel(); }), "V2 cancellation remains a nonblocking request");
        if (scenario == 11)
            AcquireSRWLockShared(&deonline::CoreStartupReferenceTestAccess::InputLock(worker));
        SetEvent(fixture.inputRelease);
        if (!noPublication && scenario != 19)
        {
            while (!fixture.counters[21] && GetTickCount64() < deadline)
                Sleep(5);
            Check(fixture.counters[21] && !fixture.counters[35] && fixture.counters[36] == 1 && worker.IsPending(),
                  "pending native retirement admits neither view nor CoreRead32");
        }
        if (scenario == 11)
        {
            const bool progressed = ContendedClockProgress([&] { return worker.CaptureGuiFrontendStartupClockV2(); },
                                                           [&](auto field) { return worker.ReadGuiFrontendStartupClockV2(field); }, fixture.counters);
            ReleaseSRWLockShared(&deonline::CoreStartupReferenceTestAccess::InputLock(worker));
            Check(progressed, "clock publication/capture does not acquire the actual contended historical input cache lock");
        }
        SetEvent(fixture.movementRelease);
        const bool quarantined =
            scenario == 7 || scenario == 8 || scenario == 9 || scenario == 10 || scenario == 14 || scenario == 15 || scenario == 16 || scenario == 18;
        if (quarantined)
        {
            while (!fixture.counters[39] && GetTickCount64() < deadline)
                Sleep(5);
            const auto until = GetTickCount64() + 100;
            while (GetTickCount64() < until)
            {
                const auto polled = Fast([&] { return worker.Poll(); });
                const bool retained = polled == 0 && worker.IsPending() && GetModuleHandleW(L"core.dll") && !fixture.counters[1] && !fixture.counters[35] &&
                                      !fixture.counters[37] && !fixture.counters[38];
                if (!retained)
                {
                    std::fprintf(stderr, "Completed clock quarantine mismatch: poll=%d pending=%u core=%u counters=", polled, worker.IsPending() ? 1u : 0u,
                                 GetModuleHandleW(L"core.dll") ? 1u : 0u);
                    for (const auto field : {0u, 1u, 2u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u})
                        std::fprintf(stderr, "%u:%ld,", field, static_cast<long>(fixture.counters[field]));
                    std::fprintf(stderr, " common19=");
                    for (std::uint32_t field = 0; field != 19; ++field)
                        std::fprintf(stderr, "%s%u", field ? "," : "", worker.Read(field));
                    std::fprintf(stderr, "\n");
                }
                Check(retained, "unqualified clock return retains live worker/TLS/Core before owner read, view or shutdown");
                Sleep(5);
            }
            Check(fixture.counters[39] && !fixture.counters[26] && fixture.counters[36] == (scenario == 15 ? 2 : 1) &&
                      fixture.counters[40] == (scenario == 15 ? 1 : 0) && !worker.CaptureGuiFrontendStartupClockOwnerV2(),
                  "pending/thrown or malformed evidence quarantines before consumer heap unwind; only Failed preflight can probe owner32");
            return;
        }
        Wait(worker);
        const bool success = scenario == 0 || scenario == 11;
        const auto expectedError = scenario == 1 ? 12u : scenario == 22 ? 22u : scenario == 21 || scenario == 24 ? 23u : success ? 0u : 27u;
        const bool terminalMatches = worker.Read(13) == 1 && worker.Read(16) == expectedError && worker.Read(17) == (success ? 1u : 0u) &&
                                     fixture.counters[33] == 1 && fixture.counters[34] == 1 && fixture.counters[37] == 1 && fixture.counters[38] == 1 &&
                                     !fixture.counters[40] && !fixture.counters[2] && fixture.counters[1] == fixture.counters[0];
        if (!terminalMatches)
        {
            std::fprintf(stderr, "Completed clock terminal mismatch: expected_error=%u common19=", expectedError);
            for (std::uint32_t field = 0; field != 19; ++field)
                std::fprintf(stderr, "%s%u", field ? "," : "", worker.Read(field));
            std::fprintf(stderr, " counters=");
            for (const auto field : {0u, 1u, 2u, 33u, 34u, 35u, 36u, 37u, 38u, 40u})
                std::fprintf(stderr, "%u:%ld,", field, static_cast<long>(fixture.counters[field]));
            std::fprintf(stderr, "\n");
        }
        Check(terminalMatches, "both stages succeed only on one completed owner; every retired failure still shuts down/releases/unloads on its worker");
        Check(fixture.counters[35] == (success || scenario == 21 || scenario == 24 ? 1 : 0), "any failed clock stage skips GUI admission");
        if (success)
            Check(worker.CaptureGuiFrontendStartup() && worker.CaptureGuiFrontendStartupClockOwnerV2() &&
                      worker.ReadGuiFrontendStartup(Report::Begin + Startup::CapabilitiesLow) == 33 &&
                      worker.ReadGuiFrontendStartupClockOwnerV2(Startup::CapabilitiesLow) == 97 &&
                      worker.ReadGuiFrontendStartup(Report::AfterView + Startup::CapabilitiesLow) == 97,
                  "real Bind33-to97 adds exactly the native-pad capability and preserves it through the same-Core GUI");
        Check(
            worker.CaptureGuiFrontendStartup() &&
                worker.ReadGuiFrontendStartup(Report::Begin + Startup::OwnerThread) == worker.ReadGuiFrontendStartup(Report::Stopped + Startup::OwnerThread) &&
                worker.ReadGuiFrontendStartup(Report::Stopped + Startup::Phase) == 4 &&
                worker.ReadGuiFrontendStartup(Report::Stopped + Startup::CleanupComplete) == 1,
            "original lifecycle96 retains the same completed owner through stop independently of clock outcome");
        if (scenario == 24)
            Check(worker.CaptureGuiFrontendStartupClockOwnerV2() && worker.ReadGuiFrontendStartupClockOwnerV2(Startup::CapabilitiesLow) == 97 &&
                      worker.ReadGuiFrontendStartup(Report::AfterView + Startup::CapabilitiesLow) == 225 && !worker.Read(17),
                  "retired clock and successful GUI cannot certify final workload success after the borrowed backend capabilities change");
        if (scenario == 3)
        {
            Check(!worker.CaptureGuiFrontendStartupClockV2() && worker.CaptureGuiFrontendStartupClockOwnerV2() && !worker.Read(15) &&
                      worker.ReadGuiFrontendStartup(Report::AfterView + Startup::Phase) == 2 &&
                      worker.ReadGuiFrontendStartup(Report::AfterView + Startup::LastFailure) == static_cast<std::uint32_t>(Startup::Failure::InputBind) &&
                      !worker.ReadGuiFrontendStartup(Report::AfterView + Startup::ViewRuns),
                  "known Failed bind refusal records genuine owner32 without fabricating native96 or AdapterStopped");
        }
        else
        {
            Check(worker.CaptureGuiFrontendStartupClockV2(), "final clock snapshot remains available after same-Core GUI and unload");
            Clock::Snapshot sample{};
            for (std::uint32_t field = 0; field != Clock::FieldCount; ++field)
                sample[field] = worker.ReadGuiFrontendStartupClockV2(field);
            Check(DefinitiveFrontendClockReference::RetiredForCompletedOwner(sample), "every admitted cleanup independently retires the clock");
            Check(worker.Read(15) == (scenario == 19 ? 0u : 1u), "backend module accounting follows actual clock PIN, including safe no-Begin cancel");
            if (scenario != 22)
                Check(worker.CaptureGuiFrontendStartupClockOwnerV2() && worker.ReadGuiFrontendStartupClockOwnerV2(Startup::Phase) == 2 &&
                          worker.ReadGuiFrontendStartupClockOwnerV2(Startup::CoreRetained) == 1 &&
                          worker.ReadGuiFrontendStartupClockOwnerV2(Startup::BackendStarted) == 1 &&
                          !worker.ReadGuiFrontendStartupClockOwnerV2(Startup::ViewRuns) &&
                          worker.ReadGuiFrontendStartupClockOwnerV2(Startup::LastViewCleanupComplete) == 1,
                      "independent owner32 is captured after clock retirement and before the GUI view changes ViewRuns");
        }
        Check(!worker.ReadGuiFrontendStartupClockV2(Clock::FieldCount) && !worker.ReadGuiFrontendStartupClockOwnerV2(Startup::FieldCount),
              "V2 scalar cache bounds remain neutral");
    }

    void TestClockWorker(deonline::CoreStartupReference& worker, const std::filesystem::path& root, const std::wstring& mode,
                         const std::filesystem::path& coreFixture)
    {
        using namespace DefinitiveClockReference;
        const unsigned          scenario = mode == L"clock-cancel"                    ? 1u
                                           : mode == L"clock-wrong-api"               ? 2u
                                           : mode == L"clock-config-refused"          ? 3u
                                           : mode == L"clock-init-failed"             ? 4u
                                           : mode == L"clock-init-thrown"             ? 5u
                                           : mode == L"clock-cache-contention"        ? 6u
                                           : mode == L"clock-returned-pending"        ? 7u
                                           : mode == L"clock-thrown-pending"          ? 8u
                                           : mode == L"clock-stale-terminal"          ? 9u
                                           : mode == L"clock-retained-terminal"       ? 10u
                                           : mode == L"clock-workload-incomplete"     ? 11u
                                           : mode == L"clock-readback-thrown-pending" ? 12u
                                                                                      : 0u;
        Fixture                 fixture(200 + scenario);
        WorkerDeleteObservation deleteObservation;
        CopyCore(coreFixture, root);
        Set(L"MTA_CORE_FIXTURE_CLOCK_MODE", std::to_wstring(scenario));
        Check(!worker.CaptureClock() && !worker.ReadClock(Version), "clock cache starts unavailable without borrowing another protocol's values");
        Check(!deonline::CoreStartupReferenceTestAccess::ConflictingClockStart(worker, &anchor),
              "actual Start refuses mixed clock and movement modes before starting a worker");
        Check(Fast([&] { return worker.BeginClockReference(&anchor); }), "clock reference queues on the actual existing asynchronous worker");
        Check(WaitForSingleObject(fixture.entered, 5000) == WAIT_OBJECT_0, "clock controlled Core entered its loader on the worker");
        if (scenario == 8 || scenario == 12)
            deleteObservation.Arm(fixture.counters);
        Check(!worker.Begin(&anchor) && !worker.BeginInputReference(&anchor) && !worker.BeginMovementReference(&anchor) &&
                  !worker.BeginRenderReferenceV6(&anchor, 2) && !worker.BeginGuiStartupReference(&anchor) && !worker.BeginClockReference(&anchor) &&
                  !worker.BeginClockSetReference(&anchor),
              "clock reference excludes every other worker mode during its complete ownership lifetime");
        SetEvent(fixture.release);
        if (scenario == 2 || scenario == 3 || mode == L"clock-missing-export")
        {
            Wait(worker);
            Check(worker.Read(16) == (scenario == 3 ? 14u : 13u) && !worker.CaptureClock() && !worker.ReadClock(Version) && !fixture.counters[9] &&
                      fixture.counters[1] == fixture.counters[0],
                  "missing, wrong-version and refused clock configuration unload only unused Core without inventing clock evidence");
            return;
        }
        const auto deadline = GetTickCount64() + 10000;
        while (!Fast([&] { return worker.CaptureClock(); }))
        {
            Check(worker.Poll() == 0 && GetTickCount64() < deadline, "clock fixture failed to publish its independent atomic snapshot");
            Sleep(5);
        }
        Check(worker.ReadClock(Version) == ApiVersion && worker.ReadClock(Phase) == 1 && worker.ReadClock(ReadOnlyVerified) == 1 &&
                  worker.ReadClock(ReadSubmitted) == worker.ReadClock(ReadAcknowledged) && worker.ReadClock(ReadAcknowledged) == 3 &&
                  !worker.ReadClock(SetsLow) && !worker.ReadClock(FreezesLow) && !worker.ReadClock(UnfreezesLow) && !worker.ReadClock(ResetsLow) &&
                  worker.ReadClock(BackendPinnedExpected) == 1 && worker.ReadClock(NativeTimer) - worker.ReadClock(LastMinuteChange) == 100,
              "coherent clock capture preserves read-only command and independent backend PIN evidence without mixed native timer fields");
        Check(!worker.CaptureClockSet() && !worker.ReadClockSet(DefinitiveClockSetReference::Version) && !worker.CaptureInput() && !worker.CaptureMovement() &&
                  !worker.CaptureRenderV6() && !worker.ReadClock(FieldCount),
              "the separate 96-field clock cache cannot masquerade as historical input, movement or renderer snapshots");
        bool        foreignRefused = false;
        std::thread foreign(
            [&] { foreignRefused = !worker.CaptureClock() && !worker.ReadClock(Version) && !worker.BeginClockReference(&anchor) && worker.Poll() == -1; });
        foreign.join();
        Check(foreignRefused, "clock capture/read/start/poll reject a foreign script caller without touching Core exports");
        if (scenario == 1)
            Check(Fast([&] { return worker.Cancel(); }), "clock cancellation is a nonblocking request rather than unload permission");
        if (scenario == 6)
            AcquireSRWLockShared(&deonline::CoreStartupReferenceTestAccess::InputLock(worker));
        SetEvent(fixture.inputRelease);
        bool sawPending = false;
        while (GetTickCount64() < deadline && worker.Poll() == 0)
        {
            if (worker.CaptureClock() && worker.ReadClock(Phase) == 3 && fixture.counters[21])
            {
                sawPending = true;
                break;
            }
            Sleep(5);
        }
        if (scenario == 6)
        {
            const bool progressed =
                ContendedClockProgress([&] { return worker.CaptureClock(); }, [&](auto field) { return worker.ReadClock(field); }, fixture.counters);
            ReleaseSRWLockShared(&deonline::CoreStartupReferenceTestAccess::InputLock(worker));
            Check(progressed, "clock pending publication and capture never block behind the actual historical input cache lock");
        }
        Check(sawPending && GetModuleHandleW(L"core.dll") && !fixture.counters[1] && worker.ReadClock(CleanupPending) == 1,
              "clock cleanup retains its actual worker/Core owner and coherent pending evidence before final retirement");
        SetEvent(fixture.movementRelease);
        const bool quarantined = (scenario >= 7 && scenario <= 10) || scenario == 12;
        if (quarantined)
        {
            while (!fixture.counters[24] && GetTickCount64() < deadline)
                Sleep(5);
            const auto until = GetTickCount64() + 100;
            while (GetTickCount64() < until)
            {
                Check(Fast([&] { return worker.Poll(); }) == 0 && worker.IsPending() && GetModuleHandleW(L"core.dll") && !fixture.counters[1],
                      "a returned/thrown Initialize or malformed retirement proof cannot unload retained clock Core or terminate its worker");
                Sleep(5);
            }
            Check(fixture.counters[24] && worker.CaptureClock(), "faulty Initialize actually returned or threw after publishing its controlled clock evidence");
            if (scenario == 8 || scenario == 12)
                Check(fixture.counters[25] && !fixture.counters[26],
                      "pending Initialize/readback exceptions quarantine before destroying any consumer-owned worker heap objects");
            if (scenario == 12)
                Check(fixture.counters[6] == 1, "controlled pending exception occurs during first diagnostic readback after Initialize returned");
            Snapshot sample{};
            for (std::uint32_t field = 0; field != FieldCount; ++field)
                sample[field] = worker.ReadClock(field);
            Check(!Retired(sample) && !worker.BeginClockReference(&anchor),
                  "the actual terminal predicate refuses pending, stale-version or retained-peer evidence");
            // This disposable non-game host ends with its deliberately faulty
            // provider still quarantined. Production clock Run never returns
            // pending; the test does not force-unload its worker or callbacks.
            return;
        }
        Wait(worker);
        Check(worker.CaptureClock(), "final coherent clock evidence remains captured after actual Core unload");
        Snapshot sample{};
        for (std::uint32_t field = 0; field != FieldCount; ++field)
            sample[field] = worker.ReadClock(field);
        Check(Retired(sample) && sample[ReadOnlyVerified] && !sample[SetsLow] && !sample[FreezesLow] && !sample[UnfreezesLow] && !sample[ResetsLow] &&
                  sample[Reason] == static_cast<std::uint32_t>(scenario == 1 ? StopReason::Cancelled : StopReason::Completed),
              "read-only final clock proof retires native hook/data/peer ownership without changing the workload's cancellation outcome");
        Check(worker.Read(13) == 1 && worker.Read(15) == 1 &&
                  worker.Read(16) == (scenario == 1   ? 12u
                                      : scenario == 4 ? 7u
                                      : scenario == 5 ? 11u
                                                      : 0u) &&
                  fixture.counters[2] == 0 && fixture.counters[1] == fixture.counters[0],
              "failed or thrown Initialize still requires same-worker terminal retirement before actual unload and backend PIN accounting");
        Check(worker.Read(17) == (scenario == 0 || scenario == 6 ? 1u : 0u),
              "safe native retirement cannot promote an incomplete, cancelled, failed or thrown clock workload into startup success");
    }

    void TestClockSetWorker(deonline::CoreStartupReference& worker, const std::filesystem::path& root, const std::wstring& mode,
                            const std::filesystem::path& coreFixture)
    {
        using namespace DefinitiveClockSetReference;
        const unsigned          scenario = mode == L"clock-set-cancel"                    ? 1u
                                           : mode == L"clock-set-wrong-api"               ? 2u
                                           : mode == L"clock-set-config-refused"          ? 3u
                                           : mode == L"clock-set-init-failed"             ? 4u
                                           : mode == L"clock-set-init-thrown"             ? 5u
                                           : mode == L"clock-set-cache-contention"        ? 6u
                                           : mode == L"clock-set-returned-pending"        ? 7u
                                           : mode == L"clock-set-thrown-pending"          ? 8u
                                           : mode == L"clock-set-stale-terminal"          ? 9u
                                           : mode == L"clock-set-retained-terminal"       ? 10u
                                           : mode == L"clock-set-workload-incomplete"     ? 11u
                                           : mode == L"clock-set-readback-thrown-pending" ? 12u
                                           : mode == L"clock-set-corrupt-workload"        ? 13u
                                           : mode == L"clock-set-not-entered"             ? 14u
                                           : mode == L"clock-set-reserved-terminal"       ? 15u
                                           : mode == L"clock-set-wrong-count"             ? 16u
                                                                                          : 0u;
        Fixture                 fixture(240 + scenario);
        WorkerDeleteObservation deleteObservation;
        CopyCore(coreFixture, root);
        Set(L"MTA_CORE_FIXTURE_CLOCK_SET_MODE", std::to_wstring(scenario));
        Check(!worker.CaptureClockSet() && !worker.ReadClockSet(Version), "clock cache starts unavailable without borrowing another protocol's values");
        Check(!deonline::CoreStartupReferenceTestAccess::ConflictingClockSetStart(worker, &anchor),
              "actual Start refuses mixed read-only and Set clock modes before starting a worker");
        Check(Fast([&] { return worker.BeginClockSetReference(&anchor); }), "clock reference queues on the actual existing asynchronous worker");
        Check(WaitForSingleObject(fixture.entered, 5000) == WAIT_OBJECT_0, "clock controlled Core entered its loader on the worker");
        if (scenario == 8 || scenario == 12)
            deleteObservation.Arm(fixture.counters);
        Check(!worker.Begin(&anchor) && !worker.BeginInputReference(&anchor) && !worker.BeginMovementReference(&anchor) &&
                  !worker.BeginRenderReferenceV6(&anchor, 2) && !worker.BeginGuiStartupReference(&anchor) && !worker.BeginClockReference(&anchor) &&
                  !worker.BeginClockSetReference(&anchor),
              "clock reference excludes every other worker mode during its complete ownership lifetime");
        SetEvent(fixture.release);
        if (scenario == 2 || scenario == 3 || mode == L"clock-set-missing-export")
        {
            Wait(worker);
            Check(worker.Read(16) == (scenario == 3 ? 14u : 13u) && !worker.CaptureClockSet() && !worker.ReadClockSet(Version) && !fixture.counters[9] &&
                      fixture.counters[1] == fixture.counters[0],
                  "missing, wrong-version and refused clock configuration unload only unused Core without inventing clock evidence");
            return;
        }
        const auto deadline = GetTickCount64() + 10000;
        if (scenario == 14)
        {
            Wait(worker);
            Check(worker.CaptureClockSet(), "explicit Set non-entry evidence remains available after unused Core unload");
            Snapshot sample{};
            for (std::uint32_t field = 0; field != FieldCount; ++field)
                sample[field] = worker.ReadClockSet(field);
            Check(Retired(sample) && !Completed(sample) && sample[NotEntered] && sample[AdapterStopped] && !sample[BeginAttempted] && !sample[SetSubmitted] &&
                      !sample[SetAcknowledged] && !sample[NativeRestored] && !sample[BackendPinnedExpected] && !worker.Read(17) && worker.Read(13) == 1 &&
                      !worker.Read(15) && !fixture.counters[2] && fixture.counters[1] == fixture.counters[0],
                  "safe preflight non-entry permits unload without inventing native restoration, a write or Set workload success");
            return;
        }
        if (scenario == 16)
        {
            // A wrong-width provider must never populate either public cache.
            // Its return cannot prove cleanup when no valid report arrived.
            SetEvent(fixture.inputRelease);
            SetEvent(fixture.movementRelease);
            while (!fixture.counters[24] && GetTickCount64() < deadline)
                Sleep(5);
            Check(fixture.counters[24], "wrong-width Set provider actually returned from Initialize");
            const auto until = GetTickCount64() + 100;
            while (GetTickCount64() < until)
            {
                Check(Fast([&] { return worker.Poll(); }) == 0 && worker.IsPending() && GetModuleHandleW(L"core.dll") && !fixture.counters[1] &&
                          !worker.CaptureClockSet() && !worker.CaptureClock() && !worker.ReadClockSet(Version) && !worker.BeginClockSetReference(&anchor),
                      "wrong-width Set reports stay unavailable and cannot authorize Core unload or a replacement worker");
                Sleep(5);
            }
            return;
        }
        while (!Fast([&] { return worker.CaptureClockSet(); }))
        {
            Check(worker.Poll() == 0 && GetTickCount64() < deadline, "clock fixture failed to publish its independent atomic snapshot");
            Sleep(5);
        }
        Check(worker.ReadClockSet(Version) == ApiVersion && worker.ReadClockSet(Phase) == 1 && !worker.ReadClockSet(ReadOnlyVerified) &&
                  worker.ReadClockSet(ReadSubmitted) == worker.ReadClockSet(ReadAcknowledged) && worker.ReadClockSet(ReadAcknowledged) == 3 &&
                  worker.ReadClockSet(SetsLow) == 1 && worker.ReadClockSet(SetSubmitted) == 1 && worker.ReadClockSet(SetAcknowledged) == 1 &&
                  worker.ReadClockSet(CoherentSetReadback) == 1 && worker.ReadClockSet(TargetHour) == 6 && worker.ReadClockSet(TargetMinute) == 15 &&
                  !worker.ReadClockSet(FreezesLow) && !worker.ReadClockSet(UnfreezesLow) && !worker.ReadClockSet(ResetsLow) &&
                  worker.ReadClockSet(BackendPinnedExpected) == 1 && worker.ReadClockSet(NativeTimer) - worker.ReadClockSet(LastMinuteChange) == 100,
              "coherent Set capture preserves its explicit write/readback facts and independent backend PIN without relabelling read-only proof");
        Check(!worker.CaptureClock() && !worker.ReadClock(DefinitiveClockReference::Version) && !worker.CaptureInput() && !worker.CaptureMovement() &&
                  !worker.CaptureRenderV6() && !worker.ReadClockSet(FieldCount),
              "the separate 128-field Set cache cannot masquerade as the historical read-only clock or other protocols");
        bool        foreignRefused = false;
        std::thread foreign(
            [&]
            { foreignRefused = !worker.CaptureClockSet() && !worker.ReadClockSet(Version) && !worker.BeginClockSetReference(&anchor) && worker.Poll() == -1; });
        foreign.join();
        Check(foreignRefused, "clock capture/read/start/poll reject a foreign script caller without touching Core exports");
        if (scenario == 1)
            Check(Fast([&] { return worker.Cancel(); }), "clock cancellation is a nonblocking request rather than unload permission");
        if (scenario == 6)
            AcquireSRWLockShared(&deonline::CoreStartupReferenceTestAccess::InputLock(worker));
        SetEvent(fixture.inputRelease);
        bool sawPending = false;
        while (GetTickCount64() < deadline && worker.Poll() == 0)
        {
            if (worker.CaptureClockSet() && worker.ReadClockSet(Phase) == 3 && fixture.counters[21])
            {
                sawPending = true;
                break;
            }
            Sleep(5);
        }
        if (scenario == 6)
        {
            const bool progressed =
                ContendedClockProgress([&] { return worker.CaptureClockSet(); }, [&](auto field) { return worker.ReadClockSet(field); }, fixture.counters);
            ReleaseSRWLockShared(&deonline::CoreStartupReferenceTestAccess::InputLock(worker));
            Check(progressed, "clock pending publication and capture never block behind the actual historical input cache lock");
        }
        Check(sawPending && GetModuleHandleW(L"core.dll") && !fixture.counters[1] && worker.ReadClockSet(CleanupPending) == 1,
              "clock cleanup retains its actual worker/Core owner and coherent pending evidence before final retirement");
        SetEvent(fixture.movementRelease);
        const bool quarantined = (scenario >= 7 && scenario <= 10) || scenario == 12;
        if (quarantined)
        {
            while (!fixture.counters[24] && GetTickCount64() < deadline)
                Sleep(5);
            const auto until = GetTickCount64() + 100;
            while (GetTickCount64() < until)
            {
                Check(Fast([&] { return worker.Poll(); }) == 0 && worker.IsPending() && GetModuleHandleW(L"core.dll") && !fixture.counters[1],
                      "a returned/thrown Initialize or malformed retirement proof cannot unload retained clock Core or terminate its worker");
                Sleep(5);
            }
            Check(fixture.counters[24] && worker.CaptureClockSet(),
                  "faulty Initialize actually returned or threw after publishing its controlled clock evidence");
            if (scenario == 8 || scenario == 12)
                Check(fixture.counters[25] && !fixture.counters[26],
                      "pending Initialize/readback exceptions quarantine before destroying any consumer-owned worker heap objects");
            if (scenario == 12)
                Check(fixture.counters[6] == 1, "controlled pending exception occurs during first diagnostic readback after Initialize returned");
            Snapshot sample{};
            for (std::uint32_t field = 0; field != FieldCount; ++field)
                sample[field] = worker.ReadClockSet(field);
            Check(!Retired(sample) && !worker.BeginClockSetReference(&anchor),
                  "the actual terminal predicate refuses pending, stale-version or retained-peer evidence");
            // This disposable non-game host ends with its deliberately faulty
            // provider still quarantined. Production clock Run never returns
            // pending; the test does not force-unload its worker or callbacks.
            return;
        }
        Wait(worker);
        Check(worker.CaptureClockSet(), "final coherent clock evidence remains captured after actual Core unload");
        Snapshot sample{};
        for (std::uint32_t field = 0; field != FieldCount; ++field)
            sample[field] = worker.ReadClockSet(field);
        Check(Retired(sample) && !sample[ReadOnlyVerified] && sample[SetsLow] == 1 && !sample[FreezesLow] && !sample[UnfreezesLow] && !sample[ResetsLow] &&
                  sample[Reason] == static_cast<std::uint32_t>(scenario == 1 ? StopReason::Cancelled : StopReason::Completed),
              "final Set retirement remains independent of cancellation and workload success");
        Check(worker.Read(13) == 1 && worker.Read(15) == 1 &&
                  worker.Read(16) == (scenario == 1   ? 12u
                                      : scenario == 4 ? 7u
                                      : scenario == 5 ? 11u
                                                      : 0u) &&
                  fixture.counters[2] == 0 && fixture.counters[1] == fixture.counters[0],
              "failed or thrown Initialize still requires same-worker terminal retirement before actual unload and backend PIN accounting");
        Check(worker.Read(17) == (scenario == 0 || scenario == 6 ? 1u : 0u),
              "safe native retirement cannot promote an incomplete, cancelled, failed or thrown clock workload into startup success");
    }

    void TestGuiWorker(deonline::CoreStartupReference& worker, const std::filesystem::path& root, const std::wstring& mode,
                       const std::filesystem::path& coreFixture)
    {
        using namespace DefinitiveGuiStartupReference;
        const unsigned scenario = mode == L"gui-workload-failed"        ? 1u
                                  : mode == L"gui-cancel"               ? 2u
                                  : mode == L"gui-wrong-api"            ? 3u
                                  : mode == L"gui-config-refused"       ? 4u
                                  : mode == L"gui-mixed-version"        ? 5u
                                  : mode == L"gui-cache-contention"     ? 6u
                                  : mode == L"gui-deadline"             ? 7u
                                  : mode == L"gui-crosswire"            ? 8u
                                  : mode == L"gui-cleanup-incomplete"   ? 9u
                                  : mode == L"gui-retained"             ? 10u
                                  : mode == L"gui-unpinned"             ? 11u
                                  : mode == L"gui-device-release-fault" ? 12u
                                                                        : 0u;
        Fixture        fixture(120 + scenario);
        CopyCore(coreFixture, root);
        Set(L"MTA_CORE_FIXTURE_GUI_MODE", std::to_wstring(scenario));
        Check(!worker.CaptureGuiStartup() && !worker.ReadGuiStartup(Version), "fresh GUI cache is unavailable and neutral");
        Check(!deonline::CoreStartupReferenceTestAccess::ConflictingGuiStart(worker, &anchor, true) &&
                  !deonline::CoreStartupReferenceTestAccess::ConflictingGuiStart(worker, &anchor, false),
              "actual Start rejects mixed GUI/movement and GUI/render flags before creating a worker");
        Check(Fast([&] { return worker.BeginGuiStartupReference(&anchor); }), "explicit GUI worker job must queue without loader work on script thread");
        Check(WaitForSingleObject(fixture.entered, 5000) == WAIT_OBJECT_0, "GUI fixture loader did not enter on worker");
        Check(!worker.Begin(&anchor) && !worker.BeginInputReference(&anchor) && !worker.BeginMovementReference(&anchor) &&
                  !worker.BeginRenderReferenceV6(&anchor, 2) && !worker.BeginGuiStartupReference(&anchor),
              "GUI job excludes ordinary, input, movement and every render lifetime");
        SetEvent(fixture.release);
        const bool configurationFailure = scenario == 3 || scenario == 4 || mode == L"gui-missing-export";
        if (configurationFailure)
        {
            Wait(worker);
            Check(worker.Read(16) == (scenario == 4 ? 14u : 13u) && !worker.Read(17) && !worker.CaptureGuiStartup() && !worker.ReadGuiStartup(Version),
                  "missing/wrong-version/refused GUI exports never substitute an old configuration or invent a cache");
            Check(!fixture.counters[9] && fixture.counters[2] == 0 && worker.Read(13) == 1 && worker.Read(15) == 0,
                  "GUI configuration refusal unloads its unused owned Core with no worker publication");
            return;
        }
        auto deadline = GetTickCount64() + 10000;
        while (!Fast([&] { return worker.CaptureGuiStartup(); }))
        {
            Check(worker.Poll() == 0 && GetTickCount64() < deadline, "GUI fixture failed to publish its first POD snapshot");
            Sleep(5);
        }
        Check(worker.ReadGuiStartup(Version) == 1 && worker.ReadGuiStartup(Phase) == 1 && worker.ReadGuiStartup(Sequence) == 1 &&
                  worker.ReadGuiStartup(LeaseGenerationHigh) == 2 && worker.ReadGuiStartup(AcceptedSnapshotsHigh) == 1 &&
                  worker.ReadGuiStartup(DispatchEntriesHigh) == 1 && worker.ReadGuiStartup(SourceProfile) == 2 &&
                  worker.ReadGuiStartup(LeaseMetadataValid) == 1,
              "GUI64 captures distinct split-counter provenance without reusing API6 or input fields");
        Check(!worker.CaptureInput() && !worker.ReadInput(0) && !worker.CaptureMovement() && !worker.ReadMovement(0) && !worker.CaptureRender() &&
                  !worker.CaptureRenderV2() && !worker.CaptureRenderV3() && !worker.CaptureRenderV4() && !worker.CaptureRenderV5() &&
                  !worker.CaptureRenderV6() && !worker.ReadRenderV6(0),
              "GUI POD publication never appears through an older input, movement or renderer cache");
        bool        foreignRejected = false;
        std::thread foreign(
            [&]
            {
                foreignRejected = !worker.CaptureGuiStartup() && !worker.ReadGuiStartup(0) && !worker.BeginGuiStartupReference(&anchor) && worker.Poll() == -1;
            });
        foreign.join();
        Check(foreignRejected, "GUI capture/read/Begin/completion remain bound to the script owner thread");
        if (scenario == 2)
            Check(Fast([&] { return worker.Cancel(); }), "GUI cancellation remains nonblocking on script thread");
        SRWLOCK* heldCache = scenario == 6 ? &deonline::CoreStartupReferenceTestAccess::InputLock(worker) : nullptr;
        if (heldCache)
            AcquireSRWLockShared(heldCache);
        SetEvent(fixture.inputRelease);
        if (heldCache)
        {
            const auto started = GetTickCount64();
            while (!fixture.counters[21] && GetTickCount64() - started < 500)
                Sleep(1);
            const bool returned = fixture.counters[21] != 0;
            const bool dropped = deonline::CoreStartupReferenceTestAccess::PublishedGuiPhase(worker) == 1;
            ReleaseSRWLockShared(heldCache);
            Check(returned && dropped && worker.ReadGuiStartup(Phase) == 1,
                  "actual GUI Phase3/CleanupPending publication drops under cache contention instead of blocking cleanup");
        }
        while (worker.ReadGuiStartup(Phase) != 3)
        {
            Check(worker.Poll() == 0 && GetTickCount64() < deadline, "GUI cleanup must retain its worker and Core owner");
            worker.CaptureGuiStartup();
            Sleep(5);
        }
        Check(worker.ReadGuiStartup(CleanupPending) == 1 && worker.ReadGuiStartup(DeviceRetained) == 1 && GetModuleHandleW(L"core.dll"),
              "pending GUI cleanup retains ownership while final success is unavailable");
        if (scenario == 7)
        {
            while (!worker.ReadGuiStartup(CleanupDeadlineExceeded))
            {
                Check(worker.Poll() == 0 && GetTickCount64() < deadline, "GUI deadline must not authorize unsafe worker unload");
                worker.CaptureGuiStartup();
                Sleep(10);
            }
            Check(GetModuleHandleW(L"core.dll") != nullptr, "GUI deadline preserves Core until actual cleanup completes");
        }
        SetEvent(fixture.movementRelease);
        Wait(worker);
        Check(worker.ReadGuiStartup(Phase) == 3, "worker completion cannot mutate an owner-captured GUI sample");
        Check(worker.CaptureGuiStartup() && worker.ReadGuiStartup(Phase) == 2 && worker.ReadGuiStartup(CleanupPending) == 0 &&
                  worker.ReadGuiStartup(ObserverPendingCleanup) == 0 && worker.ReadGuiStartup(SessionRetained) == 0 &&
                  worker.ReadGuiStartup(DeviceRetained) == 0,
              "reliable final GUI publication survives worker completion and Core unload");
        const bool malformed = scenario == 5 || scenario == 7 || scenario == 9 || scenario == 10 || scenario == 12;
        const auto expectedError = scenario == 2 ? 12u : malformed ? 15u : 0u;
        Check(worker.Read(16) == expectedError && worker.Read(17) == (expectedError ? 0u : 1u) && worker.Read(13) == 1 &&
                  worker.Read(15) == (scenario == 11 ? 0u : 1u) && worker.ReadGuiStartup(BackendPinnedExpected) == worker.Read(15),
              "GUI lifecycle result validates explicit backend pin and cleanup independently of workload outcome");
        Check(worker.ReadGuiStartup(WorkloadComplete) == 1 && worker.ReadGuiStartup(WorkloadSucceeded) == (scenario == 1 ? 0u : 1u) &&
                  worker.ReadGuiStartup(WorkloadHresult) == (scenario == 1 ? static_cast<std::uint32_t>(E_FAIL) : 0u),
              "a failed GUI workload with complete cleanup remains distinguishable from worker lifecycle failure");
        Check(!worker.ReadGuiStartup(NativeQueueCommandsIssued) && !worker.ReadGuiStartup(NativeBackbufferTouched),
              "GUI fixture transport never claims native queue commands or backbuffer access");
        Check(worker.ReadGuiStartup(LeaseMetadataValid) == 1 && worker.ReadGuiStartup(DeviceReleaseFault) == (scenario == 12 ? 1u : 0u),
              "device Release fault remains visible and prevents lifecycle success independently of cleared retention bits");
        for (unsigned field = DeviceReleaseFault + 1; field < FieldCount; ++field)
            Check(worker.ReadGuiStartup(field) == 0, "GUI64 reserved tail remains neutral");
        Check(
            !worker.ReadGuiStartup(FieldCount) && !worker.ReadGuiStartup(0xffffffffu) && fixture.counters[2] == 0 && fixture.counters[1] == fixture.counters[0],
            "GUI bounds are neutral and configure/publication/unload all remain on worker thread");
        Check(worker.Begin(&anchor) && !worker.CaptureGuiStartup() && !worker.ReadGuiStartup(Version),
              "a newly allowed ordinary job clears GUI captured/published availability before any new work");
        Wait(worker);
    }
    void TestGuiSurfaceWorker(deonline::CoreStartupReference& worker, const std::filesystem::path& root, const std::wstring& mode,
                              const std::filesystem::path& coreFixture)
    {
        using namespace DefinitiveGuiStartupReference;
        const unsigned scenario = mode == L"surface-workload-failed"          ? 1u
                                  : mode == L"surface-cancel"                 ? 2u
                                  : mode == L"surface-wrong-api"              ? 3u
                                  : mode == L"surface-config-refused"         ? 4u
                                  : mode == L"surface-mixed-version"          ? 5u
                                  : mode == L"surface-cache-contention"       ? 6u
                                  : mode == L"surface-deadline"               ? 7u
                                  : mode == L"surface-crosswire"              ? 8u
                                  : mode == L"surface-cleanup-incomplete"     ? 9u
                                  : mode == L"surface-retained"               ? 10u
                                  : mode == L"surface-unpinned"               ? 11u
                                  : mode == L"surface-device-release-fault"   ? 12u
                                  : mode == L"surface-child-retained"         ? 13u
                                  : mode == L"surface-queue-retained"         ? 14u
                                  : mode == L"surface-surface-retained"       ? 15u
                                  : mode == L"surface-window-incomplete"      ? 16u
                                  : mode == L"surface-composition-incomplete" ? 17u
                                  : mode == L"surface-class-incomplete"       ? 18u
                                  : mode == L"surface-observer-incomplete"    ? 19u
                                  : mode == L"surface-gui-incomplete"         ? 20u
                                  : mode == L"surface-shutdown-busy"          ? 21u
                                  : mode == L"surface-resize-pending"         ? 22u
                                                                              : 0u;
        Fixture        fixture(160 + scenario);
        CopyCore(coreFixture, root);
        Set(L"MTA_CORE_FIXTURE_SURFACE_MODE", std::to_wstring(scenario));
        Check(!worker.CaptureGuiSurface() && !worker.ReadGuiSurface(Version), "fresh surface cache is unavailable and neutral");
        Check(!deonline::CoreStartupReferenceTestAccess::ConflictingSurfaceStart(worker, &anchor, 0) &&
                  !deonline::CoreStartupReferenceTestAccess::ConflictingSurfaceStart(worker, &anchor, 1) &&
                  !deonline::CoreStartupReferenceTestAccess::ConflictingSurfaceStart(worker, &anchor, 2),
              "actual Start rejects mixed surface/movement and surface/render flags before creating a worker");
        Check(Fast([&] { return worker.BeginGuiSurfaceReference(&anchor); }), "explicit surface worker job must queue without loader work on script thread");
        Check(WaitForSingleObject(fixture.entered, 5000) == WAIT_OBJECT_0, "surface fixture loader did not enter on worker");
        Check(!worker.Begin(&anchor) && !worker.BeginInputReference(&anchor) && !worker.BeginMovementReference(&anchor) &&
                  !worker.BeginRenderReferenceV6(&anchor, 2) && !worker.BeginGuiSurfaceReference(&anchor) && !worker.BeginGuiStartupReference(&anchor),
              "surface job excludes ordinary, input, movement and every render lifetime");
        SetEvent(fixture.release);
        const bool configurationFailure = scenario == 3 || scenario == 4 || mode == L"surface-missing-export";
        if (configurationFailure)
        {
            Wait(worker);
            Check(worker.Read(16) == (scenario == 4 ? 14u : 13u) && !worker.Read(17) && !worker.CaptureGuiSurface() && !worker.ReadGuiSurface(Version),
                  "missing/wrong-version/refused surface exports never substitute an old configuration or invent a cache");
            Check(!fixture.counters[9] && fixture.counters[2] == 0 && worker.Read(13) == 1 && worker.Read(15) == 0,
                  "surface configuration refusal unloads its unused owned Core with no worker publication");
            return;
        }
        auto deadline = GetTickCount64() + 10000;
        while (!Fast([&] { return worker.CaptureGuiSurface(); }))
        {
            Check(worker.Poll() == 0 && GetTickCount64() < deadline, "surface fixture failed to publish its first POD snapshot");
            Sleep(5);
        }
        Check(worker.ReadGuiSurface(Version) == 1 && worker.ReadGuiSurface(Phase) == 1 && worker.ReadGuiSurface(Sequence) == 1 &&
                  worker.ReadGuiSurface(LeaseGenerationHigh) == 2 && worker.ReadGuiSurface(AcceptedSnapshotsHigh) == 1 &&
                  worker.ReadGuiSurface(DispatchEntriesHigh) == 1 && worker.ReadGuiSurface(SourceProfile) == 2 &&
                  worker.ReadGuiSurface(LeaseMetadataValid) == 1,
              "surface96 captures distinct split-counter provenance without reusing API6 or input fields");
        Check(!worker.CaptureInput() && !worker.ReadInput(0) && !worker.CaptureMovement() && !worker.ReadMovement(0) && !worker.CaptureRender() &&
                  !worker.CaptureRenderV2() && !worker.CaptureRenderV3() && !worker.CaptureRenderV4() && !worker.CaptureRenderV5() &&
                  !worker.CaptureRenderV6() && !worker.ReadRenderV6(0) && !worker.CaptureGuiStartup() && !worker.ReadGuiStartup(0),
              "surface POD publication never appears through an older input, movement or renderer cache");
        bool        foreignRejected = false;
        std::thread foreign(
            [&]
            {
                foreignRejected = !worker.CaptureGuiSurface() && !worker.ReadGuiSurface(0) && !worker.BeginGuiSurfaceReference(&anchor) && worker.Poll() == -1;
            });
        foreign.join();
        Check(foreignRejected, "surface capture/read/Begin/completion remain bound to the script owner thread");
        if (scenario == 2)
            Check(Fast([&] { return worker.Cancel(); }), "surface cancellation remains nonblocking on script thread");
        SRWLOCK* heldCache = scenario == 6 ? &deonline::CoreStartupReferenceTestAccess::InputLock(worker) : nullptr;
        if (heldCache)
            AcquireSRWLockShared(heldCache);
        SetEvent(fixture.inputRelease);
        if (heldCache)
        {
            const auto started = GetTickCount64();
            while (!fixture.counters[21] && GetTickCount64() - started < 500)
                Sleep(1);
            const bool returned = fixture.counters[21] != 0;
            const bool dropped = deonline::CoreStartupReferenceTestAccess::PublishedSurfacePhase(worker) == 1;
            ReleaseSRWLockShared(heldCache);
            Check(returned && dropped && worker.ReadGuiSurface(Phase) == 1,
                  "actual surface Phase3/CleanupPending publication drops under cache contention instead of blocking cleanup");
        }
        while (worker.ReadGuiSurface(Phase) != 3)
        {
            Check(worker.Poll() == 0 && GetTickCount64() < deadline, "surface cleanup must retain its worker and Core owner");
            worker.CaptureGuiSurface();
            Sleep(5);
        }
        Check(worker.ReadGuiSurface(CleanupPending) == 1 && worker.ReadGuiSurface(DeviceRetained) == 1 && GetModuleHandleW(L"core.dll"),
              "pending surface cleanup retains ownership while final success is unavailable");
        Check(worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::ChildRetained) == 1 &&
                  worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::QueueRetained) == 1 &&
                  worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::SurfaceRetained) == 1,
              "surface phase3 keeps HWND, own queue and composition retention separate from GUI64 fields");
        if (scenario == 7)
        {
            while (!worker.ReadGuiSurface(CleanupDeadlineExceeded))
            {
                Check(worker.Poll() == 0 && GetTickCount64() < deadline, "surface deadline must not authorize unsafe worker unload");
                worker.CaptureGuiSurface();
                Sleep(10);
            }
            Check(GetModuleHandleW(L"core.dll") != nullptr, "surface deadline preserves Core until actual cleanup completes");
        }
        SetEvent(fixture.movementRelease);
        Wait(worker);
        Check(worker.ReadGuiSurface(Phase) == 3, "worker completion cannot mutate an owner-captured surface sample");
        Check(worker.CaptureGuiSurface() && worker.ReadGuiSurface(Phase) == 2 && worker.ReadGuiSurface(CleanupPending) == 0 &&
                  worker.ReadGuiSurface(ObserverPendingCleanup) == 0 && worker.ReadGuiSurface(SessionRetained) == 0 &&
                  worker.ReadGuiSurface(DeviceRetained) == 0,
              "reliable final surface publication survives worker completion and Core unload");
        const bool malformed = scenario == 5 || scenario == 7 || scenario == 9 || scenario == 10 || scenario == 12 || scenario >= 13;
        const auto expectedError = scenario == 2 ? 12u : malformed ? 15u : 0u;
        Check(worker.Read(16) == expectedError && worker.Read(17) == (expectedError ? 0u : 1u) && worker.Read(13) == 1 &&
                  worker.Read(15) == (scenario == 11 ? 0u : 1u) && worker.ReadGuiSurface(BackendPinnedExpected) == worker.Read(15),
              "surface lifecycle result validates explicit backend pin and cleanup independently of workload outcome");
        Check(worker.ReadGuiSurface(WorkloadComplete) == 1 && worker.ReadGuiSurface(WorkloadSucceeded) == (scenario == 1 ? 0u : 1u) &&
                  worker.ReadGuiSurface(WorkloadHresult) == (scenario == 1 ? static_cast<std::uint32_t>(E_FAIL) : 0u),
              "a failed surface workload with complete cleanup remains distinguishable from worker lifecycle failure");
        Check(!worker.ReadGuiSurface(NativeQueueCommandsIssued) && !worker.ReadGuiSurface(NativeBackbufferTouched),
              "surface fixture transport never claims native queue commands or backbuffer access");
        Check(worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::ChildRetained) == (scenario == 13 ? 1u : 0u) &&
                  worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::QueueRetained) == (scenario == 14 ? 1u : 0u) &&
                  worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::SurfaceRetained) == (scenario == 15 ? 1u : 0u) &&
                  worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::WindowCleanupComplete) == (scenario == 16 ? 0u : 1u) &&
                  worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::CompositionCleanupComplete) == (scenario == 17 ? 0u : 1u) &&
                  worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::ClassUnregistered) == (scenario == 18 ? 0u : 1u),
              "surface-only retained HWND/queue/composition and all three cleanup bits remain independently authoritative");
        Check(worker.ReadGuiSurface(ObserverEnded) == (scenario == 19 ? 0u : 1u) && worker.ReadGuiSurface(GuiCleanupComplete) == (scenario == 20 ? 0u : 1u) &&
                  worker.ReadGuiSurface(GuiShutdownStatus) == (scenario == 21 ? 1u : 0u) &&
                  worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::ResizePending) == (scenario == 22 ? 1u : 0u),
              "surface completion requires ended observer, completed GUI shutdown, Ok status and no pending resize");
        Check(worker.ReadGuiSurface(LeaseMetadataValid) == 1 && worker.ReadGuiSurface(DeviceReleaseFault) == (scenario == 12 ? 1u : 0u),
              "device Release fault remains visible and prevents lifecycle success independently of cleared retention bits");
        for (unsigned field = 87; field < DefinitiveGuiSurfaceReference::FieldCount; ++field)
            Check(worker.ReadGuiSurface(field) == 0, "surface96 reserved tail remains neutral");
        Check(!worker.ReadGuiSurface(DefinitiveGuiSurfaceReference::FieldCount) && !worker.ReadGuiSurface(0xffffffffu) && fixture.counters[2] == 0 &&
                  fixture.counters[1] == fixture.counters[0],
              "surface bounds are neutral and configure/publication/unload all remain on worker thread");
        Check(worker.Begin(&anchor) && !worker.CaptureGuiSurface() && !worker.ReadGuiSurface(Version),
              "a newly allowed ordinary job clears surface captured/published availability before any new work");
        Wait(worker);
    }
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
        if (std::wstring(argv[2]).find(L"runtime-") == 0)
        {
            Check(argc == 4, "runtime worker fixture requires its controlled Core DLL");
            TestRuntimeWorker(worker, root, argv[2], argv[3]);
        }
        else if (std::wstring(argv[2]).find(L"frontend-clock") == 0)
        {
            Check(argc == 4, "completed clock worker fixture requires its controlled Core DLL");
            TestFrontendClockWorker(worker, root, argv[2], argv[3]);
        }
        else if (std::wstring(argv[2]).find(L"clock-set") == 0)
        {
            Check(argc == 4, "Set worker fixture requires its controlled Core DLL");
            TestClockSetWorker(worker, root, argv[2], argv[3]);
        }
        else if (std::wstring(argv[2]).find(L"clock") == 0)
        {
            Check(argc == 4, "clock worker fixture requires its controlled Core DLL");
            TestClockWorker(worker, root, argv[2], argv[3]);
        }
        else if (std::wstring(argv[2]).find(L"surface") == 0)
        {
            Check(argc == 4, "surface worker fixture requires its controlled Core DLL");
            TestGuiSurfaceWorker(worker, root, argv[2], argv[3]);
        }
        else if (std::wstring(argv[2]).find(L"gui") == 0)
        {
            Check(argc == 4, "GUI worker fixture requires its controlled Core DLL");
            TestGuiWorker(worker, root, argv[2], argv[3]);
        }
        else if (std::wstring(argv[2]).find(L"render") == 0)
        {
            using namespace DefinitiveRenderReference;
            Check(argc == 4, "render worker fixture requires its controlled Core DLL");
            const std::wstring mode = argv[2];
            const bool         v6 = mode.find(L"render-v6") == 0;
            const bool         v5 = mode.find(L"render-v5") == 0;
            const bool         v4 = mode.find(L"render-v4") == 0;
            const bool         v3 = mode.find(L"render-v3") == 0;
            const bool         v2 = mode.find(L"render-v2") == 0;
            const bool         explicitRoute = v2 || v3 || v4 || v5 || v6;
            const bool         cacheContention = mode.find(L"cache-contention") != std::wstring::npos;
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
            Check(!worker.BeginRenderReferenceV5(&anchor, 0) && !worker.BeginRenderReferenceV5(&anchor, 1) && !worker.BeginRenderReferenceV5(&anchor, 3) &&
                      !worker.BeginRenderReferenceV5(&anchor, 0xffffffffu),
                  "API5 must explicitly select D3D12 before scheduling Present hooks");
            Check(v6   ? worker.BeginRenderReferenceV6(&anchor, route)
                  : v5 ? worker.BeginRenderReferenceV5(&anchor, route)
                  : v4 ? worker.BeginRenderReferenceV4(&anchor, route)
                  : v3 ? worker.BeginRenderReferenceV3(&anchor, route)
                  : v2 ? worker.BeginRenderReferenceV2(&anchor, route)
                       : worker.BeginRenderReference(&anchor),
                  "Core render worker failed to begin");
            Check(!worker.BeginInputReference(&anchor) && !worker.BeginMovementReference(&anchor), "render job must exclude input and movement lifetimes");
            Check(!worker.BeginRenderReference(&anchor) && !worker.BeginRenderReferenceV2(&anchor, route) && !worker.BeginRenderReferenceV3(&anchor, 2) &&
                      !worker.BeginRenderReferenceV4(&anchor, 2) && !worker.BeginRenderReferenceV5(&anchor, 2) && !worker.BeginRenderReferenceV6(&anchor, 2),
                  "all renderer protocols must share one retained owner");
            const auto capture = [&]
            {
                return v6   ? worker.CaptureRenderV6()
                       : v5 ? worker.CaptureRenderV5()
                       : v4 ? worker.CaptureRenderV4()
                       : v3 ? worker.CaptureRenderV3()
                       : v2 ? worker.CaptureRenderV2()
                            : worker.CaptureRender();
            };
            const auto read = [&](std::uint32_t field)
            {
                return v6   ? worker.ReadRenderV6(field)
                       : v5 ? worker.ReadRenderV5(field)
                       : v4 ? worker.ReadRenderV4(field)
                       : v3 ? worker.ReadRenderV3(field)
                       : v2 ? worker.ReadRenderV2(field)
                            : worker.ReadRender(field);
            };
            const auto deadline = GetTickCount64() + 6000;
            while (!capture())
            {
                Check(worker.Poll() == 0 && GetTickCount64() < deadline, "render callback failed to publish while pending");
                Sleep(5);
            }
            Check(read(Phase) == 1 && read(GenerationHigh) == 2 && read(Width) == (explicitRoute ? 0u : 1280u) && read(BackendPinnedExpected) == 1 &&
                      read(v6   ? FieldCountV6
                           : v5 ? FieldCountV5
                           : v4 ? FieldCountV4
                           : v3 ? FieldCountV3
                           : v2 ? FieldCountV2
                                : FieldCount) == 0,
                  "render cache must preserve the coherent numeric publication and high counter halves");
            Check(v6 ? !worker.CaptureRenderV5() && !worker.ReadRenderV5(Version) && !worker.CaptureRenderV4() && !worker.CaptureRenderV3() &&
                           !worker.CaptureRenderV2() && !worker.CaptureRender()
                  : v5 ? !worker.CaptureRenderV4() && !worker.ReadRenderV4(Version) && !worker.CaptureRenderV3() && !worker.CaptureRenderV2() &&
                             !worker.CaptureRender()
                  : v4 ? !worker.CaptureRender() && !worker.CaptureRenderV2() && !worker.CaptureRenderV3() && !worker.ReadRender(Version) &&
                             !worker.ReadRenderV2(Version) && !worker.ReadRenderV3(Version)
                  : v3 ? !worker.CaptureRender() && !worker.CaptureRenderV2() && !worker.ReadRender(Version) && !worker.ReadRenderV2(Version) &&
                             !worker.CaptureRenderV4()
                  : v2 ? !worker.CaptureRender() && !worker.ReadRender(Version) && !worker.CaptureRenderV3()
                       : !worker.CaptureRenderV2() && !worker.ReadRenderV2(Version) && !worker.CaptureRenderV3(),
                  "a cache cannot be read through the other protocol");
            if (!v5 && !v6)
                Check(!worker.CaptureRenderV5() && !worker.ReadRenderV5(Version), "historical protocols cannot expose Present association fields");
            else
                for (unsigned field = 84; field < (v6 ? FieldCountV6 : FieldCountV5); ++field)
                    Check(read(field) == 0, "API5 warmup cannot invent a scoped Present or association");
            if (!v4 && !v5 && !v6)
                Check(!worker.CaptureRenderV4() && !worker.ReadRenderV4(Version), "legacy caches cannot expose classification tail fields");
            else
                Check(read(80) == 0 && read(81) == 0 && read(82) == 0 && read(83) == 0,
                      "warmup cannot invent a classified custom presenter before a publication");
            if (explicitRoute)
                Check(read(Version) == (v6   ? 6u
                                        : v5 ? 5u
                                        : v4 ? 4u
                                        : v3 ? 3u
                                             : 2u) &&
                          read(RequestedRoute) == route && !read(ObservedRoute) && !read(PresenceKnownMask) && !read(QueuePresent),
                      "warmup must preserve the explicit request without inventing a naturally observed route");
            bool        foreignRejected = false;
            std::thread foreign(
                [&]
                {
                    foreignRejected = !capture() && read(0) == 0 && !worker.BeginRenderReference(&anchor) && !worker.BeginRenderReferenceV2(&anchor, route) &&
                                      !worker.BeginRenderReferenceV3(&anchor, 2) && !worker.BeginRenderReferenceV4(&anchor, 2) &&
                                      !worker.BeginRenderReferenceV5(&anchor, 2) && !worker.BeginRenderReferenceV6(&anchor, 2);
                });
            foreign.join();
            Check(foreignRejected, "render cache and lifecycle must stay with the script owner");
            if (cancel)
                Check(Fast([&] { return worker.Cancel(); }), "render cancellation must remain value-only");
            SRWLOCK* heldCache = cacheContention ? &deonline::CoreStartupReferenceTestAccess::InputLock(worker) : nullptr;
            if (heldCache)
                AcquireSRWLockShared(heldCache);
            SetEvent(fixture.inputRelease);
            if (heldCache)
            {
                // The controlled Core invokes the actual configured production
                // PublishInput on its worker. Keep the script's real shared lock
                // held until telemetry proves that pending publication returned.
                const auto pendingAt = GetTickCount64();
                while (!fixture.counters[20] && GetTickCount64() - pendingAt < 500)
                    Sleep(1);
                const bool pendingReturned = fixture.counters[20] != 0;
                const bool pendingDropped = deonline::CoreStartupReferenceTestAccess::PublishedPhase(worker) == 1;
                ReleaseSRWLockShared(heldCache);
                Check(pendingReturned && pendingDropped, "contended API6 pending cleanup must return without blocking or publishing");
                Check(read(Phase) == 1, "dropping pending cleanup must preserve the prior coherent owner capture");
            }
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
            if (cacheContention)
                Check(read(Phase) == 2 && worker.Poll() == 1, "released cache contention must still deliver the reliable final API6 publication");
            Check(worker.Read(13) == 1 && worker.Read(15) == 1 &&
                      worker.Read(16) == (cancel                           ? 12u
                                          : (deadlineFailure || malformed) ? 15u
                                                                           : 0u),
                  "render backend PIN must remain separate from the unloaded Core owner");
            Check(!worker.BeginRenderReferenceV6(&anchor, 0) && !worker.BeginRenderReferenceV6(&anchor, 1) && !worker.BeginRenderReferenceV6(&anchor, 3),
                  "API6 admits only its explicitly selected D3D12 route");
            if (!v6)
                Check(!worker.CaptureRenderV6() && !worker.ReadRenderV6(Version), "historical caches cannot expose native dispatch");
            if (v6 && !noCallbacks)
                Check(read(84) == 1 && read(85) == 0 &&
                          read(110) == (mode.find(L"unknown-presenter") != std::wstring::npos ? 10u
                                        : mode.find(L"query-failed") != std::wstring::npos    ? 5u
                                                                                              : 2u) &&
                          read(111) == 2 && read(118) == 10 && read(119) == 2 && read(121) == 1 &&
                          read(130) == (mode.find(L"unknown-presenter") != std::wstring::npos ? 0u : read(25)) && read(136) == 0,
                      "API6 dispatch tail retains its separate profile/counters alongside old endpoint refusal");
            if (v5 && !noCallbacks)
                Check(read(84) == (mode.find(L"unknown-presenter") != std::wstring::npos ? 10u
                                   : mode.find(L"query-failed") != std::wstring::npos    ? 5u
                                                                                         : 2u) &&
                          read(85) == 1 && read(92) == 10 && read(93) == 2 && read(95) == 1 &&
                          read(104) == (mode.find(L"unknown-presenter") != std::wstring::npos ? 0u : read(25)) && read(110) == 0,
                      "API5 association tail must survive retained cleanup without aliasing older cache widths");
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
            if ((v4 || v5 || v6) && !malformed && !noCallbacks)
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
        else if (std::wstring(argv[2]) == L"actual-frontend-startup" || std::wstring(argv[2]) == L"actual-frontend-startup-v2")
        {
            namespace Startup = DefinitiveFrontendStartup;
            namespace Report = deonline::FrontendStartupReport;
            const bool v2 = std::wstring(argv[2]) == L"actual-frontend-startup-v2";
            Check(Fast([&] { return v2 ? worker.BeginGuiFrontendStartupReferenceV2(&anchor) : worker.BeginGuiFrontendStartupReference(&anchor); }),
                  "actual completed Core worker begins without script-thread loader work");
            Wait(worker);
            for (unsigned field = 0; field < 8; ++field)
                Check(!worker.Read(field), "actual completed Core worker leaves historical startup8 fields reserved");
            Check(worker.Read(8) == 2 && worker.Read(9) == static_cast<std::uint32_t>(v2 ? Startup::Status::Stopped : Startup::Status::Failed) &&
                      worker.Read(10) == static_cast<std::uint32_t>(Startup::Status::Ok),
                  "actual Core completes construction, rejects native binding, and shuts down independently");
            Check(worker.Read(13) == 1 && !worker.Read(14) && !worker.Read(15) && worker.Read(16) == (v2 ? 27u : 23u) && !worker.Read(17) && !worker.Read(18),
                  "wrong-image completed view fails workload while actual owner/backend/code cleanup succeeds");
            Check(worker.CaptureGuiFrontendStartup(), "actual copied completed Core lifecycle report survives DLL unload");
            for (const auto offset : {Report::Begin, Report::AfterView})
            {
                Check(worker.ReadGuiFrontendStartup(offset + Startup::Version) == 1 && worker.ReadGuiFrontendStartup(offset + Startup::Phase) == 2 &&
                          worker.ReadGuiFrontendStartup(offset + Startup::CoreRetained) == 1 &&
                          worker.ReadGuiFrontendStartup(offset + Startup::BackendStarted) == 1 &&
                          !worker.ReadGuiFrontendStartup(offset + Startup::ExactImageBound) && !worker.ReadGuiFrontendStartup(offset + Startup::ViewRuns) &&
                          worker.ReadGuiFrontendStartup(offset + Startup::LastViewCleanupComplete) == 1,
                      "actual completed Core/backend remains alive before and after explicit wrong-image view refusal");
            }
            Check(worker.ReadGuiFrontendStartup(Report::AfterView + Startup::LastFailure) == static_cast<std::uint32_t>(Startup::Failure::InputBind),
                  "actual backend, not a fixture boolean, supplies the native bind refusal");
            Check(worker.ReadGuiFrontendStartup(Report::Stopped + Startup::Phase) == 4 &&
                      worker.ReadGuiFrontendStartup(Report::Stopped + Startup::CoreRetained) == 1 &&
                      !worker.ReadGuiFrontendStartup(Report::Stopped + Startup::BackendStarted) &&
                      !worker.ReadGuiFrontendStartup(Report::Stopped + Startup::CapabilitiesLow) &&
                      !worker.ReadGuiFrontendStartup(Report::Stopped + Startup::CapabilitiesHigh) &&
                      worker.ReadGuiFrontendStartup(Report::Stopped + Startup::CleanupComplete) == 1 &&
                      worker.ReadGuiFrontendStartup(Report::Stopped + Startup::LastViewCleanupComplete) == 1,
                  "actual stopped Core is read before Release with adapter capabilities retired");
            for (const auto offset : {Report::Begin, Report::AfterView, Report::Stopped})
            {
                for (unsigned field = Startup::FrontendRetained; field <= Startup::InputRequested; ++field)
                    Check(!worker.ReadGuiFrontendStartup(offset + field), "wrong-image async startup creates no GUI/services/input ownership");
                Check(!worker.ReadGuiFrontendStartup(offset + Startup::NetworkStarted) && !worker.ReadGuiFrontendStartup(offset + Startup::GameplayReady) &&
                          !worker.ReadGuiFrontendStartup(offset + Startup::FullMtaReady),
                      "actual completed frontend startup never upgrades full gameplay/network readiness");
            }
            Check(!worker.CaptureGuiFrontendInput() && !worker.ReadGuiFrontendInput(0) && !worker.ReadGuiFrontendStartup(Report::FieldCount),
                  "a never-started native view invents no256 evidence and obeys report bounds");
            Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"pthread.dll"),
                  "actual completed async owner releases its Core/backend/pthread modules");
            if (v2)
                Check(!worker.CaptureGuiFrontendStartupClockV2() && worker.CaptureGuiFrontendStartupClockOwnerV2() &&
                          worker.ReadGuiFrontendStartupClockOwnerV2(Startup::Phase) == 2 &&
                          worker.ReadGuiFrontendStartupClockOwnerV2(Startup::LastFailure) == static_cast<std::uint32_t>(Startup::Failure::InputBind) &&
                          !worker.ReadGuiFrontendStartupClockOwnerV2(Startup::ViewRuns),
                      "actual V2 rejects native binding without inventing96 clock evidence, then retains/copies the genuine clean owner32");
        }
        else if (std::wstring(argv[2]) == L"actual-runtime")
        {
            namespace R = deonline::FrontendRuntimeReport;
            const auto generation = Fast([&] { return worker.BeginFrontendRuntime(&anchor); });
            Check(generation != 0, "actual runtime start owns an independent host generation");
            const auto deadline = GetTickCount64() + 15000;
            while (!worker.CaptureFrontendRuntime() || worker.ReadFrontendRuntime(R::WorkerPhase) != static_cast<std::uint32_t>(R::Phase::Hidden))
            {
                worker.HeartbeatFrontendRuntime(generation);
                Check(worker.PollFrontendRuntime() == 0 && GetTickCount64() < deadline, "actual runtime services failed before Hidden");
                Sleep(5);
            }
            Check(worker.ReadFrontendRuntime(R::CoreOffset + 33) == 2 && worker.ReadFrontendRuntime(R::CoreOffset + 38) == 1 &&
                      worker.ReadFrontendRuntime(R::CoreOffset + 43) == 1 && worker.ReadFrontendRuntime(R::CoreOffset + 44) == 1 &&
                      worker.ReadFrontendRuntime(R::CoreOffset + 45) == 1,
                  "actual Core owns one XML/config/CVars/binding session while hidden");
            Check(worker.CommandFrontendRuntime(generation, 1, 1), "actual runtime requests one fresh native view");
            while (worker.PollFrontendRuntime() == 0)
            {
                worker.HeartbeatFrontendRuntime(generation);
                Check(GetTickCount64() < deadline, "actual wrong-image runtime did not safely retire");
                Sleep(5);
            }
            Check(worker.CaptureFrontendRuntime() && worker.ReadFrontendRuntime(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Failed) &&
                      worker.ReadFrontendRuntime(R::LastFailure) == static_cast<std::uint32_t>(R::Failure::View) &&
                      worker.ReadFrontendRuntime(R::CoreSampleStage) == static_cast<std::uint32_t>(R::CoreStage::Stopped),
                  "real engine image guard rejects runtime view without corrupting ownership");
            for (const auto field : {R::CleanupComplete, R::CoreStoppedVerified, R::CoreReleasedVerified, R::CoreUnloadedVerified})
                Check(worker.ReadFrontendRuntime(field) == 1, "actual runtime owner and real DLL dependencies retired");
            for (std::uint32_t field = R::ViewOffset; field < R::FieldCount; ++field)
                Check(!worker.ReadFrontendRuntime(field), "wrong-image refusal invents no288 native/GUI evidence");
            Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"xmll.dll") && !GetModuleHandleW(L"pthread.dll"),
                  "actual refused runtime releases session XML before backend/Core unload");
        }
        else if (std::wstring(argv[2]) == L"actual-persistent-frontend")
        {
            namespace Startup = DefinitiveFrontendStartup;
            namespace Report = deonline::FrontendStartupReport;
            namespace Persistent = DefinitivePersistentFrontend;
            static_assert(Persistent::ApiVersion == 1 && Persistent::FieldCount == 288);
            const auto generation = Fast([&] { return worker.BeginPersistentFrontend(&anchor); });
            Check(generation != 0, "actual persistent Core worker begins with a nonzero host-owned token without loader work on script thread");
            Check(!worker.Begin(&anchor) && !worker.BeginGuiFrontendStartupReference(&anchor), "pending persistent owner excludes historical worker attempts");
            Check(!Fast([&] { return worker.HeartbeatPersistentFrontend(generation + 1); }), "actual worker refuses a stale host generation");
            const auto started = GetTickCount64();
            // Keep the independent host lease live while the actual DLL loads.
            // A rejection racing normal cleanup is not an expiry claim: only
            // the copied final Core/lifetime values establish what happened.
            while (Fast([&] { return worker.Poll(); }) == 0)
            {
                Fast([&] { return worker.HeartbeatPersistentFrontend(generation); });
                Check(GetTickCount64() - started < 15000, "actual persistent worker failed to retire within owned fixture budget");
                Sleep(10);
            }
            Check(worker.Poll() == 1 && !worker.IsPending() && !worker.HeartbeatPersistentFrontend(generation),
                  "actual persistent completion retires the worker and prevents later host renewal");
            for (unsigned field = 0; field < 8; ++field)
                Check(!worker.Read(field), "persistent worker keeps historical startup8 positions reserved instead of reinterpreting them");
            Check(worker.Read(8) == 2 && worker.Read(9) == static_cast<std::uint32_t>(Startup::Status::Failed) &&
                      worker.Read(10) == static_cast<std::uint32_t>(Startup::Status::Ok) && worker.Read(13) == 1 && !worker.Read(14) && !worker.Read(15) &&
                      worker.Read(16) == 23 && !worker.Read(17) && !worker.Read(18),
                  "actual persistent wrong-image workload fails while its separate completed owner and modules cleanly retire");
            Check(worker.CapturePersistentFrontendLifecycle(), "actual persistent lifecycle96 remains copied after Core unload");
            const auto read = [&](std::uint32_t field) { return worker.ReadPersistentFrontendLifecycle(field); };
            const auto owner = read(Startup::OwnerThread);
            Check(owner && owner != GetCurrentThreadId() && read(Report::AfterView + Startup::OwnerThread) == owner &&
                      read(Report::Stopped + Startup::OwnerThread) == owner,
                  "actual Begin, persistent refusal, shutdown and final publication retain the same independent Core worker owner");
            for (const auto offset : {Report::Begin, Report::AfterView})
            {
                Check(read(offset + Startup::Version) == 1 && read(offset + Startup::Phase) == 2 && read(offset + Startup::CoreRetained) == 1 &&
                          read(offset + Startup::BackendStartAttempted) == 1 && read(offset + Startup::BackendStarted) == 1 &&
                          !read(offset + Startup::ExactImageBound) && !read(offset + Startup::ViewRuns) &&
                          read(offset + Startup::LastViewCleanupComplete) == 1 && read(offset + Startup::CapabilitiesLow) == 33 &&
                          !read(offset + Startup::CapabilitiesHigh),
                      "actual persistent refusal retains completed Core/backend and CPU-only synthesis capabilities without a native lease or view");
            }
            Check(!read(Startup::LastFailure) && read(Report::AfterView + Startup::LastFailure) == static_cast<std::uint32_t>(Startup::Failure::InputBind) &&
                      read(Report::AfterView + Startup::Pulses) >= read(Startup::Pulses),
                  "real exact-image binding supplies the persistent refusal without corrupting completed Core pulses");
            Check(read(Report::Stopped + Startup::Phase) == 4 && read(Report::Stopped + Startup::CoreRetained) == 1 &&
                      !read(Report::Stopped + Startup::BackendStarted) && !read(Report::Stopped + Startup::CapabilitiesLow) &&
                      !read(Report::Stopped + Startup::CapabilitiesHigh) && !read(Report::Stopped + Startup::CleanupPending) &&
                      read(Report::Stopped + Startup::CleanupComplete) == 1 && !read(Report::Stopped + Startup::ReleaseFault) &&
                      read(Report::Stopped + Startup::LastViewCleanupComplete) == 1,
                  "actual persistent worker reads a retained stopped Core with no backend capability before explicit Release/unload");
            for (const auto offset : {Report::Begin, Report::AfterView, Report::Stopped})
            {
                for (unsigned field = Startup::FrontendRetained; field <= Startup::InputRequested; ++field)
                    Check(!read(offset + field), "wrong-image persistent startup creates no GUI/service/input ownership");
                Check(!read(offset + Startup::NetworkStarted) && !read(offset + Startup::GameplayReady) && !read(offset + Startup::FullMtaReady),
                      "actual persistent lifecycle never advertises network, gameplay or full MTA readiness");
                for (unsigned field = Startup::Reserved24; field < Startup::FieldCount; ++field)
                    Check(!read(offset + field), "actual persistent lifecycle reserved fields stay zero");
            }
            bool neutralPersistent = true, neutralHistorical = true;
            for (std::uint32_t field = 0; field < Persistent::FieldCount; ++field)
                neutralPersistent = neutralPersistent && !worker.ReadPersistentFrontend(field);
            for (std::uint32_t field = 0; field < DefinitiveGuiFrontendInputReference::FieldCount; ++field)
                neutralHistorical = neutralHistorical && !worker.ReadGuiFrontendInput(field);
            Check(!worker.CapturePersistentFrontend() && neutralPersistent && !worker.ReadPersistentFrontend(Persistent::FieldCount) &&
                      !worker.CaptureGuiFrontendInput() && neutralHistorical && !worker.CaptureGuiFrontendStartup() && !worker.ReadGuiFrontendStartup(0) &&
                      !worker.ReadPersistentFrontendLifecycle(Report::FieldCount),
                  "unstarted persistent view invents no288 input, leaks no historical256/96 cache and obeys each separate report bound");
            Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"pthread.dll") && !GetModuleHandleW(L"cgui.dll") &&
                      !GetModuleHandleW(L"xmll.dll"),
                  "actual persistent wrong-image cleanup unloads all real Core/backend dependencies without starting GUI");
            // Reuse the same script owner for the unchanged historical gate.
            // Safe persistent refusal must neither quarantine the worker nor
            // convert InitializeCore/Startup8 into a completed-gameplay claim.
            Check(Fast([&] { return worker.Begin(&anchor); }), "actual clean persistent refusal permits an ordinary Core startup retry");
            Wait(worker);
            const std::array<unsigned, 11> legacy{1, 5, 3, 33, 0, 0, 0, 1, 3, 4, 5};
            for (unsigned field = 0; field < legacy.size(); ++field)
                Check(worker.Read(field) == legacy[field], "actual legacy Startup8 and directory-gate diagnostics remain unchanged after persistent cleanup");
            VerifyCleanup(worker, 8);
            Check(!worker.CapturePersistentFrontend() && !worker.CapturePersistentFrontendLifecycle() && !worker.ReadPersistentFrontend(0) &&
                      !worker.ReadPersistentFrontendLifecycle(0) && !GetModuleHandleW(L"game_de.dll"),
                  "ordinary retry retires its actual backend and cannot expose predecessor persistent evidence");
        }
        else if (std::wstring(argv[2]) == L"actual-clock-set")
        {
            using namespace DefinitiveClockSetReference;
            Check(Fast([&] { return worker.BeginClockSetReference(&anchor); }), "actual normal Core accepts the separate Set worker protocol");
            Wait(worker);
            Check(worker.CaptureClockSet(), "actual Core publishes Set preflight refusal evidence before worker unload");
            Snapshot sample{};
            for (std::uint32_t field = 0; field != FieldCount; ++field)
                sample[field] = worker.ReadClockSet(field);
            Check(Retired(sample) && !Completed(sample) && sample[NotEntered] && sample[AdapterStopped] && !sample[BeginAttempted] && !sample[SetSubmitted] &&
                      !sample[SetAcknowledged] && !sample[NativeRestored] && !sample[BackendPinnedExpected] && !sample[FullMtaReady] && !worker.CaptureClock(),
                  "actual wrong-image Set preflight proves non-entry cleanup without borrowing read-only evidence or certifying native writes");
            VerifyCleanup(worker, 8);
            Check(!GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"pthread.dll") && !worker.Read(17),
                  "actual refused Set releases Core/backend dependencies without claiming a GTA workload");
        }
        else if (std::wstring(argv[2]) == L"actual-clock")
        {
            using namespace DefinitiveClockReference;
            Check(Fast([&] { return worker.BeginClockReference(&anchor); }), "actual normal Core accepts the separate clock worker protocol");
            Wait(worker);
            Check(worker.CaptureClock(), "actual Core publishes explicit preflight refusal evidence before worker unload");
            Snapshot sample{};
            for (std::uint32_t field = 0; field != FieldCount; ++field)
                sample[field] = worker.ReadClock(field);
            Check(Retired(sample) && !sample[BeginAttempted] && !sample[BeginSucceeded] && !sample[ReadyObserved] && !sample[ReadSubmitted] &&
                      !sample[ReadAcknowledged] && !sample[ReadOnlyVerified] && !sample[NativeRestored] && !sample[BackendPinnedExpected] &&
                      !sample[FullMtaReady],
                  "actual earlier input binding refusal proves only pre-native cleanup and cannot certify any native clock callbacks, reads or restore");
            VerifyCleanup(worker, 8);
            Check(!GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"pthread.dll") && !worker.Read(17),
                  "actual wrong-image clock preflight releases Core/backend dependencies without claiming a GTA workload");
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
                    static_cast<unsigned long long>(worstApiMs), std::wstring(argv[2]).find(L"actual") == 0 ? "true" : "false");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("{\"success\":false,\"checks\":%d,\"error\":\"%s\"}\n", checks, error.what());
        return 2;
    }
}
