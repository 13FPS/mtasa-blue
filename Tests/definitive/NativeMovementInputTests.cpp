/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the production native callback without a running game
 *****************************************************************************/

// Including the production translation unit gives this test access to its
// private callback state without weakening any shipping executable/hash gate.
// Synthetic memory belongs solely to this ordinary test process; Begin still
// rejects this executable before pinning a module or installing any hook.
#include "../../Client/game_de/NativeMovementInput.cpp"
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace mta::game_de;

namespace
{
    std::atomic<unsigned>             checks{0};
    NativeInputRouting::PadStateBytes physical;
    std::atomic<unsigned>             originalCalls{0};
    NativeMovementInput*              testOwner = nullptr;
    void*                             unownedPed = reinterpret_cast<void*>(0x12345);
    HANDLE                            splitEntered = nullptr, splitResume = nullptr, splitFinished = nullptr, splitNext = nullptr;
    std::atomic<unsigned>             splitStage{0};

    void Check(bool condition, const char* label)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks.load(), label);
            std::exit(1);
        }
    }

    template <typename T>
    void Put(std::uintptr_t address, const T& value)
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }

    NativeInputRouting::PadStateBytes Pad()
    {
        NativeInputRouting::PadStateBytes bytes{};
        Check(Read(runtime.image + padRva + NativePadCurrentOffset, bytes.data(), bytes.size()), "fixture pad read");
        return bytes;
    }

    bool OriginalMembership(void*, std::int32_t)
    {
        ++originalCalls;
        return false;
    }

    std::int32_t OriginalQuery(void*, std::int32_t, float* value)
    {
        ++originalCalls;
        if (value)
            *value = 0.75f;
        return 3;
    }

    void OriginalPhysical(void*)
    {
        ++originalCalls;
        Check(Pad() == physical, "forwarding observes physical pad");
    }

    void OriginalMovement(void*)
    {
        ++originalCalls;
        Check(Pad() != physical, "native original observes replacement pad");
        CControllerState current;
        Check(Read(runtime.image + padRva + NativePadCurrentOffset, current), "replacement current readable");
        Check(current.LeftStickY == -128 && current.ButtonCross == 255, "replacement uses MTA movement and sprint");
        float value = 42.0f;
        auto* provider = reinterpret_cast<void*>(runtime.provider);
        Check(HookMembership(provider, 0x1f), "membership authoritative for axis");
        Check(HookAvailability(provider, -1), "negative availability never opens physical fallback");
        Check(HookQuery(provider, 0x1f, &value) == 2 && value == -1.0f, "production route supplies first axis press");
        Check(HookQuery(provider, 2, &value) == 2 && value == 1.0f, "production route supplies sprint press");
        Check(HookQuery(provider, 0xfe, &value) == 1 && value == 0.0f, "unknown route supplies neutral state");
        // Real control is allowed to mutate its temporary pad. Restoration
        // must recover the physical snapshot despite that mutation.
        Put(runtime.image + padRva + NativePadCurrentOffset, std::uint32_t{0xabcdef01});
        Put(runtime.ped + 0x600, std::uint32_t{6});
    }

    void OriginalException(void*)
    {
        ++originalCalls;
        Check(Pad() != physical, "exception original enters with replacement");
        RaiseException(0xe0424242, 0, 0, nullptr);
    }

    void OriginalNested(void* ped)
    {
        ++originalCalls;
        if (ped != reinterpret_cast<void*>(runtime.ped))
        {
            Check(Pad() == physical, "nested unowned original sees restored physical pad");
            physical[17] = std::byte{0x45};
            Check(WritePad(physical), "nested original can change physical state");
            return;
        }
        Check(Pad() != physical, "outer replacement active before recursion");
        HookControl(unownedPed);
        Check(Pad() == physical, "outer original resumes after restored nested call");
    }

    void OriginalContextChange(void*)
    {
        ++originalCalls;
        Put(runtime.image + 0x5726120, std::int32_t{1});
    }

    void OriginalEligibilityChange(void*)
    {
        ++originalCalls;
        Check(Pad() != physical, "native eligibility transition starts inside replacement scope");
        Put(runtime.ped + 0x5fc, std::uint32_t{58});
    }

    void OriginalQueryContextChange(void*)
    {
        ++originalCalls;
        Put(runtime.image + 0x5726120, std::int32_t{1});
        float value = 0;
        Check(HookQuery(reinterpret_cast<void*>(runtime.provider), 2, &value) == 3 && value == 0.75f,
              "changed provider context forwards physical query after abort");
        Check(Pad() == physical && !runtime.transaction.Active(), "provider query restores before forwarding changed context");
    }

    void OriginalEndAttempt(void*)
    {
        ++originalCalls;
        Check(testOwner && !testOwner->End(), "End refuses teardown inside native callback");
        Check(runtime.transaction.Active() && runtime.scopes.load() == 1, "refused End retains restoration ownership");
        Check(Pad() != physical, "refused End leaves current scope for finally cleanup");
        float value = 0;
        Check(HookQuery(reinterpret_cast<void*>(runtime.provider), 2, &value) == 2 && value == 1.0f,
              "disarmed existing scope remains authoritative until restoration");
    }

    void OriginalProtectionChange(void*)
    {
        ++originalCalls;
        DWORD old = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + padRva + NativePadCurrentOffset), physical.size(), PAGE_READONLY, &old) != FALSE,
              "fixture can cause genuine restoration write failure");
    }

    void OriginalSplit(void* ped)
    {
        if (ped != reinterpret_cast<void*>(runtime.ped) || GetCurrentThreadId() != runtime.engineThread.load())
        {
            OriginalPhysical(ped);
            return;
        }
        ++originalCalls;
        const auto stage = splitStage.fetch_add(1);
        float      value = 42.0f;
        auto*      provider = reinterpret_cast<void*>(runtime.provider);
        if (!stage)
        {
            Check(HookQuery(provider, 2, &value) == 2 && value == 1.0f, "engine receives initial sprint press");
            Check(HookQuery(provider, 0x1f, &value) == 2 && value == -1.0f, "engine receives initial forward press");
        }
        else if (stage == 1)
        {
            Check(HookQuery(provider, 2, &value) == 3 && value == 1.0f, "native previous consumes held sprint once");
            Check(HookQuery(provider, 0x1f, &value) == 0 && value == 0.0f, "native previous supplies forward release");
            Check(HookQuery(provider, 0x1e, &value) == 2 && value == 1.0f, "next scope receives newly published lateral input");
        }
        else
            Check(HookQuery(provider, 2, &value) == 0 && value == 0.0f, "engine receives consumed sprint release");
        const auto installed = Pad();
        Check(SetEvent(splitEntered) != FALSE, "engine publishes scope entry");
        Check(WaitForSingleObject(splitResume, 5000) == WAIT_OBJECT_0, "engine resumes after producer publication or End");
        Check(Pad() == installed, "producer publication never changes an active native pad");
        if (!stage)
            Check(HookQuery(provider, 0x1f, &value) == 2 && value == -1.0f, "in-flight routing retains its coherent frame");
        if (stage == 2)
            Check(HookQuery(provider, 0xfe, &value) == 1 && value == 0.0f, "pending producer End keeps scope authoritative");
    }

    void OriginalStress(void*)
    {
        float value = 0;
        auto* provider = reinterpret_cast<void*>(runtime.provider);
        HookQuery(provider, 2, &value);
        HookQuery(provider, 0xfe, &value);
    }

    void OriginalEligibilityPublication(void*)
    {
        ++originalCalls;
        Check((Pad() != physical) == scope.routed, "eligibility fixture original sees only its owned pad context");
    }

    bool ReadFixtureProducerEligibility(NativeMovementInput& owner, NativeMovementEligibility& eligibility)
    {
#if defined(MTA_NATIVE_ELIGIBILITY_SAVED_OBSERVER)
        // The saved observer used GetModuleHandle(main EXE) before entering
        // this exact production helper. Supply the same synthetic image as the
        // callbacks so RED isolates borrowed context, not wrong-exe rejection.
        // Production Begin and its executable/hash checks remain unchanged.
        (void)owner;
        ApiOperation    operation;
        BindingIdentity identity;
        return operation.acquired && ReadLocalEligibility(runtime.image, {runtime.ped, runtime.provider}, nullptr, eligibility, identity);
#else
        return owner.ReadEligibility(eligibility);
#endif
    }

    void OriginalBlockedControl(void* ped)
    {
        OriginalPhysical(ped);
        if (ped == unownedPed)
        {
            Check(SetEvent(splitEntered) != FALSE, "physical control original publishes reader entry");
            Check(WaitForSingleObject(splitResume, 5000) == WAIT_OBJECT_0, "physical control original completes");
            Check(Pad() == physical, "physical control reader never observes a replacement");
        }
    }

    bool OriginalBlockedMembership(void*, std::int32_t)
    {
        Check(SetEvent(splitEntered) != FALSE, "provider publishes original entry");
        Check(WaitForSingleObject(splitResume, 5000) == WAIT_OBJECT_0, "provider resumes without an owned lock");
        return false;
    }

    std::int32_t OriginalBlockedQuery(void* provider, std::int32_t action, float*)
    {
        OriginalBlockedMembership(provider, action);
        return 1;
    }

    __declspec(noinline) bool DetourMembershipTarget(void* provider, std::int32_t action)
    {
        ++originalCalls;
        if (action == 0x55)
            return OriginalBlockedMembership(provider, action);
        return action == 0x1337;
    }

    __declspec(noinline) bool DetourAvailabilityTarget(void*, std::int32_t action)
    {
        ++originalCalls;
        return action == 0x1338;
    }

    __declspec(noinline) std::int32_t DetourQueryTarget(void*, std::int32_t, float* value)
    {
        ++originalCalls;
        if (value)
            *value = 0.75f;
        return 3;
    }

    __declspec(noinline) void DetourControlTarget(void* ped)
    {
        if (!scope.routed)
        {
            OriginalPhysical(ped);
            return;
        }
        ++originalCalls;
        Check(Pad() != physical, "genuine Detours trampoline receives scoped pad");
        MembershipFunction volatile membership = DetourMembershipTarget;
        MembershipFunction volatile availability = DetourAvailabilityTarget;
        QueryFunction volatile query = DetourQueryTarget;
        auto* provider = reinterpret_cast<void*>(runtime.provider);
        float value = 0;
        Check(membership(provider, 2) && availability(provider, 2), "genuine provider entry hooks are authoritative");
        Check(query(provider, 2, &value) == 2 && value == 1.0f, "genuine query entry supplies MTA sprint press");
    }

    void RunExceptionBoundary()
    {
        bool caught = false;
        __try
        {
            HookControl(reinterpret_cast<void*>(runtime.ped));
        }
        __except (GetExceptionCode() == 0xe0424242 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            caught = true;
        }
        Check(caught, "native original exception propagates beyond production thunk");
    }

    void Configure(ControlFunction original, bool advanceGeneration = true)
    {
        Check(!runtime.transaction.Active(), "previous scope not stranded");
        Check(!runtime.scopes.load(), "no active callback scope");
        ClearCounters();
        Check(!runtime.inFlight.load(), "previous hooks have returned");
        runtime.producerThread = GetCurrentThreadId();
        runtime.engineThread.store(GetCurrentThreadId());
        if (advanceGeneration)
            runtime.generation.fetch_add(1);
        runtime.bound.store(true);
        runtime.active.store(true);
        runtime.frame = {};
        runtime.frame.current.LeftStickY = -128;
        runtime.frame.current.ButtonCross = 255;
        scope.consumed = {};
        scope.consumedGeneration = 0;
        runtime.started = runtime.refreshed = GetTickCount64();
        runtime.originalControl = original;
        runtime.originalMembership = OriginalMembership;
        runtime.originalAvailability = OriginalMembership;
        runtime.originalQuery = OriginalQuery;
        originalCalls = 0;
        Put(runtime.image + 0x5726120, std::int32_t{0});
        Put(runtime.ped + 0x634, std::uint32_t{0});
        Put(runtime.ped + 0x5fc, std::uint32_t{1});
        Put(runtime.image + padRva + 0x30, std::uint16_t{0});
        Put(runtime.ped + 0x7c8, std::uintptr_t{0});
        Put(runtime.ped + 0x7e0, std::int32_t{0});
        Check(WritePad(physical), "reset fixture physical pad");
    }
}

int main()
{
    NativeMovementInput rejected;
    MtaInputState       neutral;
    Check(!rejected.Begin(neutral), "ordinary executable rejected");
    NativeMovementTrialState rejection;
    rejected.ReadState(rejection);
    Check(!rejection.bound && !rejection.active, "wrong executable never bound");
    Check(!rejection.modulePinned && !runtime.modulePinned.load(), "wrong executable rejection never pins callback module");
    Check(rejection.failureReason == static_cast<std::uint32_t>(Failure::UnsupportedBuild), "wrong executable has build rejection reason");
    Check(!runtime.hooked.load() && !runtime.scopes.load(), "wrong executable installs no hooks or scopes");
    Check(rejected.End() && rejected.End(), "unbound End is idempotent");
    NativeMovementInput anotherRejected;
    Check(!anotherRejected.Begin(neutral), "second ordinary executable admission rejected");
    rejected.ReadState(rejection);
    Check(rejection.failureReason == static_cast<std::uint32_t>(Failure::UnsupportedBuild), "earlier owner preserves final diagnostic");
    Check(anotherRejected.End(), "second rejection cleans up");
    NativeMovementEligibility rejectedEligibility;
    rejectedEligibility.knownMask = 0x76543210;
    Check(!rejected.ReadEligibility(rejectedEligibility) && rejectedEligibility.knownMask == 0x76543210,
          "wrong executable eligibility read preserves output without hooks");

    runtime.image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 97141760, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    runtime.ped = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    runtime.provider = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(runtime.image && runtime.ped && runtime.provider, "synthetic fixture memory allocated");
    Put(runtime.image + 0x53eb720, runtime.ped);
    Put(runtime.ped, runtime.image + 0x4319730);
    Put(runtime.image + 0x4319730 + 0x50, runtime.image + controlRva);
    Put(runtime.ped + 0x5f0, runtime.image + 0x53eb728);
    Put(runtime.ped + 0x7e0, std::int32_t{0});
    Put(runtime.image + 0x5067940, runtime.image + padRva);
    Put(runtime.image + 0x5726100, runtime.provider);
    Put(runtime.provider, runtime.image + 0x4226b20);
    const std::array<std::uintptr_t, 7> methods = {0xc09750, 0xc097d0, 0xc09810, 0xc09950, 0xc09970, 0xc09ae0, 0xae3c00};
    for (std::size_t i = 0; i < methods.size(); ++i)
        Put(runtime.image + 0x4226b20 + i * sizeof(std::uintptr_t), runtime.image + methods[i]);
    const std::array<unsigned char, 6> type2 = {0xb8, 0x02, 0, 0, 0, 0xc3};
    std::memcpy(reinterpret_cast<void*>(runtime.image + 0xae3c00), type2.data(), type2.size());
    // Reproduce the exact independently disassembled instructions in our own
    // allocation. Production Begin still rejects this executable; only the
    // private observation/callback path consumes this controlled fixture image.
    const std::array<unsigned char, 5>  baseConstructor = {0xe8, 0x59, 0xb8, 0xfc, 0xff};
    const std::array<unsigned char, 10> idleState = {0xc7, 0x87, 0xfc, 0x05, 0, 0, 1, 0, 0, 0};
    const std::array<unsigned char, 10> onFoot = {0xf7, 0x83, 0x34, 0x06, 0, 0, 0, 1, 0, 0};
    const std::array<unsigned char, 4>  controlsEnabled = {0x66, 0x39, 0x79, 0x30};
    Put(runtime.image + 0x124e002, baseConstructor);
    Put(runtime.image + 0x1219cce, idleState);
    Put(runtime.image + 0x124eb5e, onFoot);
    Put(runtime.image + 0x11579bf, controlsEnabled);
    for (const auto code : {0x124e002, 0x1219cce, 0x124eb5e, 0x11579bf})
    {
        DWORD old = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + code), 16, PAGE_EXECUTE_READ, &old) != FALSE,
              "fixture eligibility instructions are executable and read only");
    }
    for (std::size_t i = 0; i < physical.size(); ++i)
        physical[i] = static_cast<std::byte>(i + 1);

    Configure(OriginalPhysical, false);
    NativeMovementEligibility eligibility;
    BindingIdentity           identity;
    const BindingIdentity     expected{runtime.ped, runtime.provider};
    Check(ReadLocalEligibility(runtime.image, expected, nullptr, eligibility, identity) && eligibility.eligible &&
              eligibility.knownMask == NativeMovementEligibilityMask && eligibility.flags == NativeMovementEligibilityMask && eligibility.rawPedState == 1 &&
              eligibility.rawDisabledControls == 0,
          "exact local IDLE enabled context has complete positive admission");
    Check(!eligibility.context.inVehicle && !eligibility.context.isDead && identity.ped == runtime.ped && identity.provider == runtime.provider,
          "eligible context only supplies relevant on-foot and alive values");
    Check(runtime.counters.steps.load() == 0 && Pad() == physical, "eligibility observation never applies input");
    for (std::uint32_t state = 0; state <= 64; ++state)
    {
        if (state == 1)
            continue;
        Configure(OriginalPhysical, false);
        Put(runtime.ped + 0x5fc, state);
        Check(ReadLocalEligibility(runtime.image, expected, nullptr, eligibility, identity) && !eligibility.eligible && eligibility.rawPedState == state &&
                  !(eligibility.flags & static_cast<std::uint32_t>(NativeMovementEligibilityFlag::MovementState)),
              "every non-IDLE state fails closed including NONE and transitions");
        Check(eligibility.context.isDead == (state == 0x36 || state == 0x37), "only exact die/dead enum states are recognized");
        HookControl(reinterpret_cast<void*>(runtime.ped));
        Check(!runtime.active.load() && runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::IneligibleContext) &&
                  runtime.counters.steps.load() == 0 && Pad() == physical && originalCalls == 1,
              "non-IDLE native callback forwards physical input before any replacement");
    }
    for (unsigned bit = 0; bit < 16; ++bit)
    {
        Configure(OriginalPhysical, false);
        Put(runtime.image + padRva + 0x30, static_cast<std::uint16_t>(1u << bit));
        Check(ReadLocalEligibility(runtime.image, expected, nullptr, eligibility, identity) && !eligibility.eligible &&
                  eligibility.rawDisabledControls == (1u << bit) &&
                  !(eligibility.flags & static_cast<std::uint32_t>(NativeMovementEligibilityFlag::ControlsEnabled)),
              "every controls-disable bit rejects observed admission");
        HookControl(reinterpret_cast<void*>(runtime.ped));
        Check(!runtime.active.load() && runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::IneligibleContext) &&
                  runtime.counters.steps.load() == 0 && Pad() == physical,
              "disabled native callback preserves physical state and never writes");
    }
    Configure(OriginalPhysical, false);
    Check(ReadLocalEligibility(runtime.image, expected, nullptr, eligibility, identity) && eligibility.eligible,
          "producer can observe eligible frame before transition");
    Put(runtime.ped + 0x5fc, std::uint32_t{58});
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(!runtime.active.load() && runtime.counters.steps.load() == 0 && Pad() == physical, "native boundary rechecks eligibility after producer observation");
    Configure(OriginalPhysical, false);
    Put(runtime.ped + 0x634, std::uint32_t{0x100});
    Check(ReadLocalEligibility(runtime.image, expected, nullptr, eligibility, identity) && !eligibility.eligible && eligibility.context.inVehicle &&
              !(eligibility.flags & static_cast<std::uint32_t>(NativeMovementEligibilityFlag::OnFoot)),
          "vehicle flag reports an explicitly ineligible context");
    eligibility.knownMask = 0x12345678;
    const BindingIdentity wrongIdentity{runtime.ped + 8, runtime.provider};
    Check(!ReadLocalEligibility(runtime.image, wrongIdentity, nullptr, eligibility, identity) && eligibility.knownMask == 0x12345678,
          "identity mismatch preserves prior observation");
    Configure(OriginalPhysical, false);
    Put(runtime.ped + 0x5fc, UINT32_MAX);
    Check(ReadLocalEligibility(runtime.image, expected, nullptr, eligibility, identity) && !eligibility.eligible && eligibility.rawPedState == UINT32_MAX,
          "out-of-domain native state remains explicitly ineligible");
    Configure(OriginalPhysical, false);
    DWORD pinProtection = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + 0x1219cce), idleState.size(), PAGE_READWRITE, &pinProtection) != FALSE,
          "fixture can invalidate state-layout evidence");
    Put(runtime.image + 0x1219cce, std::uint8_t{0x90});
    Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + 0x1219cce), idleState.size(), PAGE_EXECUTE_READ, &pinProtection) != FALSE,
          "poisoned instruction keeps executable page classification");
    eligibility.knownMask = 0x87654321;
    Check(!ReadLocalEligibility(runtime.image, expected, nullptr, eligibility, identity) && eligibility.knownMask == 0x87654321,
          "changed native state instruction refuses observation without modifying output");
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(!runtime.active.load() && runtime.counters.steps.load() == 0 && Pad() == physical,
          "changed native state instruction rejects before any callback write");
    Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + 0x1219cce), idleState.size(), PAGE_READWRITE, &pinProtection) != FALSE,
          "fixture restores native evidence page for remaining tests");
    Put(runtime.image + 0x1219cce, idleState);
    Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + 0x1219cce), idleState.size(), PAGE_EXECUTE_READ, &pinProtection) != FALSE,
          "restored instruction page remains read only and executable");

    Configure(OriginalEligibilityChange, false);
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(!runtime.active.load() && runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::IneligibleContext) &&
              runtime.counters.steps.load() == 1 && runtime.counters.restored.load() == 1 && Pad() == physical,
          "native state transition during original always restores owned pad and disarms");

    Configure(OriginalPhysical, false);
    bool        wrongThreadUpdated = true;
    std::thread wrongThread([&anotherRejected, &wrongThreadUpdated] { wrongThreadUpdated = anotherRejected.Update(runtime.frame); });
    wrongThread.join();
    Check(!wrongThreadUpdated && !runtime.active.load(), "foreign API update rejected and disarmed");
    Check(Pad() == physical && runtime.counters.steps.load() == 0, "foreign API update never writes a native pad");

    Configure(OriginalEndAttempt, false);
    testOwner = &anotherRejected;
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(Pad() == physical && runtime.counters.restored.load() == 1, "finally cleans scope after refused active End");
    Check(anotherRejected.End(), "End succeeds after callback restoration");
    NativeMovementTrialState finalMetrics;
    anotherRejected.ReadState(finalMetrics);
    Check(!finalMetrics.bound && !finalMetrics.active && finalMetrics.scopedSteps == 1 && finalMetrics.restoredSteps == 1,
          "End preserves final counters with inactive flags");

    Configure(OriginalProtectionChange, false);
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(runtime.transaction.Active() && runtime.scopes.load() == 1 && runtime.counters.restoreFailures.load() == 1,
          "real failed write retains snapshot and restoration ownership");
    Check(!anotherRejected.End(), "pending restoration refuses teardown");
    DWORD oldProtection = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + padRva + NativePadCurrentOffset), physical.size(), PAGE_READWRITE, &oldProtection) != FALSE,
          "fixture write protection restored");
    const auto historicalRestoreFailures = runtime.counters.restoreFailures.load();
    Check(anotherRejected.End(), "End explicitly retries restoration after callback returned");
    Check(Pad() == physical && !runtime.transaction.Active() && !runtime.scopes.load(), "explicit End retry restores every physical byte");
    anotherRejected.ReadState(finalMetrics);
    Check(finalMetrics.restoredSteps == 1 && finalMetrics.restoreFailures == historicalRestoreFailures,
          "retry cleanup preserves failure and recovery counters");

    const auto          fixtureImage = runtime.image, fixturePed = runtime.ped, fixtureProvider = runtime.provider;
    NativeMovementInput newOwner;
    Check(!newOwner.Begin(neutral) && newOwner.End(), "new wrong-executable owner remains rejected");
    anotherRejected.ReadState(finalMetrics);
    Check(finalMetrics.restoredSteps == 1 && finalMetrics.restoreFailures == historicalRestoreFailures && !finalMetrics.bound,
          "later owner cannot replace prior final diagnostics");
    runtime.image = fixtureImage;
    runtime.ped = fixturePed;
    runtime.provider = fixtureProvider;

    Configure(OriginalMovement);
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(Pad() == physical, "normal native return restores all96 physical bytes");
    Check(runtime.counters.steps.load() == 1 && runtime.counters.restored.load() == 1, "normal scope has exact restore accounting");
    Check(runtime.counters.queries.load() == 3 && runtime.counters.neutral.load() == 1, "normal native query metrics retained");
    Check(runtime.counters.lastMoveState.load() == 6, "raw native move state captured");
    Check(scope.consumed.ButtonCross == 255 && runtime.frame.previous.ButtonCross == 0, "native consumer owns previous independently of producer");
    Check(runtime.active.load() && runtime.counters.reason.load() == 0, "normal scope remains healthy");
    float value = 0;
    Check(HookQuery(reinterpret_cast<void*>(runtime.provider), 2, &value) == 3 && value == 0.75f, "query outside scope forwards original");

    Configure(OriginalMovement);
    Put(runtime.ped + 0x7c8, std::uintptr_t{0x123456});
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(runtime.active.load() && runtime.counters.reason.load() == 0 && runtime.counters.steps.load() == 1,
          "stale vehicle pointer does not reject an on-foot ped");
    Check(Pad() == physical, "on-foot ped with cached vehicle pointer restores pad");

    Configure(OriginalException);
    RunExceptionBoundary();
    Check(Pad() == physical && !runtime.transaction.Active(), "SEH unwind restores and releases physical snapshot");
    Check(runtime.counters.restored.load() == 1 && !runtime.active.load(), "SEH unwind disarms after restoration");
    Check(runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::OriginalException), "SEH diagnostic preserved");

    Configure(OriginalNested);
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(Pad() == physical, "nested native physical changes survive outer cleanup");
    Check(runtime.counters.nested.load() == 1 && runtime.counters.restored.load() == 1 && originalCalls == 2, "nested scope restored exactly once");
    Check(!runtime.active.load() && runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::NestedCallback), "nested callback disarms trial");

    Configure(OriginalPhysical);
    std::thread foreign([] { HookControl(reinterpret_cast<void*>(runtime.ped)); });
    foreign.join();
    Check(runtime.counters.foreign.load() == 1 && runtime.counters.steps.load() == 0 && originalCalls == 1, "foreign callback forwards without a scope");
    Check(Pad() == physical, "foreign callback leaves physical pad intact");

    Configure(OriginalContextChange);
    HookControl(reinterpret_cast<void*>(runtime.ped));
    std::int32_t index = 0;
    Check(Read(runtime.image + 0x5726120, index) && index == 1, "native provider context change is never blindly overwritten");
    Check(Pad() == physical && !runtime.active.load(), "provider context change restores physical pad and disarms");

    Configure(OriginalQueryContextChange);
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::ContextChanged) && runtime.counters.restored.load() == 1,
          "provider query detects context change within native original");

    Configure(OriginalPhysical);
    runtime.started -= maximumTrialMs;
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(originalCalls == 1 && runtime.counters.steps.load() == 0, "expired trial forwards without native writes");
    Check(runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::Expired), "hard trial deadline enforced");

    Configure(OriginalPhysical);
    runtime.refreshed -= maximumInputAgeMs + 1;
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::StaleInput), "stale MTA input disarms");
    Check(Pad() == physical && runtime.counters.steps.load() == 0, "stale input never reaches native pad");

    Configure(OriginalPhysical);
    Put(runtime.ped + 0x634, std::uint32_t{0x100});
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::Vehicle), "vehicle entry rejected");
    Check(Pad() == physical && runtime.counters.steps.load() == 0, "vehicle entry leaves physical input untouched");

    Configure(OriginalPhysical);
    runtime.engineThread.store(0);
    Put(runtime.ped + 0x7e0, std::int32_t{1});
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(!runtime.engineThread.load() && runtime.counters.steps.load() == 0, "unverified local callback cannot adopt an engine thread");

    // Admission remains the real rejected-executable path. Once it has supplied
    // an owner generation, the fixture restores only its own synthetic identity
    // and invokes production thunks; no shipping guard is bypassed by Begin.
    runtime.active.store(false);
    runtime.bound.store(false);
    NativeMovementInput splitOwner;
    Check(!splitOwner.Begin(neutral), "two-thread fixture still rejects executable admission");
    runtime.image = fixtureImage;
    runtime.ped = fixturePed;
    runtime.provider = fixtureProvider;
    Configure(OriginalSplit, false);
    runtime.engineThread.store(0);
    splitEntered = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    splitResume = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    splitFinished = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    splitNext = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Check(splitEntered && splitResume && splitFinished && splitNext, "two-thread coordination events created");
    std::thread unownedFirst([] { HookControl(unownedPed); });
    unownedFirst.join();
    Check(!runtime.engineThread.load() && runtime.counters.steps.load() == 0 && !runtime.counters.foreign.load(),
          "unowned callback before adoption is not misclassified as a foreign engine thread");
    splitStage.store(0);
    std::atomic<DWORD> adoptedThread{0};
    std::thread        engine(
        [&adoptedThread]
        {
            adoptedThread.store(GetCurrentThreadId());
            for (unsigned step = 0; step < 3; ++step)
            {
                HookControl(reinterpret_cast<void*>(runtime.ped));
                Check(SetEvent(splitFinished) != FALSE, "engine publishes restored step");
                if (step != 2)
                    Check(WaitForSingleObject(splitNext, 5000) == WAIT_OBJECT_0, "engine waits for next producer frame");
            }
        });
    Check(WaitForSingleObject(splitEntered, 5000) == WAIT_OBJECT_0, "producer observes live engine scope");
    Check(runtime.engineThread.load() == adoptedThread.load() && runtime.engineThread.load() != runtime.producerThread,
          "verified callback adopts distinct engine thread");
    MtaInputState lateral;
    lateral.current.LeftStickX = 128;
    lateral.current.ButtonCross = 255;
    Check(splitOwner.Update(lateral) && splitOwner.Update(lateral), "producer publishes repeated input while engine scope is active");
    Check(runtime.counters.reason.load() == 0 && runtime.scopes.load() == 1, "cross-thread publication is healthy and keeps scope ownership");
    Check(SetEvent(splitResume) != FALSE && WaitForSingleObject(splitFinished, 5000) == WAIT_OBJECT_0, "first engine frame restores after publication");
    Check(Pad() == physical && runtime.counters.restored.load() == 1, "first engine frame restores physical bytes");
    std::thread laterForeign([] { HookControl(reinterpret_cast<void*>(runtime.ped)); });
    laterForeign.join();
    Check(runtime.counters.foreign.load() == 1 && runtime.engineThread.load() == adoptedThread.load(), "foreign callback cannot replace adopted engine");
    Check(SetEvent(splitNext) != FALSE && WaitForSingleObject(splitEntered, 5000) == WAIT_OBJECT_0, "second native frame begins");
    Check(splitOwner.Update(neutral), "producer publishes neutral while held native frame runs");
    Check(SetEvent(splitResume) != FALSE && WaitForSingleObject(splitFinished, 5000) == WAIT_OBJECT_0, "second native frame restores");
    Check(SetEvent(splitNext) != FALSE && WaitForSingleObject(splitEntered, 5000) == WAIT_OBJECT_0, "third native frame begins");
    Check(!splitOwner.End(), "producer End returns pending while native scope is active");
    Check(runtime.bound.load() && !runtime.active.load() && runtime.counters.reason.load() == 0, "pending End retains code without a permanent failure");
    Check(SetEvent(splitResume) != FALSE && WaitForSingleObject(splitFinished, 5000) == WAIT_OBJECT_0, "pending End allows native finally restoration");
    engine.join();
    Check(splitOwner.End(), "producer cleanup retry succeeds after engine return");
    splitOwner.ReadState(finalMetrics);
    Check(!finalMetrics.bound && !finalMetrics.active && finalMetrics.scopedSteps == 3 && finalMetrics.restoredSteps == 3 && !finalMetrics.failureReason,
          "two-thread End preserves clean final accounting");

    Configure(OriginalSplit, false);
    runtime.engineThread.store(0);
    splitStage.store(0);
    std::thread collisionEngine([] { HookControl(reinterpret_cast<void*>(runtime.ped)); });
    Check(WaitForSingleObject(splitEntered, 5000) == WAIT_OBJECT_0, "collision fixture holds a genuine replacement scope");
    const auto  beforeCollisionCalls = originalCalls.load();
    std::thread collisionForeign([] { HookControl(unownedPed); });
    collisionForeign.join();
    Check(originalCalls.load() == beforeCollisionCalls && !runtime.active.load() &&
              runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::WrongThread),
          "overlapping foreign control aborts without exposing the replacement to its original");
    std::thread physicalProvider(
        []
        {
            float nativeValue = 0;
            Check(HookQuery(reinterpret_cast<void*>(runtime.provider), 2, &nativeValue) == 3 && nativeValue == 0.75f,
                  "foreign physical-source provider query remains untouched during collision");
        });
    physicalProvider.join();
    Check(SetEvent(splitResume) != FALSE, "adopted engine alone completes collision restoration");
    collisionEngine.join();
    Check(Pad() == physical && !runtime.padAccess.load() && runtime.counters.restored.load() == 1,
          "collision owner restores and releases exclusive pad access");
    const auto  afterCollisionCalls = originalCalls.load();
    std::thread afterCollisionForeign([] { HookControl(unownedPed); });
    afterCollisionForeign.join();
    Check(originalCalls.load() == afterCollisionCalls + 1, "subsequent foreign control forwards normally after restoration");
    Check(splitOwner.End(), "collision abort remains safely removable");

    Configure(OriginalBlockedControl, false);
    std::thread physicalControl([] { HookControl(unownedPed); });
    Check(WaitForSingleObject(splitEntered, 5000) == WAIT_OBJECT_0, "foreign physical reader precedes attempted replacement");
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(!runtime.counters.steps.load() && runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::WrongThread),
          "an existing physical reader prevents any native pad replacement");
    Check(SetEvent(splitResume) != FALSE, "physical reader finishes after rejected replacement");
    physicalControl.join();
    Check(Pad() == physical && !runtime.padAccess.load() && splitOwner.End(), "physical reader collision leaves no pad ownership");

    // Every provider wrapper must retain its trampoline even for an unowned
    // call with no pad scope. Recheck after Detours suspension catches the race
    // between an optimistic End check and a provider entering the hook.
    for (unsigned providerHook = 0; providerHook < 3; ++providerHook)
    {
        Configure(OriginalPhysical, false);
        runtime.originalMembership = OriginalBlockedMembership;
        runtime.originalAvailability = OriginalBlockedMembership;
        runtime.originalQuery = OriginalBlockedQuery;
        std::thread provider(
            [providerHook]
            {
                if (providerHook == 0)
                    HookMembership(reinterpret_cast<void*>(runtime.provider), 2);
                else if (providerHook == 1)
                    HookAvailability(reinterpret_cast<void*>(runtime.provider), 2);
                else
                    HookQuery(reinterpret_cast<void*>(runtime.provider), 2, nullptr);
            });
        Check(WaitForSingleObject(splitEntered, 5000) == WAIT_OBJECT_0, "producer observes provider-only hook");
        Check(runtime.inFlight.load() == 1 && !splitOwner.End() && !runtime.counters.reason.load(), "provider-only original prevents premature teardown");
        bool pending = false;
        Check(!ChangeHooks(false, &pending) && pending, "post-suspension check aborts and resumes in-flight provider");
        Check(SetEvent(splitResume) != FALSE, "provider is allowed to finish");
        provider.join();
        Check(!runtime.inFlight.load() && splitOwner.End(), "provider cleanup retry succeeds after hook returns");
    }

    Configure(OriginalPhysical, false);
    runtime.engineThread.store(0);
    AcquireSRWLockExclusive(&runtime.mailboxLock);
    std::thread contendedEngine([] { HookControl(reinterpret_cast<void*>(runtime.ped)); });
    contendedEngine.join();
    ReleaseSRWLockExclusive(&runtime.mailboxLock);
    Check(runtime.active.load() && !runtime.counters.reason.load() && !runtime.counters.steps.load(), "engine never blocks on a producer-owned mailbox lock");
    Check(Pad() == physical && splitOwner.End(), "mailbox contention leaves physical input and cleanup intact");

    Configure(OriginalProtectionChange, false);
    runtime.engineThread.store(0);
    std::thread failedRestoreEngine([] { HookControl(reinterpret_cast<void*>(runtime.ped)); });
    failedRestoreEngine.join();
    Check(runtime.transaction.Active() && runtime.scopes.load() == 1 && !runtime.inFlight.load(),
          "engine-thread failed restoration survives thread return in process state");
    Check(!splitOwner.End(), "producer retains failed restoration while memory is read-only");
    Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + padRva + NativePadCurrentOffset), physical.size(), PAGE_READWRITE, &oldProtection) != FALSE,
          "cross-thread restoration fixture becomes writable again");
    const auto crossThreadFailures = runtime.counters.restoreFailures.load();
    Check(splitOwner.End() && Pad() == physical && !runtime.transaction.Active(), "producer restores retained engine snapshot after quiescence");
    splitOwner.ReadState(finalMetrics);
    Check(finalMetrics.restoreFailures == crossThreadFailures && finalMetrics.restoredSteps == 1,
          "cross-thread restore retry retains historical failure accounting");

    Configure(OriginalStress, false);
    runtime.engineThread.store(0);
    std::atomic<bool> stopStress{false};
    std::thread       stressEngine(
        [&stopStress]
        {
            while (!stopStress.load())
                HookControl(reinterpret_cast<void*>(runtime.ped));
        });
    bool                     coherent = true, monotone = true;
    NativeMovementTrialState previousMetrics;
    unsigned                 samples = 0;
    const auto               sampleDeadline = GetTickCount64() + 3000;
    do
    {
        splitOwner.Update(lateral);
        NativeMovementTrialState sample;
        splitOwner.ReadState(sample);
        coherent = coherent && sample.restoredSteps <= sample.scopedSteps && sample.scopedSteps <= sample.callbackCount &&
                   sample.neutralQueries <= sample.routedQueries && sample.foreignThreadCallbacks <= sample.callbackCount;
        monotone = monotone && sample.restoredSteps >= previousMetrics.restoredSteps && sample.scopedSteps >= previousMetrics.scopedSteps &&
                   sample.callbackCount >= previousMetrics.callbackCount && sample.routedQueries >= previousMetrics.routedQueries;
        previousMetrics = sample;
        ++samples;
        SwitchToThread();
    } while ((samples < 1000 || previousMetrics.scopedSteps < 100) && GetTickCount64() < sampleDeadline);
    stopStress.store(true);
    stressEngine.join();
    Check(samples >= 1000 && runtime.counters.steps.load() >= 100, "metrics sampled during many real engine-thread steps");
    Check(coherent && monotone, "cross-thread metric samples preserve causal relations and monotonicity");
    Check(Pad() == physical && runtime.counters.steps.load() == runtime.counters.restored.load(), "stress loop restores every owned scope");
    Check(splitOwner.End(), "stress owner cleans up after engine thread exits");

    Configure(DetourControlTarget, false);
    runtime.originalMembership = DetourMembershipTarget;
    runtime.originalAvailability = DetourAvailabilityTarget;
    runtime.originalQuery = DetourQueryTarget;
    Check(ChangeHooks(true), "genuine Detours installs all four own-process target hooks");
    runtime.hooked.store(true);
    Check(runtime.originalControl != DetourControlTarget && runtime.originalMembership != DetourMembershipTarget &&
              runtime.originalAvailability != DetourAvailabilityTarget && runtime.originalQuery != DetourQueryTarget,
          "genuine Detours replaces all four original pointers with trampolines");
    ControlFunction volatile controlEntry = DetourControlTarget;
    controlEntry(reinterpret_cast<void*>(runtime.ped));
    Check(Pad() == physical && runtime.counters.steps.load() == 1 && runtime.counters.restored.load() == 1,
          "genuine hooked entry performs and restores one native scope");
    std::thread hookedProvider(
        []
        {
            MembershipFunction volatile entry = DetourMembershipTarget;
            entry(reinterpret_cast<void*>(runtime.provider), 0x55);
        });
    Check(WaitForSingleObject(splitEntered, 5000) == WAIT_OBJECT_0, "genuine hooked provider remains in flight");
    Check(!splitOwner.End(), "genuine hook teardown retains in-flight trampoline");
    bool pendingDetach = false;
    Check(!ChangeHooks(false, &pendingDetach) && pendingDetach, "genuine detach aborts and resumes in-flight provider");
    Check(SetEvent(splitResume) != FALSE, "genuine provider original can finish after abort");
    hookedProvider.join();
    Check(splitOwner.End(), "genuine Detours detaches all four hooks after callback exit");
    Check(runtime.originalControl == DetourControlTarget && runtime.originalMembership == DetourMembershipTarget &&
              runtime.originalAvailability == DetourAvailabilityTarget && runtime.originalQuery == DetourQueryTarget,
          "genuine detach restores every original function pointer");
    const auto callbacksAfterDetach = runtime.counters.callbacks.load();
    controlEntry(reinterpret_cast<void*>(runtime.ped));
    MembershipFunction volatile membershipEntry = DetourMembershipTarget;
    MembershipFunction volatile availabilityEntry = DetourAvailabilityTarget;
    QueryFunction volatile queryEntry = DetourQueryTarget;
    value = 0;
    Check(!membershipEntry(reinterpret_cast<void*>(runtime.provider), 2) && !availabilityEntry(reinterpret_cast<void*>(runtime.provider), 2) &&
              queryEntry(reinterpret_cast<void*>(runtime.provider), 2, &value) == 3 && value == 0.75f,
          "detached provider entries return their original physical results");
    Check(runtime.counters.callbacks.load() == callbacksAfterDetach && !runtime.inFlight.load(), "detached entries no longer enter production hooks");

    // A producer does not own the engine's temporary provider selection. Keep
    // one actual engine thread alive across these callbacks so a foreign global
    // context cannot be mistaken for a foreign callback-thread passthrough.
    NativeMovementInput eligibilityOwner;
    Check(!eligibilityOwner.Begin(neutral), "eligibility race fixture still rejects executable admission");
    runtime.image = fixtureImage;
    runtime.ped = fixturePed;
    runtime.provider = fixtureProvider;
    Configure(OriginalEligibilityPublication, false);
    runtime.engineThread.store(0);
    HANDLE eligibilityRequest = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE eligibilityDone = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Check(eligibilityRequest && eligibilityDone, "eligibility producer/engine events created");
    std::thread eligibilityEngine(
        [&]
        {
            for (unsigned step = 0; step < 3; ++step)
            {
                Check(WaitForSingleObject(eligibilityRequest, 5000) == WAIT_OBJECT_0, "eligibility engine receives callback request");
                HookControl(reinterpret_cast<void*>(runtime.ped));
                Check(SetEvent(eligibilityDone) != FALSE, "eligibility engine publishes callback completion");
            }
        });
    Check(SetEvent(eligibilityRequest) != FALSE && WaitForSingleObject(eligibilityDone, 5000) == WAIT_OBJECT_0,
          "verified engine callback publishes and restores eligibility scope");
    Check(runtime.counters.steps.load() == 1 && runtime.counters.restored.load() == 1 && runtime.active.load(),
          "first eligibility publication follows a healthy scoped callback");
    NativeMovementEligibility publishedEligibility;
    Check(ReadFixtureProducerEligibility(eligibilityOwner, publishedEligibility) && publishedEligibility.eligible,
          "old observer and new publication both accept the same owned baseline");
    Put(runtime.image + 0x5726120, std::int32_t{1});
    Put(runtime.image + 0x5067940, runtime.image + padRva + NativePadStride);
    NativeMovementEligibility directGuard;
    directGuard.knownMask = 0xabcdef12;
    BindingIdentity directIdentity;
    Check(!ReadLocalEligibility(runtime.image, {runtime.ped, runtime.provider}, nullptr, directGuard, directIdentity) && directGuard.knownMask == 0xabcdef12,
          "unchanged direct engine guard refuses the isolated foreign context");
    Check(ReadFixtureProducerEligibility(eligibilityOwner, publishedEligibility) && publishedEligibility.eligible &&
              publishedEligibility.knownMask == NativeMovementEligibilityMask && publishedEligibility.flags == NativeMovementEligibilityMask,
          "active producer reads engine publication during foreign transient provider context");
    Check(runtime.active.load() && !runtime.counters.reason.load() && runtime.counters.steps.load() == 1 && Pad() == physical,
          "producer observation neither disarms nor applies a pad in foreign context");
    Check(SetEvent(eligibilityRequest) != FALSE && WaitForSingleObject(eligibilityDone, 5000) == WAIT_OBJECT_0,
          "same adopted engine reaches callback during foreign context");
    Check(!runtime.active.load() && runtime.counters.reason.load() == static_cast<std::uint32_t>(Failure::BindingChanged) &&
              runtime.counters.steps.load() == 1 && runtime.counters.restored.load() == 1 && Pad() == physical,
          "native callback still refuses foreign context before any replacement");
    Put(runtime.image + 0x5726120, std::int32_t{0});
    Put(runtime.image + 0x5067940, runtime.image + padRva);
    Check(SetEvent(eligibilityRequest) != FALSE && WaitForSingleObject(eligibilityDone, 5000) == WAIT_OBJECT_0,
          "engine receives restored context after disarm");
    eligibilityEngine.join();
    Check(!runtime.active.load() && runtime.counters.steps.load() == 1 && Pad() == physical, "restoring transient globals never resumes a disarmed writer");
    publishedEligibility.knownMask = 0x13572468;
    Check(!eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.knownMask == 0x13572468,
          "disarmed eligibility publication cannot replace caller output");
    Check(eligibilityOwner.End(), "eligibility race owner ends after callback rejection");
    Check(CloseHandle(eligibilityRequest) != FALSE && CloseHandle(eligibilityDone) != FALSE, "eligibility coordination handles closed");

    // Native control updates unrelated flags in this same word. Repeated OS
    // memory reads must compare the InVehicle predicate, not require unrelated
    // engine bits to stay frozen throughout a producer observation.
    std::atomic<bool>     stopFlagUpdates{false};
    std::atomic<unsigned> flagUpdates{0};
    std::thread           flagEngine(
        [&]
        {
            while (!stopFlagUpdates.load())
            {
                InterlockedXor(reinterpret_cast<volatile LONG*>(runtime.ped + 0x634), 0x10000000);
                flagUpdates.fetch_add(1);
            }
        });
    bool relevantFlagStable = true;
    for (unsigned attempt = 0; attempt < 1000; ++attempt)
    {
        NativeMovementEligibility sampled;
        BindingIdentity           sampledIdentity;
        relevantFlagStable = ReadLocalEligibility(runtime.image, {runtime.ped, runtime.provider}, nullptr, sampled, sampledIdentity) && sampled.eligible &&
                             !sampled.context.inVehicle && relevantFlagStable;
    }
    stopFlagUpdates.store(true);
    flagEngine.join();
    Put(runtime.ped + 0x634, std::uint32_t{0});
    Check(flagUpdates.load() > 0, "unrelated native flag updates overlap producer observations");
    Check(relevantFlagStable, "only InVehicle bit stability is required during unrelated native flag changes");

    Check(!eligibilityOwner.Begin(neutral), "fresh cache fixture keeps production executable rejection");
    runtime.image = fixtureImage;
    runtime.ped = fixturePed;
    runtime.provider = fixtureProvider;
    Configure(OriginalEligibilityPublication, false);
    publishedEligibility.knownMask = 0x24681357;
    Check(!eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.knownMask == 0x24681357,
          "new generation cannot reuse an earlier callback publication");
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.eligible, "fresh callback refreshes new generation eligibility");
    const auto publishedPed = runtime.ped;
    runtime.ped += 8;
    publishedEligibility.knownMask = 0x11223344;
    Check(!eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.knownMask == 0x11223344,
          "publication identity tag rejects a different owned ped without output mutation");
    runtime.ped = publishedPed;
    Check(eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.eligible,
          "matching immutable identity can consume its fresh publication");
    // Expiry is a real producer observation after wall-clock time, rather than
    // mutating private timestamps to make the intended condition tautological.
    Sleep(300);
    publishedEligibility.knownMask = 0x55667788;
    Check(!eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.knownMask == 0x55667788,
          "unrefreshed eligibility publication expires beyond 250 milliseconds");
    Check(runtime.active.load() && !runtime.counters.reason.load(), "expired observation does not invent a native callback failure");
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.eligible,
          "verified callback refreshes an expired producer observation");
#if defined(MTA_NATIVE_ELIGIBILITY_PUBLISHED_CACHE)
    // Publishing must remain nonblocking even when a producer holds a read
    // snapshot. The existing fresh value remains valid until its own expiry.
    const auto beforeContendedPublication = runtime.counters.steps.load();
    runtime.engineThread.store(0);
    AcquireSRWLockShared(&runtime.eligibilityLock);
    std::thread publicationContender([] { HookControl(reinterpret_cast<void*>(runtime.ped)); });
    publicationContender.join();
    ReleaseSRWLockShared(&runtime.eligibilityLock);
    Check(runtime.active.load() && !runtime.counters.reason.load() && runtime.counters.steps.load() == beforeContendedPublication + 1 &&
              runtime.counters.restored.load() == runtime.counters.steps.load() && Pad() == physical,
          "actual adopted callback completes while eligibility publication lock is held");
#endif
    runtime.active.store(false);
    publishedEligibility.knownMask = 0x99aabbcc;
    Check(!eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.knownMask == 0x99aabbcc,
          "inactive writer cannot publish cached eligible state");
    Check(eligibilityOwner.End(), "fresh eligibility cache owner ends cleanly");
    publishedEligibility.knownMask = 0xaabbccdd;
    Check(!eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.knownMask == 0xaabbccdd,
          "ended writer clears its publication and preserves output");

    NativeMovementInput nextEligibilityOwner;
    Check(!nextEligibilityOwner.Begin(neutral), "replacement owner preserves wrong-executable guard");
    runtime.image = fixtureImage;
    runtime.ped = fixturePed;
    runtime.provider = fixtureProvider;
    Configure(OriginalEligibilityPublication, false);
    HookControl(reinterpret_cast<void*>(runtime.ped));
    Check(nextEligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.eligible,
          "replacement owner receives only its own callback generation");
    publishedEligibility.knownMask = 0xdeadbeef;
    Check(!eligibilityOwner.ReadEligibility(publishedEligibility) && publishedEligibility.knownMask == 0xdeadbeef,
          "retired adapter owner cannot consume the next generation publication");
    Check(nextEligibilityOwner.End(), "replacement eligibility owner ends cleanly");
    for (HANDLE event : {splitEntered, splitResume, splitFinished, splitNext})
        Check(CloseHandle(event) != FALSE, "coordination event closed");

    runtime.active.store(false);
    runtime.bound.store(false);
    Check(VirtualFree(reinterpret_cast<void*>(runtime.provider), 0, MEM_RELEASE) != FALSE, "provider fixture freed");
    Check(VirtualFree(reinterpret_cast<void*>(runtime.ped), 0, MEM_RELEASE) != FALSE, "ped fixture freed");
    Check(VirtualFree(reinterpret_cast<void*>(runtime.image), 0, MEM_RELEASE) != FALSE, "image fixture freed");
    runtime.provider = runtime.ped = runtime.image = 0;
    std::printf("Passed %u production native movement callback checks\n", checks.load());
}
