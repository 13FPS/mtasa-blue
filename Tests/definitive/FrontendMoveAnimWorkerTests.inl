/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only new movement-aware worker lifetimes, with controlled Core exports
 ****************************************************************************/
#include "../../Client/definitive/FrontendMoveAnimWorkloadGate.h"
#include <cstring>
namespace MoveAnimWorkerProtocol
{
    enum Field : unsigned
    {
        Phase,
        Forbidden,
        CorePulses,
        CachedReads,
        ConfigureCalls,
        LocalPulses,
        SuspendCalls,
        DisableCalls,
        ShutdownCalls,
        ShutdownBusy,
        ViewStarts,
        ViewReturns,
        AllowFinish,
        WrongThread,
        WordCount = 16
    };
    enum State : LONG
    {
        Idle,
        Queued,
        Pending,
        Finished
    };
}

#if defined(CORE_FIXTURE_MOVEANIM_WORKER)
namespace MoveAnimWorkerFixture
{
    HANDLE         telemetryMapping = nullptr;
    volatile LONG* telemetry = nullptr;
    volatile LONG* Data() noexcept
    {
        if (telemetry)
            return telemetry;
        wchar_t name[256]{};
        if (!GetEnvironmentVariableW(L"MTA_CORE_FIXTURE_MOVEANIM_MAP", name, 256))
            return nullptr;
        telemetryMapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
        if (telemetryMapping)
            telemetry =
                static_cast<volatile LONG*>(MapViewOfFile(telemetryMapping, FILE_MAP_ALL_ACCESS, 0, 0, MoveAnimWorkerProtocol::WordCount * sizeof(LONG)));
        return telemetry;
    }
    bool Enabled() noexcept
    {
        return Setting(L"MTA_CORE_FIXTURE_MOVEANIM_MODE") != 0;
    }
    void Count(unsigned field) noexcept
    {
        if (auto* data = Data())
        {
            InterlockedIncrement(data + field);
            if (counters && static_cast<DWORD>(counters[0]) != GetCurrentThreadId())
                InterlockedIncrement(data + MoveAnimWorkerProtocol::WrongThread);
        }
    }
    bool Blocked() noexcept
    {
        auto* data = Data();
        return data && (data[MoveAnimWorkerProtocol::Phase] == MoveAnimWorkerProtocol::Queued ||
                        data[MoveAnimWorkerProtocol::Phase] == MoveAnimWorkerProtocol::Pending);
    }
    void Began() noexcept
    {
        if (auto* data = Data())
            InterlockedExchange(data + MoveAnimWorkerProtocol::Phase, MoveAnimWorkerProtocol::Idle);
    }
    void Pulsed() noexcept
    {
        Count(MoveAnimWorkerProtocol::CorePulses);
        if (auto* data = Data(); data && data[MoveAnimWorkerProtocol::Phase] == MoveAnimWorkerProtocol::Pending && data[MoveAnimWorkerProtocol::AllowFinish])
            InterlockedExchange(data + MoveAnimWorkerProtocol::Phase, MoveAnimWorkerProtocol::Finished);
    }
    bool BeforeLocal(unsigned kind) noexcept
    {
        Count(kind == 0   ? MoveAnimWorkerProtocol::ConfigureCalls
              : kind == 1 ? MoveAnimWorkerProtocol::LocalPulses
              : kind == 2 ? MoveAnimWorkerProtocol::SuspendCalls
                          : MoveAnimWorkerProtocol::DisableCalls);
        if (!Blocked())
            return true;
        Count(MoveAnimWorkerProtocol::Forbidden);
        return false;
    }
    bool BeforeView() noexcept
    {
        if (!Blocked())
            return true;
        Count(MoveAnimWorkerProtocol::Forbidden);
        return false;
    }
    void ViewStarted() noexcept
    {
        Count(MoveAnimWorkerProtocol::ViewStarts);
        if (auto* data = Data(); data && data[MoveAnimWorkerProtocol::ViewStarts] == 1)
            InterlockedExchange(data + MoveAnimWorkerProtocol::Phase, MoveAnimWorkerProtocol::Queued);
    }
    void ViewReturned() noexcept
    {
        Count(MoveAnimWorkerProtocol::ViewReturns);
        if (auto* data = Data(); data && data[MoveAnimWorkerProtocol::ViewReturns] == 1)
            InterlockedExchange(data + MoveAnimWorkerProtocol::Phase, MoveAnimWorkerProtocol::Pending);
    }
    DefinitiveFrontend::Status Shutdown() noexcept
    {
        namespace C = DefinitiveFrontendRuntime;
        namespace S = DefinitiveFrontendStartup;
        Count(MoveAnimWorkerProtocol::ShutdownCalls);
        auto* data = Data();
        if (data && data[MoveAnimWorkerProtocol::Phase] == MoveAnimWorkerProtocol::Pending && data[MoveAnimWorkerProtocol::ShutdownBusy] < 2)
        {
            Count(MoveAnimWorkerProtocol::ShutdownBusy);
            return DefinitiveFrontend::Status::Busy;
        }
        if (data)
            InterlockedExchange(data + MoveAnimWorkerProtocol::Phase, MoveAnimWorkerProtocol::Finished);
        // This controlled Core owns its internal input order, as shipping Core
        // does. It must not depend on a caller spinning the local export first.
        LocalRetired(true);
        Call(58);
        runtimeOwner[S::Phase] = 4;
        runtimeOwner[S::BackendStarted] = runtimeOwner[S::ExactImageBound] = runtimeOwner[S::CapabilitiesLow] = 0;
        runtimeOwner[S::CleanupComplete] = 1;
        runtimeOwner[C::RuntimePhase] = 5;
        runtimeOwner[C::ServicesRetained] = runtimeOwner[C::XmlRetained] = runtimeOwner[C::ConfigRetained] = runtimeOwner[C::CVarsLoaded] =
            runtimeOwner[C::BindingsRetained] = 0;
        runtimeOwner[C::CleanupComplete] = 1;
        if (frontendBackend)
        {
            FreeLibrary(frontendBackend);
            frontendBackend = nullptr;
        }
        return DefinitiveFrontend::Status::Ok;
    }
    void Detached() noexcept
    {
        if (telemetry)
        {
            UnmapViewOfFile(const_cast<LONG*>(telemetry));
            telemetry = nullptr;
        }
        if (telemetryMapping)
        {
            CloseHandle(telemetryMapping);
            telemetryMapping = nullptr;
        }
    }
}
extern "C" __declspec(dllexport) DefinitiveFrontend::Status MTA_ReadDEFrontendRuntimeMoveAnimWorkloadV1(void* handle, std::uint32_t version,
                                                                                                        DefinitiveMoveAnimWorkload::ReportV1* output)
{
    namespace M = MoveAnimWorkerProtocol;
    namespace R = DefinitiveMoveAnimWorkload;
    using namespace mta::game_de;
    if (handle != &runtimeOwner || version != 1 || !output)
        return DefinitiveFrontend::Status::InvalidArgument;
    if (!counters || static_cast<DWORD>(counters[0]) != GetCurrentThreadId())
        return DefinitiveFrontend::Status::WrongThread;
    MoveAnimWorkerFixture::Count(M::CachedReads);
    auto* data = MoveAnimWorkerFixture::Data();
    if (!data)
        return DefinitiveFrontend::Status::Failed;
    R::ReportV1 report;
    report.available = 1;
    report.lastStatus = report.beginStatus = Status::Ok;
    const auto phase = data[M::Phase];
    report.phase = phase == M::Queued ? R::Phase::Queued : phase == M::Pending ? R::Phase::Running : phase == M::Finished ? R::Phase::Finished : R::Phase::Idle;
    report.queued = phase == M::Queued ? 1u : 0u;
    report.beginAttempted = phase >= M::Pending ? 1u : 0u;
    report.attemptPending = phase == M::Pending ? 1u : 0u;
    if (phase >= M::Pending)
    {
        report.native.sequence = 1;
        report.native.phase = phase == M::Pending ? MoveAnimWorkloadPhaseV1::Observing : MoveAnimWorkloadPhaseV1::Retired;
        report.native.result = ClockResult::Ok;
        report.native.selectedGroup = report.native.afterGroup = 54;
        report.native.beforeGroup = 56;
        report.native.owner.generation = UINT64_C(0x1234567887654321);
        report.native.owner.producerThread = GetCurrentThreadId();
        report.native.owner.modulePinned = 1;  // Permanent PIN is not pending ownership.
        report.native.owner.active = report.native.owner.hooked = report.native.cleanupPending = phase == M::Pending ? 1u : 0u;
        report.native.tokenRegistered = report.native.overridePublished = phase == M::Pending ? 1u : 0u;
        report.native.hooksEnabled = report.native.writeAcknowledged = report.native.reapplyAcknowledged = 1;
        report.native.endAcknowledged = phase == M::Finished ? 1u : 0u;
    }
    *output = report;
    return DefinitiveFrontend::Status::Ok;
}
#else
namespace
{
    struct MoveTelemetry
    {
        HANDLE         mapping = nullptr;
        volatile LONG* values = nullptr;
        MoveTelemetry()
        {
            const auto name = L"Local\\MtaMoveWorker-" + std::to_wstring(GetCurrentProcessId());
            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, MoveAnimWorkerProtocol::WordCount * sizeof(LONG), name.c_str());
            Check(mapping != nullptr, "create separate movement telemetry without changing historical64 counters");
            values = static_cast<volatile LONG*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, MoveAnimWorkerProtocol::WordCount * sizeof(LONG)));
            Check(values != nullptr, "map controlled movement worker telemetry");
            Set(L"MTA_CORE_FIXTURE_MOVEANIM_MAP", name);
        }
        ~MoveTelemetry()
        {
            if (values)
                UnmapViewOfFile(const_cast<LONG*>(values));
            if (mapping)
                CloseHandle(mapping);
        }
    };
    void TestMoveAnimWorker(deonline::CoreStartupReference& worker, const std::filesystem::path& root, const std::wstring& mode,
                            const std::filesystem::path& corePath)
    {
        namespace M = MoveAnimWorkerProtocol;
        namespace R = deonline::FrontendRuntimeReport;
        const bool stopping = mode == L"moveanim-worker-stop", legacy = mode == L"moveanim-worker-legacy";
        Check(stopping || legacy || mode == L"moveanim-worker-deferred", "one new movement-aware worker case selected");
        Fixture       fixture(975);
        MoveTelemetry move;
        Set(L"MTA_CORE_FIXTURE_LOCAL_RUNTIME_MODE", L"1");
        Set(L"MTA_CORE_FIXTURE_MOVEANIM_MODE", legacy ? L"0" : stopping ? L"2" : L"1");
        DefinitiveMoveAnimWorkload::ReportV1        sentinel;
        std::array<unsigned char, sizeof(sentinel)> untouched;
        untouched.fill(0xa5);
        std::memcpy(&sentinel, untouched.data(), sizeof(sentinel));
        Check(!worker.CaptureFrontendMoveAnimWorkload(1, sentinel) && !std::memcmp(&sentinel, untouched.data(), sizeof(sentinel)),
              "unstarted workload cache refuses without publishing a default sample");
        CopyCore(corePath, root);
        SetEvent(fixture.release);
        const auto generation = Fast([&] { return worker.BeginFrontendRuntime(&anchor, true); });
        Check(generation != 0, "actual worker begins one optional-input retained Runtime");
        const auto deadline = GetTickCount64() + 14000;
        const auto read = [&](std::uint32_t field) { return worker.ReadFrontendRuntime(field); };
        const auto wait = [&](const auto& predicate)
        {
            while (!worker.CaptureFrontendRuntime() || !predicate())
            {
                worker.HeartbeatFrontendRuntime(generation);
                Check(worker.PollFrontendRuntime() == 0 && GetTickCount64() < deadline, "movement worker reaches the bounded controlled handoff");
                Sleep(5);
            }
        };
        const auto command = [&](std::uint32_t sequence, R::Command action) { AdmitRuntimeCommand(worker, generation, sequence, action); };
        wait([&] { return read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Hidden); });
        command(1, R::Command::Show);
        wait([&] { return read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Visible); });
        Check(!fixture.counters[54] && !fixture.counters[55], "first visible view precedes local rearm");
        if (!legacy)
            Check(move.values[M::Phase] == M::Queued && !move.values[M::ConfigureCalls] && !move.values[M::LocalPulses],
                  "controlled explicit command queues while view owns input and performs no workload/native calls");
        command(2, R::Command::Hide);
        wait([&] { return read(R::ViewsRetired) == 1 && read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Hidden); });
        if (!legacy)
        {
            DefinitiveMoveAnimWorkload::ReportV1 pending;
            wait([&] { return worker.CaptureFrontendMoveAnimWorkload(generation, pending) && pending.attemptPending; });
            Check(pending.phase == DefinitiveMoveAnimWorkload::Phase::Running && pending.native.owner.modulePinned == 1 &&
                      deonline::FrontendMoveAnimWorkloadGate::Blocks(pending),
                  "actual worker cache carries pending native debt separately from the permanent process PIN");
            std::memcpy(&sentinel, untouched.data(), sizeof(sentinel));
            Check(!worker.CaptureFrontendMoveAnimWorkload(generation + 1, sentinel) && !std::memcmp(&sentinel, untouched.data(), sizeof(sentinel)),
                  "foreign runtime generation cannot borrow or overwrite the cached workload report");
            bool        refused = false;
            std::thread foreign(
                [&]
                {
                    DefinitiveMoveAnimWorkload::ReportV1 foreignOutput;
                    std::memcpy(&foreignOutput, untouched.data(), sizeof(foreignOutput));
                    refused = !worker.CaptureFrontendMoveAnimWorkload(generation, foreignOutput) &&
                              !std::memcmp(&foreignOutput, untouched.data(), sizeof(foreignOutput));
                });
            foreign.join();
            Check(refused && !move.values[M::WrongThread], "foreign cache reader refuses before calling any controlled Core export");
        }
        if (legacy)
        {
            std::memcpy(&sentinel, untouched.data(), sizeof(sentinel));
            Check(!worker.CaptureFrontendMoveAnimWorkload(generation, sentinel) && !std::memcmp(&sentinel, untouched.data(), sizeof(sentinel)),
                  "absent optional export cannot manufacture a valid workload report");
            wait([&] { return fixture.counters[54] > 0 && fixture.counters[55] > 0; });
            Check(!move.values[M::CachedReads], "legacy absent optional export preserves its existing rearm path");
            command(3, R::Command::Stop);
        }
        else if (stopping)
        {
            wait([&] { return move.values[M::Phase] == M::Pending && move.values[M::CorePulses] > 0; });
            Check(!move.values[M::Forbidden] && !move.values[M::ConfigureCalls] && !move.values[M::LocalPulses],
                  "pending physical workload excludes local rearm and local polling");
            command(3, R::Command::Stop);
        }
        else
        {
            wait([&] { return move.values[M::Phase] == M::Pending && move.values[M::CorePulses] > 0; });
            const auto before = move.values[M::CorePulses];
            command(3, R::Command::Show);
            const auto holdUntil = GetTickCount64() + 200;
            while (GetTickCount64() < holdUntil)
            {
                worker.HeartbeatFrontendRuntime(generation);
                worker.CaptureFrontendRuntime();
                Sleep(5);
            }
            Check(move.values[M::ViewStarts] == 1 && fixture.counters[47] == 1 && !move.values[M::Forbidden] && !move.values[M::ConfigureCalls] &&
                      !move.values[M::LocalPulses],
                  "concurrent Show waits while the workload blocks admission and input");
            Check(move.values[M::CorePulses] > before, "deferred Show continues genuine Core pulses instead of starving workload progress");
            InterlockedExchange(move.values + M::AllowFinish, 1);
            wait(
                [&]
                {
                    return move.values[M::Phase] == M::Finished && read(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Visible) &&
                           fixture.counters[47] == 2;
                });
            Check(!move.values[M::Forbidden], "only acknowledged finished movement ownership permits the next view");
            command(4, R::Command::Hide);
            wait([&] { return move.values[M::ViewReturns] == 2 && move.values[M::ConfigureCalls] > 0 && move.values[M::LocalPulses] > 0; });
            Check(move.values[M::Phase] == M::Finished && !move.values[M::Forbidden], "normal local input resumes only after the movement owner has retired");
            command(5, R::Command::Stop);
        }
        Wait(worker);
        Check(worker.CaptureFrontendRuntime() && read(R::CleanupComplete) && !read(R::CleanupPending) && !read(R::LastFailure) &&
                  read(R::CoreStoppedVerified) && read(R::CoreReleasedVerified) && read(R::CoreUnloadedVerified),
              "actual worker stops/releases/unloads the same controlled Core");
        Check(!fixture.counters[2] && !fixture.counters[52] && fixture.counters[49] == 1 && !GetModuleHandleW(L"core.dll"),
              "worker preserves Core affinity and view lifetime through final release");
        if (!legacy)
        {
            DefinitiveMoveAnimWorkload::ReportV1 final;
            Check(worker.CaptureFrontendMoveAnimWorkload(generation, final) && final.phase == DefinitiveMoveAnimWorkload::Phase::Finished &&
                      final.native.endAcknowledged && final.native.owner.modulePinned == 1 && !deonline::FrontendMoveAnimWorkloadGate::Blocks(final),
                  "final cached receipt confirms Core End while retaining the process PIN and no borrower debt");
            Check(!move.values[M::Forbidden] && !move.values[M::WrongThread] && move.values[M::CachedReads] > 0,
                  "optional report consumption and lifecycle calls stay on the real worker producer");
            if (stopping)
                Check(move.values[M::ShutdownCalls] >= 3 && move.values[M::ShutdownBusy] == 2 && !move.values[M::DisableCalls],
                      "Stop reaches authoritative Core shutdown Busy-to-Ok without spinning local-disable first");
        }
        std::printf("MoveAnim worker cached report case completed: %ls\n", mode.c_str());
        std::printf("MoveAnim worker case completed: %ls\n", mode.c_str());
    }
}
#endif
