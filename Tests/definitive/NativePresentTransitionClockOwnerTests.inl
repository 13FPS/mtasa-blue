/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Present decision exclusion through actual clock peer transactions
 ****************************************************************************/
namespace PresentTransitionClockFixture
{
    namespace Gate = NativePresentTransitionGate;
    struct Peer
    {
        HANDLE             stop = nullptr, handle = nullptr;
        std::atomic<DWORD> id{0};
        std::thread        worker;
        Peer()
        {
            stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Check(stop != nullptr, "create the disposable actual peer's owned event");
            worker = std::thread(
                [&]
                {
                    id.store(GetCurrentThreadId());
                    WaitForSingleObject(stop, INFINITE);
                });
            while (!id.load())
                SwitchToThread();
            handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, id.load());
            Check(handle != nullptr, "observe only the disposable fixture peer's real suspend count");
        }
        ~Peer()
        {
            SetEvent(stop);
            worker.join();
            CloseHandle(handle);
            CloseHandle(stop);
        }
    };
    void Open(const Gate::Registration& owner)
    {
        Gate::Read read;
        Check(Gate::TryRead(owner, read) == ClockResult::Ok && read.Held(), "the actual registered policy gate admits one short decision read");
        Check(read.Release() == ClockResult::Ok && !read.Held(), "the actual short decision read retires before native Present would run");
    }
    void Closed(const Gate::Registration& owner)
    {
        Gate::Read read;
        Check(Gate::TryRead(owner, read) == ClockResult::Busy && !read.Held(), "the actual retained peer transition denies a new policy read without waiting");
    }
    void Run(ClockImageFixture& fixture)
    {
        // An unregistered presenter keeps the existing clock semantics. A
        // transition still closes registration before it can park any peer.
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, false);
            PeerHold hold;
            Check(hold.Prepare() && hold.Suspend(), "actual peer ownership works with no registered presenter");
            Gate::Registration late;
            Check(Gate::RegisterOwner(10, late) == ClockResult::Busy && !late.IsSet(), "late registration cannot race an actual unregistered clock transition");
            Check(hold.Release() && service.End() && !service.IsRetained(), "unregistered transition retires before its actual clock admission");
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto state = Execute(service, fixture, ClockOperation::Set);
            Check(state.lastCommandResult == ClockResult::Ok && state.sets == 1, "absence of a presenter preserves the actual native SDK Set route");
            FinishOwned(service, fixture);
        }
        Gate::Registration owner;
        Check(Gate::RegisterOwner(42, owner) == ClockResult::Ok && owner.IsSet() && Gate::RegisteredModuleRetained() &&
                  PresentTransitionModuleFault::acquisitions.load() == 1 && PresentTransitionModuleFault::acquired.load() &&
                  !PresentTransitionModuleFault::pinRequested.load() && !PresentTransitionModuleFault::releaseAttempts.load(),
              "register the actual gate code lifetime without a native provider or rendering capability");
        Open(owner);
        {
            NativeClockService presentTransitionBusyService;
            BeginOwned(presentTransitionBusyService, fixture);
            Gate::Read presentTransitionBusyRead;
            Check(Gate::TryRead(owner, presentTransitionBusyRead) == ClockResult::Ok, "hold a real policy decision across one actual SDK Set attempt");
            const auto presentTransitionBusyWrites = ClockTestFaults::clockDataWrites.load();
            const auto presentTransitionBusyPatches = ClockTestFaults::clockCodeExchanges.load();
            const auto presentTransitionBusySuspends = ClockTestFaults::suspendCalls.load();
            const auto presentTransitionBusy = Execute(presentTransitionBusyService, fixture, ClockOperation::Set);
            Check(presentTransitionBusy.lastCommandResult == ClockResult::Busy && presentTransitionBusy.active && !runtime.stopping.load() &&
                      !runtime.nativeTouched.load() && !presentTransitionBusy.dataRecoveryPending && !presentTransitionBusy.patchRecoveryPending &&
                      !runtime.presentationRecovery.Held() && !runtime.peerBusy.load() && !runtime.peersRetained.load() &&
                      ClockTestFaults::clockDataWrites.load() == presentTransitionBusyWrites &&
                      ClockTestFaults::clockCodeExchanges.load() == presentTransitionBusyPatches &&
                      ClockTestFaults::suspendCalls.load() == presentTransitionBusySuspends,
                  "the actual SDK mutator reports policy-reader Busy without stopping, parking, writing or inventing recovery debt");
            Check(presentTransitionBusyRead.Release() == ClockResult::Ok && presentTransitionBusyService.Renew(runtime.generation.load()),
                  "release the short decision and renew the same genuine clock owner before retrying");
            const auto presentTransitionRetried = Execute(presentTransitionBusyService, fixture, ClockOperation::Set, 2);
            Check(presentTransitionRetried.lastCommandResult == ClockResult::Ok && presentTransitionRetried.sets == 1 && presentTransitionRetried.active,
                  "the same actual SDK owner accepts a fresh Set after the policy reader retires");
            FinishOwned(presentTransitionBusyService, fixture);
            Open(owner);
        }
        {
            // Only the fresh command's real policy-reader refusal is retryable.
            // An older Toolhelp failure also returns Busy, but it must retain
            // the original Stop behavior instead of inheriting that provenance.
            NativeClockService presentTransitionEnumerationService;
            BeginOwned(presentTransitionEnumerationService, fixture);
            const auto presentTransitionEnumerationWrites = ClockTestFaults::clockDataWrites.load();
            const auto presentTransitionEnumerationPatches = ClockTestFaults::clockCodeExchanges.load();
            ClockTestFaults::failEnumeration.store(true);
            const auto presentTransitionEnumeration = Execute(presentTransitionEnumerationService, fixture, ClockOperation::Set);
            Check(presentTransitionEnumeration.lastCommandResult == ClockResult::Busy && !presentTransitionEnumeration.active && runtime.stopping.load() &&
                      presentTransitionEnumeration.failure == ClockResult::Busy && !runtime.presentationRecovery.Held() && !runtime.peersRetained.load() &&
                      !runtime.peerBusy.load() && ClockTestFaults::clockDataWrites.load() == presentTransitionEnumerationWrites &&
                      ClockTestFaults::clockCodeExchanges.load() == presentTransitionEnumerationPatches,
                  "a genuine older Toolhelp Busy still stops the actual SDK command and cannot borrow policy-read retry provenance");
            ClockTestFaults::failEnumeration.store(false);
            FinishOwned(presentTransitionEnumerationService, fixture);
            Open(owner);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, false);
            Gate::Read read;
            Check(Gate::TryRead(owner, read) == ClockResult::Ok && read.Held(), "hold one real short policy read before a clock transaction");
            const auto suspends = ClockTestFaults::suspendCalls.load();
            const auto writes = ClockTestFaults::clockDataWrites.load(), patches = ClockTestFaults::clockCodeExchanges.load();
            ClockTestFaults::failEnumeration.store(true);
            PeerHold refused;
            Check(!refused.Prepare() && !refused.Held() && ClockTestFaults::failEnumeration.load() && ClockTestFaults::suspendCalls.load() == suspends &&
                      refused.Release(),
                  "real PeerHold refuses the live reader before enumeration, thread parking or transition ownership");
            Check(!ChangeHooks(true) && !runtime.hooked.load() && ClockTestFaults::failEnumeration.load() && ClockTestFaults::suspendCalls.load() == suspends &&
                      ClockTestFaults::clockDataWrites.load() == writes && ClockTestFaults::clockCodeExchanges.load() == patches && CheckBinding(true),
                  "actual Detours preparation refuses a live policy reader with native code and data unchanged");
            ClockTestFaults::failEnumeration.store(false);
            Check(read.Release() == ClockResult::Ok, "release the original policy reader without lending it to the clock owner");
            Peer     peer;
            PeerHold held;
            Check(held.Prepare() && held.Suspend() && held.Held(), "actual Prepare and Suspend hold the disposable peer before closing the transaction");
            Closed(owner);
            Gate::Registration contender;
            Check(Gate::RegisterOwner(43, contender) != ClockResult::Ok && !contender.IsSet() && Gate::UnregisterOwner(owner) == ClockResult::Busy,
                  "registration and registered lifetime retirement cannot race an actual held peer transition");
            Check(held.Release(), "actual peer resume retires the clock transition");
            Open(owner);
            const auto count = ::SuspendThread(peer.handle);
            Check(count == 0 && ::ResumeThread(peer.handle) == 1, "independent native suspend-count probe confirms the peer really resumed");
            Check(service.End() && !service.IsRetained(), "read-only unhooked fixture relinquishes its exact native admission");
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, false);
            ClockTestFaults::failEnumeration.store(true);
            PeerHold enumeration;
            Check(!enumeration.Prepare(), "inject only the real Toolhelp enumeration boundary failure");
            Closed(owner);
            Check(enumeration.Release(), "enumeration failure retires its acquired transition through actual Release");
            Open(owner);
            Peer peer;
            ClockTestFaults::suspendTarget.store(peer.id.load());
            ClockTestFaults::failSuspends.store(1);
            PeerHold partial;
            Check(partial.Prepare() && !partial.Suspend() && !ClockTestFaults::failSuspends.load(),
                  "actual peer enumeration reaches the one deliberately failed SuspendThread boundary");
            Closed(owner);
            Check(partial.Release() && !runtime.peersRetained.load() && runtime.recoveryPeers.empty(),
                  "partial suspension failure resumes every acquired native peer before releasing the gate");
            ClockTestFaults::suspendTarget.store(0);
            Open(owner);
            Check(service.End() && !service.IsRetained(), "partial suspension refusal leaves no clock or gate debt");
        }
        std::puts("Production present transition actual peer hold and reader exclusion completed");

        {
            NativeClockService service;
            BeginOwned(service, fixture);
            Peer peer;
            ClockTestFaults::resumeTarget.store(peer.id.load());
            ClockTestFaults::failResumes.store(64);
            const auto before = State(service);
            Check(service.Submit(Command(before, 1, ClockOperation::Set)), "queue one actual SDK Set before the native resume fault");
            fixture.invoke();
            const auto state = State(service);
            Check(state.failure == ClockResult::RestoreFailure && state.retainedPeerThreads == 1 && state.dataRecoveryPending &&
                      runtime.presentationRecovery.Held() && runtime.presentationRecovery.OwnerGeneration() == runtime.generation.load() && !service.End() &&
                      service.IsRetained(),
                  "real failed ResumeThread transfers the exact transition with suspended-peer and native-data recovery ownership");
            Closed(owner);
            Check(Gate::UnregisterOwner(owner) == ClockResult::Busy, "retained native peer debt keeps the registered module lifetime owned across End");
            const auto count = ::SuspendThread(peer.handle);
            Check(count == 1 && ::ResumeThread(peer.handle) == 2, "the retained actual peer remains suspended rather than being falsely acknowledged");
            ClockTestFaults::failResumes.store(0);
            fixture.invoke();
            const auto recovered = State(service);
            Check(!recovered.retainedPeerThreads && runtime.recoveryPeers.empty() && !runtime.presentationRecovery.Held() && recovered.restored &&
                      recovered.lastCompletedSequence == 1 && recovered.lastCommandResult == ClockResult::RestoreFailure,
                  "actual native callback recovery resumes the exact peer and retires its transferred transition only after verified restoration");
            Open(owner);
            FinishOwned(service, fixture);
            ClockTestFaults::resumeTarget.store(0);
        }
        std::puts("Production present transition actual peer suspension recovery completed");

        {
            NativeClockService service;
            BeginOwned(service, fixture);
            Peer peer;
            ClockTestFaults::failProtectionRestores.store(true);
            const auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::PatchFailure && state.patchRecoveryPending && state.protectionRestorePending &&
                      !state.retainedPeerThreads && runtime.recoveryPeers.empty() && runtime.presentationRecovery.Held() && !service.End() &&
                      service.IsRetained(),
                  "actual unsafe executable protection retains its transition even after every native peer resumed");
            Closed(owner);
            Check(Gate::UnregisterOwner(owner) == ClockResult::Busy, "unsafe patch recovery cannot retire the registered lifetime or reopen policy reads");
            ClockTestFaults::failProtectionRestores.store(false);
            FinishOwned(service, fixture);
            Check(!runtime.presentationRecovery.Held() && !runtime.patchRecovery.load() && !runtime.protectionPending.load(),
                  "actual native protection and branch recovery retire the original transition without a replacement owner");
            Open(owner);
        }
        std::puts("Production present transition actual patch protection recovery completed");

        {
            // Execute one exact native Fog route, never the prior seven-group
            // matrix. Only its final OS cache-flush result is fault scheduled;
            // real allocation, code CAS, protection, owner and recovery run.
            fixture.AdmitWeatherPins();
            fixture.AdmitWeatherScalarPins();
            fixture.AdmitRainPins();
            const auto presentTransitionPriorImage = ClockReaderTestHost::image;
            ClockReaderTestHost::image = fixture.image;
            const NativeWeatherOverrideAtomicProfile::Window* presentTransitionFogWindow = nullptr;
            for (const auto& presentTransitionCandidate : NativeWeatherOverrideAtomicProfile::Windows)
                if (presentTransitionCandidate.field == NativeWeatherScalarProfile::Field::Fog)
                {
                    presentTransitionFogWindow = &presentTransitionCandidate;
                    break;
                }
            Check(presentTransitionFogWindow && presentTransitionFogWindow->windowSize == 8,
                  "the actual atomic profile supplies the bounded native Fog cache-fault window");
            const auto             presentTransitionStockWord = fixture.Get<std::uint64_t>(presentTransitionFogWindow->windowRva);
            NativeClockService     presentTransitionWeatherService;
            WeatherOverrideContext presentTransitionWeatherContext;
            presentTransitionWeatherContext.sdkIndex = 2;
            presentTransitionWeatherContext.field = NativeWeatherScalarProfile::Field::Fog;
            presentTransitionWeatherContext.value = 0.875f;
            BeginOwned(presentTransitionWeatherService, fixture, false, true, true, &WeatherOverrideHandler, &presentTransitionWeatherContext);
            Peer presentTransitionWeatherPeer;
            PresentTransitionWeatherFault::address.store(fixture.image + presentTransitionFogWindow->windowRva);
            PresentTransitionWeatherFault::failures.store(0);
            PresentTransitionWeatherFault::fail.store(true);
            fixture.invokeTimer();
            const auto presentTransitionWeatherDebt = State(presentTransitionWeatherService);
            Check(!presentTransitionWeatherContext.sdkResult && PresentTransitionWeatherFault::failures.load() && !runtime.weatherOverrides.UnsafeCode() &&
                      runtime.weatherOverrides.NeedsRecovery() && runtime.weatherOverrides.HasReferences() && !runtime.weatherOverridePeers.Held() &&
                      !runtime.peersRetained.load() && !presentTransitionWeatherDebt.retainedPeerThreads && runtime.recoveryPeers.empty() &&
                      runtime.presentationRecovery.Held() && !presentTransitionWeatherService.End() && presentTransitionWeatherService.IsRetained(),
                  "actual weather cache debt keeps the original transition closed after all peers resumed despite UnsafeCode being false");
            Closed(owner);
            Check(Gate::UnregisterOwner(owner) == ClockResult::Busy,
                  "genuine published NeedsRecovery prevents registered lifetime retirement while native weather cache debt remains");
            const auto presentTransitionWeatherCalls = presentTransitionWeatherContext.calls;
            PresentTransitionWeatherFault::fail.store(false);
            fixture.invokeTimer();
            Check(presentTransitionWeatherService.End() && !presentTransitionWeatherService.IsRetained() && !runtime.weatherOverrides.NeedsRecovery() &&
                      !runtime.weatherOverrides.HasReferences() && !runtime.presentationRecovery.Held() && !WeatherOverrideRetained() &&
                      presentTransitionWeatherContext.calls == presentTransitionWeatherCalls &&
                      fixture.Get<std::uint64_t>(presentTransitionFogWindow->windowRva) == presentTransitionStockWord,
                  "actual native weather recovery flushes and restores the same writer, retires scratch and gate debt, and does not reenter the setter");
            PresentTransitionWeatherFault::address.store(0);
            ClockReaderTestHost::image = presentTransitionPriorImage;
            Open(owner);
        }
        {
            // Healthy accepted Fog references still need asynchronous cleanup.
            // A short policy read must defer that actual native cleanup before
            // any write, rather than becoming a restoration error.
            const auto presentTransitionCleanupPriorImage = ClockReaderTestHost::image;
            ClockReaderTestHost::image = fixture.image;
            const NativeWeatherOverrideAtomicProfile::Window* presentTransitionCleanupWindow = nullptr;
            for (const auto& presentTransitionCleanupCandidate : NativeWeatherOverrideAtomicProfile::Windows)
                if (presentTransitionCleanupCandidate.field == NativeWeatherScalarProfile::Field::Fog)
                {
                    presentTransitionCleanupWindow = &presentTransitionCleanupCandidate;
                    break;
                }
            Check(presentTransitionCleanupWindow && presentTransitionCleanupWindow->windowSize == 8,
                  "reuse the exact native Fog profile for the bounded cleanup-Busy case");
            const auto             presentTransitionCleanupStock = fixture.Get<std::uint64_t>(presentTransitionCleanupWindow->windowRva);
            NativeClockService     presentTransitionCleanupService;
            WeatherOverrideContext presentTransitionCleanupContext;
            presentTransitionCleanupContext.sdkIndex = 2;
            presentTransitionCleanupContext.field = NativeWeatherScalarProfile::Field::Fog;
            presentTransitionCleanupContext.value = 0.9375f;
            BeginOwned(presentTransitionCleanupService, fixture, false, true, true, &WeatherOverrideHandler, &presentTransitionCleanupContext);
            PresentTransitionWeatherFault::address.store(fixture.image + presentTransitionCleanupWindow->windowRva);
            PresentTransitionWeatherFault::fail.store(false);
            PresentTransitionWeatherFault::calls.store(0);
            fixture.invokeTimer();
            Check(presentTransitionCleanupContext.sdkResult && runtime.weatherOverrides.HasReferences() && !runtime.weatherOverrides.NeedsRecovery() &&
                      PresentTransitionWeatherFault::calls.load() && !runtime.presentationRecovery.Held(),
                  "the actual SDK accepts one healthy Fog override before the cleanup decision-read conflict");
            const auto presentTransitionCleanupRedirected = fixture.Get<std::uint64_t>(presentTransitionCleanupWindow->windowRva);
            const auto presentTransitionCleanupScalar = fixture.Get<std::uint32_t>(NativeWeatherScalarProfile::Fields[3]);
            const auto presentTransitionCleanupState = State(presentTransitionCleanupService);
            const auto presentTransitionCleanupFlushes = PresentTransitionWeatherFault::calls.load();
            const auto presentTransitionCleanupWrites = ClockTestFaults::clockDataWrites.load();
            const auto presentTransitionCleanupPatches = ClockTestFaults::clockCodeExchanges.load();
            const auto presentTransitionCleanupCalls = presentTransitionCleanupContext.calls;
            Gate::Read presentTransitionCleanupRead;
            Check(Gate::TryRead(owner, presentTransitionCleanupRead) == ClockResult::Ok && !presentTransitionCleanupService.End() &&
                      presentTransitionCleanupService.IsRetained(),
                  "End retains the genuine healthy override while one short presentation decision read remains live");
            fixture.invokeTimer();
            const auto presentTransitionCleanupDeferred = State(presentTransitionCleanupService);
            Check(presentTransitionCleanupDeferred.restoreFailures == presentTransitionCleanupState.restoreFailures &&
                      presentTransitionCleanupDeferred.failure != ClockResult::RestoreFailure && !presentTransitionCleanupDeferred.restored &&
                      runtime.weatherOverrides.HasReferences() && !runtime.weatherOverrides.NeedsRecovery() && !runtime.presentationRecovery.Held() &&
                      !runtime.weatherOverridePeers.Held() && !runtime.peerBusy.load() &&
                      presentTransitionCleanupContext.calls == presentTransitionCleanupCalls &&
                      PresentTransitionWeatherFault::calls.load() == presentTransitionCleanupFlushes &&
                      ClockTestFaults::clockDataWrites.load() == presentTransitionCleanupWrites &&
                      ClockTestFaults::clockCodeExchanges.load() == presentTransitionCleanupPatches &&
                      fixture.Get<std::uint64_t>(presentTransitionCleanupWindow->windowRva) == presentTransitionCleanupRedirected &&
                      fixture.Get<std::uint32_t>(NativeWeatherScalarProfile::Fields[3]) == presentTransitionCleanupScalar,
                  "actual RestoreOriginal defers weather cleanup on reader Busy without native cleanup writes, new debt or false RestoreFailure");
            Check(presentTransitionCleanupRead.Release() == ClockResult::Ok, "retire the exact short read before retrying asynchronous native weather cleanup");
            fixture.invokeTimer();
            Check(presentTransitionCleanupService.End() && !presentTransitionCleanupService.IsRetained() && !runtime.weatherOverrides.HasReferences() &&
                      !runtime.weatherOverrides.NeedsRecovery() && !runtime.presentationRecovery.Held() && !WeatherOverrideRetained() &&
                      presentTransitionCleanupContext.calls == presentTransitionCleanupCalls &&
                      PresentTransitionWeatherFault::calls.load() > presentTransitionCleanupFlushes &&
                      fixture.Get<std::uint64_t>(presentTransitionCleanupWindow->windowRva) == presentTransitionCleanupStock &&
                      fixture.Get<std::uint32_t>(NativeWeatherScalarProfile::Fields[3]) == presentTransitionCleanupScalar,
                  "the next actual callback cleans the original Fog code after the read retires and preserves its accepted scalar intent");
            PresentTransitionWeatherFault::address.store(0);
            ClockReaderTestHost::image = presentTransitionCleanupPriorImage;
            Open(owner);
        }
        std::puts("Production present transition actual weather cleanup Busy deferred completed");
        std::puts("Production present transition actual weather cache recovery completed");

        {
            NativeClockService                service;
            WorldGroundOwnerFixture::Callback callback;
            unsigned                          calls = 0;
            callback.action = [&](NativeGameTimingAccess&)
            {
                ++calls;
                Check(runtime.weatherOverrideHost.ParkPeers(nullptr, 0) == ClockResult::Ok && runtime.weatherOverrideHost.PeersHeld(),
                      "the genuine callback-owned weather host acquires its persistent actual PeerHold");
                Closed(owner);
                Check(!runtime.weatherOverrideHost.FinishPeers(false) && runtime.weatherOverridePeers.Held(),
                      "unsafe-to-resume veto keeps the weather member and original transition lifetime held");
                Closed(owner);
                Check(runtime.weatherOverrideHost.FinishPeers(true) && !runtime.weatherOverridePeers.Held(),
                      "the actual weather host resumes peers before retiring its persistent member transition");
                Open(owner);
                Gate::Read read;
                Check(Gate::TryRead(owner, read) == ClockResult::Ok, "hold one actual policy read before the Rain preparation route");
                ClockTestFaults::failEnumeration.store(true);
                const auto suspends = ClockTestFaults::suspendCalls.load();
                PeerHold   rain;
                Check(!rain.PrepareRain({}) && ClockTestFaults::failEnumeration.load() && ClockTestFaults::suspendCalls.load() == suspends && rain.Release(),
                      "real Rain preparation shares reader exclusion before enumeration, guard preparation or peer suspension");
                ClockTestFaults::failEnumeration.store(false);
                Check(read.Release() == ClockResult::Ok, "retire the short Rain-refusal decision read before native continuation");
            };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            Peer peer;
            fixture.invokeTimer();
            Check(calls == 1 && !runtime.weatherOverridePeers.Held() && !runtime.presentationRecovery.Held() && !WeatherOverrideRetained(),
                  "only the new bounded weather/Rain gate routing executed without native writer or weather recovery debt");
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "actual native timing callback acknowledges weather test retirement");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "weather routing retires its native hooks and admission after actual gate cleanup");
            Open(owner);
        }
        std::puts("Production present transition persistent weather and rain entry exclusion completed");
        const auto presentTransitionOwnedModule = PresentTransitionModuleFault::acquired.load();
        PresentTransitionModuleFault::failRelease.store(true);
        Check(Gate::UnregisterOwner(owner) == ClockResult::RestoreFailure && Gate::RegisteredModuleRetained() &&
                  PresentTransitionModuleFault::releaseAttempts.load() == 1 && !PresentTransitionModuleFault::releases.load() &&
                  PresentTransitionModuleFault::acquisitions.load() == 1 && PresentTransitionModuleFault::releaseGateClosed.load(),
              "one refused module release retains the exact owned reference and keeps registration/Clock exclusion during the release callback");
        Check(Gate::UnregisterOwner(owner) == ClockResult::Ok && !Gate::RegisteredModuleRetained() &&
                  PresentTransitionModuleFault::releaseAttempts.load() == 2 && PresentTransitionModuleFault::releases.load() == 1 &&
                  PresentTransitionModuleFault::released.load() == presentTransitionOwnedModule && PresentTransitionModuleFault::acquisitions.load() == 1 &&
                  PresentTransitionModuleFault::releaseGateClosed.load(),
              "retry releases exactly the acquired module handle once before the actual registration gate reopens");
        std::puts("Production present transition exact module reference retirement completed");
        Gate::Read stale;
        Check(Gate::TryRead(owner, stale) != ClockResult::Ok && !stale.Held() && !runtime.presentationRecovery.Held() && !runtime.peerBusy.load() &&
                  runtime.recoveryPeers.empty() && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "stale registration cannot borrow a read after complete native and gate retirement");
        std::puts("Production present transition clock runtime fixture completed");
    }
}
