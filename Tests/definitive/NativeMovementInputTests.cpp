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
    unsigned                          checks = 0;
    NativeInputRouting::PadStateBytes physical;
    unsigned                          originalCalls = 0;
    NativeMovementInput*              testOwner = nullptr;
    void*                             unownedPed = reinterpret_cast<void*>(0x12345);

    void Check(bool condition, const char* label)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed: %s\n", label);
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

    void OriginalQueryContextChange(void*)
    {
        ++originalCalls;
        Put(runtime.image + 0x5726120, std::int32_t{1});
        float value = 0;
        Check(HookQuery(reinterpret_cast<void*>(runtime.provider), 2, &value) == 3 && value == 0.75f,
              "changed provider context forwards physical query after abort");
        Check(Pad() == physical && !scope.transaction.Active(), "provider query restores before forwarding changed context");
    }

    void OriginalEndAttempt(void*)
    {
        ++originalCalls;
        Check(testOwner && !testOwner->End(), "End refuses teardown inside native callback");
        Check(scope.transaction.Active() && runtime.scopes.load() == 1, "refused End retains restoration ownership");
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
        Check(!scope.transaction.Active(), "previous scope not stranded");
        Check(!runtime.scopes.load(), "no active callback scope");
        ClearCounters();
        runtime.thread = GetCurrentThreadId();
        if (advanceGeneration)
            runtime.generation.fetch_add(1);
        runtime.bound.store(true);
        runtime.active.store(true);
        runtime.frame = {};
        runtime.frame.current.LeftStickY = -128;
        runtime.frame.current.ButtonCross = 255;
        runtime.started = runtime.refreshed = GetTickCount64();
        runtime.originalControl = original;
        runtime.originalMembership = OriginalMembership;
        runtime.originalAvailability = OriginalMembership;
        runtime.originalQuery = OriginalQuery;
        originalCalls = 0;
        Put(runtime.image + 0x5726120, std::int32_t{0});
        Put(runtime.ped + 0x634, std::uint32_t{0});
        Put(runtime.ped + 0x7c8, std::uintptr_t{0});
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
    Check(rejection.failureReason == static_cast<std::uint32_t>(Failure::UnsupportedBuild), "wrong executable has build rejection reason");
    Check(!runtime.hooked.load() && !runtime.scopes.load(), "wrong executable installs no hooks or scopes");
    Check(rejected.End() && rejected.End(), "unbound End is idempotent");
    NativeMovementInput anotherRejected;
    Check(!anotherRejected.Begin(neutral), "second ordinary executable admission rejected");
    rejected.ReadState(rejection);
    Check(rejection.failureReason == static_cast<std::uint32_t>(Failure::UnsupportedBuild), "earlier owner preserves final diagnostic");
    Check(anotherRejected.End(), "second rejection cleans up");

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
    for (std::size_t i = 0; i < physical.size(); ++i)
        physical[i] = static_cast<std::byte>(i + 1);

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
    Check(scope.transaction.Active() && runtime.scopes.load() == 1 && runtime.counters.restoreFailures.load() == 1,
          "real failed write retains snapshot and restoration ownership");
    Check(!anotherRejected.End(), "pending restoration refuses teardown");
    DWORD oldProtection = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(runtime.image + padRva + NativePadCurrentOffset), physical.size(), PAGE_READWRITE, &oldProtection) != FALSE,
          "fixture write protection restored");
    const auto historicalRestoreFailures = runtime.counters.restoreFailures.load();
    Check(anotherRejected.End(), "End explicitly retries restoration after callback returned");
    Check(Pad() == physical && !scope.transaction.Active() && !runtime.scopes.load(), "explicit End retry restores every physical byte");
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
    Check(runtime.frame.previous.ButtonCross == 255, "native step consumes sprint edge once");
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
    Check(Pad() == physical && !scope.transaction.Active(), "SEH unwind restores and releases physical snapshot");
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

    runtime.active.store(false);
    runtime.bound.store(false);
    Check(VirtualFree(reinterpret_cast<void*>(runtime.provider), 0, MEM_RELEASE) != FALSE, "provider fixture freed");
    Check(VirtualFree(reinterpret_cast<void*>(runtime.ped), 0, MEM_RELEASE) != FALSE, "ped fixture freed");
    Check(VirtualFree(reinterpret_cast<void*>(runtime.image), 0, MEM_RELEASE) != FALSE, "image fixture freed");
    runtime.provider = runtime.ped = runtime.image = 0;
    std::printf("Passed %u production native movement callback checks\n", checks);
}
