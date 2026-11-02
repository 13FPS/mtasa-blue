/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Prove first-menu intent over the actual persistent Core worker route
 ****************************************************************************/
template <class Predicate>
void WaitLoaded(Predicate predicate, const char* reason)
{
    const auto deadline = GetTickCount64() + 15000;
    while (!predicate())
    {
        if (GetTickCount64() >= deadline)
            throw std::runtime_error(reason);
        Sleep(5);
    }
}
void TestLoadedWorldIntent()
{
    namespace R = deonline::FrontendRuntimeReport;
    namespace A = deonline::FrontendAdmissionSample;
    using I = deonline::NativeFrontendStartupIntent;
    R::Snapshot runtime{};
    runtime[R::Version] = 1;
    runtime[R::SessionGenerationLow] = 7;
    runtime[R::WorkerThread] = 19;
    runtime[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Hidden);
    runtime[R::HostLeaseFresh] = 1;
    A::Snapshot sample{};
    sample[A::Version] = 1;
    sample[A::RuntimeGeneration] = 7;
    sample[A::WorkerThread] = 19;
    sample[A::Status] = static_cast<std::uint32_t>(DefinitiveFrontend::Status::Ok);
    sample[A::Admission] = static_cast<std::uint32_t>(mta::game_de::FrontendAdmission::EligibleLocal);
    sample[A::SerialLow] = 1;
    sample[A::TickLow] = 1000;
    I disabled(false, 7), intent(true, 7);
    Check(!disabled.ShouldShow(runtime, sample, true, false, 1000), "legacy default never queues an automatic Show");
    Check(intent.ShouldShow(runtime, sample, true, false, 1000), "fresh same-owner eligible sample may queue the existing Show");
    Check(!intent.ShouldShow(runtime, sample, false, false, 1000) && !intent.ShouldShow(runtime, sample, true, true, 1000),
          "focus and explicit manual intent retain priority over automatic startup");
    auto changed = sample;
    changed[A::WorkerThread] = 20;
    Check(!intent.ShouldShow(runtime, changed, true, false, 1000), "another producer cannot lend eligible admission");
    changed = sample;
    changed[A::RuntimeGeneration] = 8;
    Check(!intent.ShouldShow(runtime, changed, true, false, 1000), "a stale runtime generation cannot drive startup");
    Check(!intent.ShouldShow(runtime, sample, true, false, 1501) && !intent.ShouldShow(runtime, sample, true, false, 999),
          "expired and future observations cannot drive startup");
    changed = sample;
    changed[A::Status] = static_cast<std::uint32_t>(DefinitiveFrontend::Status::Failed);
    changed[A::SerialLow] = 2;
    Check(!intent.ShouldShow(runtime, changed, true, false, 1000), "a newer failed sample invalidates old eligibility without becoming Deferred");
    changed = sample;
    changed[A::Admission] = static_cast<std::uint32_t>(mta::game_de::FrontendAdmission::Deferred);
    Check(!intent.ShouldShow(runtime, changed, true, false, 1000), "valid Deferred remains hidden without consuming first-show intent");
    intent.Submitted(sample);
    Check(intent.Pending() && !intent.ShouldShow(runtime, sample, true, false, 1000), "queueing consumes only this sample, not admission intent");
    ++sample[A::SerialLow];
    Check(intent.ShouldShow(runtime, sample, true, false, 1000), "a deferred attempt can retry only after a new fresh eligible observation");
    runtime[R::OutstandingSequence] = 1;
    Check(!intent.ShouldShow(runtime, sample, true, false, 1000), "the host cannot overlap an outstanding view command");
    runtime[R::OutstandingSequence] = 0;
    runtime[R::ShowAcknowledgements] = 1;
    intent.Observe(runtime);
    Check(!intent.Pending() && !intent.ShouldShow(runtime, sample, true, false, 1000), "an actual manual or automatic Show acknowledgement consumes startup");
    runtime[R::ShowAcknowledgements] = 0;
    intent.Observe(runtime);
    Check(!intent.Pending(), "Quit or Hide never rearms consumed startup intent");
    I stopped(true, 7);
    stopped.Stop();
    Check(!stopped.ShouldShow(runtime, sample, true, false, 1000) && !stopped.Pending(), "explicit Stop is terminal before first eligibility too");
    I visible(true, 7);
    runtime[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Visible);
    visible.Observe(runtime);
    Check(!visible.Pending(), "observed actual visibility consumes even before a later hidden sample");
}
void TestLoadedWorldHost(deonline::CoreStartupReference& worker, const std::filesystem::path& root, const std::wstring& mode,
                         const std::filesystem::path& fixturePath)
{
    namespace N = deonline::NativeFrontendHostProtocol;
    namespace R = deonline::FrontendRuntimeReport;
    namespace A = deonline::FrontendAdmissionSample;
    const bool legacy = mode == L"loaded-world-host-legacy", manual = mode == L"loaded-world-host-manual", stopWaiting = mode == L"loaded-world-host-stop",
               missing = mode == L"loaded-world-host-missing";
    Check(legacy || manual || stopWaiting || missing || mode == L"loaded-world-host-gates", "known loaded-world host route fixture");
    Fixture fixture(950);
    Set(L"MTA_CORE_FIXTURE_RUNTIME_MODE", L"7");
    Set(L"MTA_CORE_FIXTURE_AUTO_ENABLED", L"1");
    Set(L"MTA_CORE_FIXTURE_AUTO_ADMISSION", manual || stopWaiting ? L"1" : legacy ? L"2" : L"0");
    CopyCore(fixturePath, root);
    SetEvent(fixture.release);
    deonline::NativeFrontendHostTestContext::focused.store(true);
    deonline::NativeFrontendHostTestContext::keys.store(0);
    static std::atomic<DWORD>           nativeOwner{0};
    static deonline::NativeFrontendHost host(worker, nativeOwner);
    N::RequestV1                        request;
    request.flags = legacy ? 0u : N::LoadedWorldAutoShow;
    Check(host.Start(&anchor, request), "normal host owns one explicit loaded-world frontend runtime");
    N::Snapshot hostState{};
    R::Snapshot runtime{};
    A::Snapshot sample{};
    const auto  capture = [&]()
    { return host.Read(hostState.data(), N::FieldCount) && deonline::CoreStartupReferenceTestAccess::RuntimeCache(worker, runtime); };
    const auto sampleReady = [&]() { return deonline::CoreStartupReferenceTestAccess::AdmissionCache(worker, sample); };
    const auto terminal = [&]()
    {
        return capture() && (hostState[N::HostPhase] == static_cast<std::uint32_t>(N::Phase::Stopped) ||
                             hostState[N::HostPhase] == static_cast<std::uint32_t>(N::Phase::Failed));
    };
    if (missing)
    {
        WaitLoaded(terminal, "missing auto admission export did not retire");
        Check(hostState[N::HostPhase] == static_cast<std::uint32_t>(N::Phase::Failed) &&
                  hostState[N::WorkerFailure] == static_cast<std::uint32_t>(R::Failure::Exports) && !fixture.counters[44] && !fixture.counters[47],
              "explicit auto option refuses a missing optional export before Core Begin or a view");
    }
    else
    {
        WaitLoaded([&] { return capture() && hostState[N::HostPhase] == static_cast<std::uint32_t>(N::Phase::Running); }, "host did not retain hidden Core");
        if (legacy)
        {
            Sleep(250);
            Check(capture() && !sampleReady() && !fixture.counters[63] && !fixture.counters[47],
                  "legacy manual host does not import/poll auto admission or show a menu");
        }
        else
        {
            WaitLoaded([&] { return sampleReady(); }, "new optional admission cache remained unpublished");
            const auto initialSerial = A::Pair(sample, A::SerialLow);
            Check(capture() && runtime[R::WorkerPhase] == static_cast<std::uint32_t>(R::Phase::Hidden) && !runtime[R::ShowAcknowledgements] &&
                      fixture.counters[44] == 1 && !fixture.counters[47] && !fixture.counters[54] && !fixture.counters[55],
                  "failed or deferred observation retains one healthy hidden Core without GUI or local input");
            if (!manual && !stopWaiting)
            {
                Check(sample[A::Status] == static_cast<std::uint32_t>(DefinitiveFrontend::Status::Failed) && !sample[A::Admission],
                      "first failed native observation is explicit and has no eligible kind");
                Set(L"MTA_CORE_FIXTURE_AUTO_ADMISSION", L"1");
                WaitLoaded(
                    [&]
                    {
                        return sampleReady() && A::Pair(sample, A::SerialLow) > initialSerial &&
                               sample[A::Status] == static_cast<std::uint32_t>(DefinitiveFrontend::Status::Ok) &&
                               sample[A::Admission] == static_cast<std::uint32_t>(mta::game_de::FrontendAdmission::Deferred);
                    },
                    "failed read did not advance to a genuine Deferred observation");
                Check(!fixture.counters[47] && capture() && runtime[R::WorkerPhase] == static_cast<std::uint32_t>(R::Phase::Hidden),
                      "Deferred observation does not queue a Show or create a view");
            }
            Check(capture() && std::all_of(runtime.begin() + R::Reserved56, runtime.begin() + R::HeaderCount, [](auto value) { return value == 0; }),
                  "optional auto data never repurposes legacy runtime header reserved words");
            if (!stopWaiting)
            {
                if (manual)
                {
                    // Use the actual host's unchanged three-second key path.
                    // A held manual chord blocks automatic queueing.
                    deonline::NativeFrontendHostTestContext::keys.store(7);
                    WaitLoaded([&] { return capture() && hostState[N::KeyMask] == 7; }, "host did not observe manual chord");
                    Sleep(3100);
                    Set(L"MTA_CORE_FIXTURE_AUTO_ADMISSION", L"2");
                    Sleep(150);
                    Check(!fixture.counters[47], "held manual chord retains priority over fresh automatic eligibility");
                    deonline::NativeFrontendHostTestContext::keys.store(0);
                }
                else
                    Set(L"MTA_CORE_FIXTURE_AUTO_ADMISSION", L"2");
                WaitLoaded(
                    [&]
                    {
                        return capture() && runtime[R::ShowAcknowledgements] == 1 && runtime[R::ViewsRetired] == 1 &&
                               runtime[R::WorkerPhase] == static_cast<std::uint32_t>(R::Phase::Hidden);
                    },
                    "eligible first Show was not admitted and retired by the real worker path");
                Check(fixture.counters[47] == 1 && runtime[R::ShowAcknowledgements] == 1 && runtime[R::GuiHideCount] == 1 &&
                          runtime[R::WorkerPhase] == static_cast<std::uint32_t>(R::Phase::Hidden),
                      "one actual Show ACK consumes startup before the menu's ordinary Quit/Hide retirement");
                const auto reads = fixture.counters[63];
                Sleep(350);
                Check(capture() && fixture.counters[47] == 1 && fixture.counters[63] == reads && runtime[R::ShowAcknowledgements] == 1 &&
                          fixture.counters[44] == 1 && !runtime[R::LastFailure],
                      "hidden menu stays closed after Quit and never reconstructs Core or polls another startup view");
            }
        }
        Check(host.Stop(), "explicit Stop requests the original retained owner retirement");
        WaitLoaded(terminal, "auto host failed to finish explicit Stop");
        Check(hostState[N::HostPhase] == static_cast<std::uint32_t>(N::Phase::Stopped) && hostState[N::CleanupComplete] && hostState[N::CoreStoppedVerified] &&
                  hostState[N::CoreReleasedVerified] && hostState[N::CoreUnloadedVerified],
              "same host/Core/backend retire only after their existing complete cleanup proof");
        if (stopWaiting)
        {
            Set(L"MTA_CORE_FIXTURE_AUTO_ADMISSION", L"2");
            Sleep(150);
            Check(!fixture.counters[47] && !host.Start(&anchor, request), "Stop while waiting cannot restart on later eligibility");
        }
    }
    Check(!host.Start(&anchor, request) && !hostState[N::NetworkStarted] && !hostState[N::GameplayReady] && !hostState[N::FullMtaReady] &&
              !GetModuleHandleW(L"core.dll") && !fixture.counters[2],
          "automatic frontend does not grant gameplay/network readiness or a replacement owner");
}
void TestActualLoadedWorldAdmission(deonline::CoreStartupReference& worker)
{
    namespace R = deonline::FrontendRuntimeReport;
    namespace A = deonline::FrontendAdmissionSample;
    const auto generation = worker.BeginFrontendRuntime(&anchor, false, false, true);
    Check(generation != 0, "actual rebuilt Core begins its real optional admission route");
    A::Snapshot sample{};
    WaitLoaded(
        [&]
        {
            worker.HeartbeatFrontendRuntime(generation);
            if (worker.PollFrontendRuntime() != 0)
            {
                namespace C = DefinitiveFrontendRuntime;
                namespace S = DefinitiveFrontendStartup;
                // Preserve the failure while exposing its existing value-only
                // owner snapshot, so setup faults cannot be mistaken for admission.
                const bool captured = worker.CaptureFrontendRuntime();
                throw std::runtime_error("actual Core retired before its admission observation; captured=" + std::to_string(captured) +
                                         "; runtimeFailure=" + std::to_string(worker.ReadFrontendRuntime(R::LastFailure)) +
                                         "; lastCoreStatus=" + std::to_string(worker.ReadFrontendRuntime(R::LastCoreStatus)) +
                                         "; loadError=" + std::to_string(worker.Read(18)) + "; workerError=" + std::to_string(worker.Read(16)) +
                                         "; workerPhase=" + std::to_string(worker.ReadFrontendRuntime(R::WorkerPhase)) +
                                         "; coreStage=" + std::to_string(worker.ReadFrontendRuntime(R::CoreSampleStage)) +
                                         "; owner=" + std::to_string(worker.ReadFrontendRuntime(R::WorkerThread)) +
                                         "; scriptOwner=" + std::to_string(worker.ReadFrontendRuntime(R::ScriptThread)) +
                                         "; coreLoaded=" + std::to_string(worker.ReadFrontendRuntime(R::CoreLoaded)) +
                                         "; coreRetained=" + std::to_string(worker.ReadFrontendRuntime(R::CoreOwnerRetained)) +
                                         "; startupFailure=" + std::to_string(worker.ReadFrontendRuntime(R::CoreOffset + S::LastFailure)) +
                                         "; runtimePhase=" + std::to_string(worker.ReadFrontendRuntime(R::CoreOffset + C::RuntimePhase)) +
                                         "; coreRuntimeFailure=" + std::to_string(worker.ReadFrontendRuntime(R::CoreOffset + C::LastFailure)));
            }
            return worker.ReadFrontendAdmissionSample(generation, sample);
        },
        "actual rebuilt Core admission export did not publish");
    Check(sample[A::Version] == 1 && sample[A::RuntimeGeneration] == generation && sample[A::WorkerThread] && sample[A::WorkerThread] != GetCurrentThreadId() &&
              sample[A::Status] == static_cast<std::uint32_t>(DefinitiveFrontend::Status::Failed) && !sample[A::Admission],
          "the real rebuilt export preserves exact-image refusal rather than inventing native eligibility");
    const auto serial = A::Pair(sample, A::SerialLow);
    WaitLoaded(
        [&]
        {
            worker.HeartbeatFrontendRuntime(generation);
            return worker.CaptureFrontendRuntime() && worker.ReadFrontendAdmissionSample(generation, sample) && A::Pair(sample, A::SerialLow) > serial;
        },
        "actual hidden Core did not keep its observation lifetime");
    Check(worker.ReadFrontendRuntime(R::WorkerPhase) == static_cast<std::uint32_t>(R::Phase::Hidden) && worker.ReadFrontendRuntime(R::CoreOwnerRetained) == 1 &&
              !worker.ReadFrontendRuntime(R::LastFailure) && !worker.ReadFrontendRuntime(R::ViewAttempts),
          "failed read-only admission leaves the real completed hidden Core healthy with no view");
    for (std::uint32_t field = R::Reserved56; field < R::HeaderCount; ++field)
        Check(!worker.ReadFrontendRuntime(field), "actual legacy runtime report reserved words remain zero");
    auto        unchanged = sample;
    bool        foreignRefused = false;
    std::thread foreign([&] { foreignRefused = !worker.ReadFrontendAdmissionSample(generation, unchanged) && unchanged == sample; });
    foreign.join();
    Check(foreignRefused, "the optional cache preserves the real host's owner-thread/output contract");
    Check(worker.CommandFrontendRuntime(generation, 1, static_cast<std::uint32_t>(R::Command::Stop)),
          "actual Core receives explicit Stop through the existing mailbox");
    WaitLoaded(
        [&]
        {
            worker.HeartbeatFrontendRuntime(generation);
            return worker.PollFrontendRuntime() != 0;
        },
        "actual Core did not retire");
    Check(worker.CaptureFrontendRuntime() && worker.ReadFrontendRuntime(R::CleanupComplete) && worker.ReadFrontendRuntime(R::CoreStoppedVerified) &&
              worker.ReadFrontendRuntime(R::CoreReleasedVerified) && worker.ReadFrontendRuntime(R::CoreUnloadedVerified) && !GetModuleHandleW(L"core.dll") &&
              !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"xmll.dll") && !GetModuleHandleW(L"pthread.dll"),
          "actual rebuilt Core and exact paired backend unload after original producer retirement");
}
