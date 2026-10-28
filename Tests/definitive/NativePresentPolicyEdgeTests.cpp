/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual native-edge qualification, shared lifetime and exact forward
 ****************************************************************************/
#include "../../Client/game_de/NativeFrameBoundaryProbe.cpp"
#include "../../Client/game_de/NativePresentTransitionGate.h"
#include "NativePresentPolicyOwnedImageFixture.inl"
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace
{
    using namespace mta::game_de;
    namespace Detail = boundary_detail;
    namespace Gate = NativePresentTransitionGate;
    using NativePresentPolicyFixture::OwnedPolicyImage;
    unsigned           checks = 0, targetCalls = 0, selectorCalls = 0;
    UINT               receivedInterval = 99, receivedFlags = 0;
    HRESULT            targetResult = static_cast<HRESULT>(0x12345678);
    bool               disabled = true, releasedBeforeOriginal = false, expectClockRetained = false;
    Gate::Registration registration;
    std::uint64_t      generation = 0;
    HANDLE             lifecycleEntered = nullptr, lifecycleResume = nullptr;

    void Require(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Native policy edge failure: %s\n", message);
            std::exit(1);
        }
    }
    HRESULT STDMETHODCALLTYPE Target(IDXGISwapChain*, UINT interval, UINT flags)
    {
        ++targetCalls;
        receivedInterval = interval;
        receivedFlags = flags;
        if (!expectClockRetained)
        {
            Gate::Transition transition;
            releasedBeforeOriginal = Gate::TryAcquireTransition(100000 + generation, transition) == ClockResult::Ok;
            if (transition.Held())
                releasedBeforeOriginal = transition.Release() == ClockResult::Ok && releasedBeforeOriginal;
        }
        if (flags == 0xdead)
            RaiseException(0xe04f504f, 0, 0, nullptr);
        return targetResult;
    }
    std::uint32_t Select(const NativePresentPolicyScope& selected, std::uintptr_t chain, std::uintptr_t target, std::uint32_t incoming, std::uint32_t flags,
                         const void* caller, unsigned depth) noexcept
    {
        ++selectorCalls;
        (void)flags;
        Gate::Read read;
        if (Gate::TryRead(registration, read) != ClockResult::Ok)
            return incoming;
        const bool qualified = QualifyNativePresentPolicyEdge(selected, chain, target, caller, depth);
        const auto result = qualified && disabled ? 0u : incoming;
        // Target can acquire a Clock transition only if this actual short read
        // has already left. It is never held over the original or COM work.
        if (read.Release() != ClockResult::Ok)
            return incoming;
        return result;
    }
    bool Qualified(NativePresentPolicyScope& selected, OwnedPolicyImage& fixture, unsigned depth = 1, const void* caller = nullptr)
    {
        Gate::Read read;
        if (Gate::TryRead(registration, read) != ClockResult::Ok)
            return false;
        const bool result = QualifyNativePresentPolicyEdge(selected, reinterpret_cast<std::uintptr_t>(fixture.Chain()),
                                                           reinterpret_cast<std::uintptr_t>(&Target), caller ? caller : fixture.Caller(), depth);
        Require(read.Release() == ClockResult::Ok, "qualification releases the actual M short read");
        return result;
    }
    void Begin(OwnedPolicyImage& fixture)
    {
        ++generation;
        Require(fixture.ConfigureOwnedPolicyImage(&Target), "owned image, relay, real module ref and current raw tuple prepared");
        registration = {};
        Require(Gate::RegisterOwner(generation, registration) == ClockResult::Ok, "actual M owner registers outside callback");
        Require(ConfigureNativePresentPolicy(generation, &Select), "actual configured native policy generation admitted");
        Require(NativePolicyHooksCurrent(generation), "unchanged MEM_IMAGE and owned relay guards admit current fixture profiles");
    }
    void End()
    {
        StopFrameBoundaryAdmission();
        Require(FrameBoundaryDiagnosticInFlight() == 0, "diagnostic scopes drained before final owner retirement");
        Require(FrameBoundaryHooksCommitted(false), "real detach acknowledgement closes diagnostic admission");
        Require(Gate::UnregisterOwner(registration) == ClockResult::Ok, "actual M owner unregisters after callback drain");
        Require(RetireNativePresentPolicy(generation), "configured policy retires after final detach and M unregister");
    }
    void Enter(NativePresentPolicyScope& scope, OwnedPolicyImage& fixture, unsigned depth = 1)
    {
        EnterNativePresentPolicyScope(scope, generation, fixture.image, fixture.viewport.data(), GetCurrentThreadId(), depth);
        Require(scope.entered, "real manager policy scope obtains a distinct lifetime ticket");
    }
    HRESULT Dispatch(OwnedPolicyImage& fixture, UINT incoming = 7, UINT flags = 0x81)
    {
        Detail::dispatchDepth = 1;
        const auto result = Detail::PresentBody(fixture.Chain(), incoming, flags, fixture.Caller(), &Target, true);
        Detail::dispatchDepth = 0;
        return result;
    }
    bool CatchActualHook(OwnedPolicyImage* fixture)
    {
        __try
        {
            Detail::HookNativeDispatch(fixture->Chain(), 7, 0xdead, &Target);
        }
        __except (GetExceptionCode() == 0xe04f504f ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }
    void BlockingResize(void*, UINT, UINT, bool)
    {
        SetEvent(lifecycleEntered);
        WaitForSingleObject(lifecycleResume, INFINITE);
    }
    void ThrowingNotify(void*)
    {
        RaiseException(0xe04f5050, 0, 0, nullptr);
    }
    bool CatchNotify()
    {
        __try
        {
            Detail::HookNotify(nullptr);
        }
        __except (GetExceptionCode() == 0xe04f5050 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    void CheckNativeEdge(OwnedPolicyImage& fixture)
    {
        Begin(fixture);
        NativePresentPolicyScope scope;
        Enter(scope, fixture);
        Require(Qualified(scope, fixture), "fresh actual typed raw roots adopt one current viewport without GPU/custom/COM evidence");
        Require(Detail::runtime.policyViewport.load() == reinterpret_cast<std::uintptr_t>(fixture.viewport.data()), "first fresh viewport adopted");
        const auto before = targetCalls;
        Require(Dispatch(fixture) == targetResult && targetCalls == before + 1 && receivedInterval == 0 && receivedFlags == 0x81 && releasedBeforeOriginal,
                "only final interval changes; actual target called once, flags/result preserved, M released before original");
        disabled = false;
        Require(Dispatch(fixture, 0xfedcba98, 0xabcdef01) == targetResult && receivedInterval == 0xfedcba98 && receivedFlags == 0xabcdef01,
                "enabled vsync preserves game's full DWORD interval and all flags");
        disabled = true;
        Require(!Qualified(scope, fixture, 2) && !Qualified(scope, fixture, 1, reinterpret_cast<void*>(std::uintptr_t{1})),
                "nested and foreign caller edges refuse");
        NativePresentPolicyScope nested;
        Enter(nested, fixture, 2);
        Require(!Qualified(nested, fixture), "nested natural wrapper lifetime cannot authorize interval policy");
        LeaveNativePresentPolicyScope(nested);
        Gate::Transition retained;
        Require(Gate::TryAcquireTransition(200000 + generation, retained) == ClockResult::Ok, "Clock can retain its real transition");
        expectClockRetained = true;
        Require(Dispatch(fixture, 11, 0x102) == targetResult && receivedInterval == 11 && receivedFlags == 0x102,
                "Clock-retained native edge passively forwards actual interval and flags");
        expectClockRetained = false;
        Require(retained.Release() == ClockResult::Ok, "Clock release reopens policy reads");
        Require(Dispatch(fixture) == targetResult && receivedInterval == 0, "same installed policy resumes after Clock without another hook");
        const auto calls = targetCalls;
        Require(CatchActualHook(&fixture) && targetCalls == calls + 1 && Detail::dispatchDepth == 0 && Detail::runtime.inFlight.load() == 0 &&
                    receivedFlags == 0xdead,
                "actual HookNativeDispatch forwards original SEH exactly once and drains its finally counters");
        Require(!RetireNativePresentPolicy(generation) && !ConfigureNativePresentPolicy(generation + 1, &Select),
                "entered original scope prevents retirement and generation replacement");
        LeaveNativePresentPolicyScope(scope);
        Require(!RetireNativePresentPolicy(generation + 1), "different live generation cannot retire another policy");
        End();
    }
    void CheckFreshRefusals(OwnedPolicyImage& fixture)
    {
        for (unsigned mode = 0; mode < 7; ++mode)
        {
            Begin(fixture);
            NativePresentPolicyScope scope;
            Enter(scope, fixture);
            Require(Qualified(scope, fixture), "fresh generation starts from current tuple");
            if (mode == 0)
                OwnedPolicyImage::Put(fixture.owner.data(), 0x8a8, reinterpret_cast<std::uintptr_t>(fixture.owner.data()));
            if (mode == 1)
                fixture.chainTable[8] = reinterpret_cast<std::uintptr_t>(&OwnedPolicyImage::Noop);
            if (mode == 2)
                OwnedPolicyImage::Put(fixture.viewport.data(), 0x60, std::uintptr_t{0});
            if (mode == 3)
                ownedPolicyImageCode[512 + 20] ^= 1;
            if (mode == 4)
                Detail::HookDestroy(fixture.viewport.data());
            if (mode == 5)
                Detail::runtime.modules[0].base ^= 1;
            if (mode == 6)
            {
                targetResult = DXGI_ERROR_DEVICE_REMOVED;
                Require(Dispatch(fixture) == DXGI_ERROR_DEVICE_REMOVED, "native device loss HRESULT preserved");
                targetResult = static_cast<HRESULT>(0x12345678);
            }
            Require(!Qualified(scope, fixture) && NativePresentPolicyPoisoned(generation),
                    "adopted root/target/code/cache/destruction/device-loss change poisons generation");
            fixture.RestoreViewportTarget(&Target);
            Require(Dispatch(fixture, 13, 0x104) == targetResult && receivedInterval == 13 && receivedFlags == 0x104,
                    "poisoned generation keeps exact passive forwarding");
            LeaveNativePresentPolicyScope(scope);
            End();
        }
    }
    void CheckPreconfigurationAndSharedDiagnostic(OwnedPolicyImage& fixture)
    {
        ++generation;
        Require(fixture.ConfigureOwnedPolicyImage(&Target), "prepared native hooks exist before Configure");
        registration = {};
        Require(Gate::RegisterOwner(generation, registration) == ClockResult::Ok, "M registers before publishing selector");
        lifecycleEntered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        lifecycleResume = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Require(lifecycleEntered && lifecycleResume, "owned lifecycle synchronization created");
        Detail::runtime.resize = &BlockingResize;
        std::thread original([&] { Detail::HookResize(fixture.viewport.data(), 1280, 720, false); });
        Require(WaitForSingleObject(lifecycleEntered, 5000) == WAIT_OBJECT_0 && Detail::runtime.policyLifecycle.load() == 1 &&
                    FrameBoundaryDiagnosticInFlight() == 0,
                "policy-only preconfiguration original counts atomically without diagnostic hash/lock ticket");
        const auto epoch = Detail::runtime.policyEpoch.load();
        Require(ConfigureNativePresentPolicy(generation, &Select) && Detail::runtime.policyLifecycle.load() == 1 && Detail::runtime.policyEpoch.load() == epoch,
                "Configure preserves executing preconfiguration lifecycle and does not require unrelated global original drain");
        NativePresentPolicyScope scope;
        Enter(scope, fixture);
        Require(!Qualified(scope, fixture) && !NativePresentPolicyPoisoned(generation), "active preconfiguration lifecycle defers first adoption");
        const auto cached = Detail::runtime.modules[0].retained;
        const auto relay = Detail::runtime.dispatchRelay;
        Require(BeginSharedFrameBoundaryDiagnostic(generation + 100, true) && Detail::runtime.modules[0].retained == cached &&
                    Detail::runtime.dispatchRelay == relay,
                "shared diagnostic begin preserves actual common module/relay and independent policy epoch");
        FrameBoundaryScope               blocked;
        CustomPresentClassificationState custom;
        EnterFrameBoundaryScope(blocked, fixture.image, fixture.viewport.data(), GetCurrentThreadId(), custom);
        Require(!blocked.diagnosticCounted && FrameBoundaryDiagnosticInFlight() == 0, "shared diagnostic refuses prebegin policy-only lifecycle original");
        SetEvent(lifecycleResume);
        original.join();
        Require(Detail::runtime.policyLifecycle.load() == 0 && Detail::runtime.inFlight.load() == 0 && Qualified(scope, fixture),
                "same new policy admits only after preconfiguration lifecycle original drains");
        FrameBoundaryScope admitted;
        EnterFrameBoundaryScope(admitted, fixture.image, fixture.viewport.data(), GetCurrentThreadId(), custom);
        Require(admitted.diagnosticCounted && FrameBoundaryDiagnosticInFlight() == 1, "shared diagnostic actual scope owns one closeable lifetime ticket");
        StopFrameBoundaryAdmission();
        Require(!BeginSharedFrameBoundaryDiagnostic(generation + 101, true), "new shared diagnostic refuses while old scope remains counted");
        // The callback entered before Stop and owns its scope until Leave. An
        // independent lifecycle original after closure must still invalidate it.
        Detail::runtime.notify = &OwnedPolicyImage::Noop;
        Detail::HookNotify(nullptr);
        admitted.nativeDispatch.status = static_cast<unsigned>(FrameBoundaryObservationStatus::ObservedCoherent);
        admitted.nativeDispatch.factsMask = 255;
        LeaveFrameBoundaryScope(admitted);
        Require(admitted.nativeDispatch.status == static_cast<unsigned>(FrameBoundaryObservationStatus::ResizeOverlap) &&
                    admitted.nativeDispatch.factsMask == 127 && FrameBoundaryDiagnosticInFlight() == 0,
                "independent policy lifecycle bookend invalidates completed diagnostic before publication");
        Require(BeginSharedFrameBoundaryDiagnostic(generation + 102, true), "fresh shared diagnostic begins only after full scope drain");
        Detail::runtime.notify = &ThrowingNotify;
        Require(CatchNotify() && FrameBoundaryDiagnosticInFlight() == 0 && Detail::runtime.policyLifecycle.load() == 0 && Detail::runtime.inFlight.load() == 0,
                "actual shared diagnostic lifecycle SEH drains both gates in finally");
        StopFrameBoundaryAdmission();
        LeaveNativePresentPolicyScope(scope);
        End();
        CloseHandle(lifecycleEntered);
        CloseHandle(lifecycleResume);
        lifecycleEntered = lifecycleResume = nullptr;
        Require(RetireNativePresentPolicy(generation + 1), "never-configured failed manager epoch retires without fabricated setup");
    }
}
int main()
{
    OwnedPolicyImage fixture;
    Require(fixture.valid, "owned MEM_IMAGE executable section and system DXGI module available");
    CheckNativeEdge(fixture);
    CheckFreshRefusals(fixture);
    CheckPreconfigurationAndSharedDiagnostic(fixture);
    Require(selectorCalls != 0, "actual final native body invokes supplied manager selector");
    std::printf("Passed %u native present policy edge checks\n", checks);
    std::puts(
        "actualNativePolicyFreshRootsTested=1\nactualNativePolicyExactForwardTested=1\nactualNativePolicyClockReadReleaseTested="
        "1\nactualNativePolicyPoisonTested=1\nactualNativePolicySharedDiagnosticLifetimeTested=1\nactualNativePolicyPreconfigurationLifecycleTested=1");
    return 0;
}
