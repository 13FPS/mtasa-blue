/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned fixtures for API5 query, lifetime, and patch admission faults
 *****************************************************************************/
// Private inclusion exposes the real helpers without creating shipping bypasses.
#include "../../Client/game_de/NativeFrameBoundaryProbe.cpp"
#include "../../Client/sdk/game/CGameRenderSetupDiagnostic.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>
bool  OwnedNativeDispatchCleanup(bool& pending);
void* OwnedNativeDispatchWrapperHook();
bool  OwnedNativeDispatchTransaction(bool install, std::uintptr_t wrapper, bool& pending);
void  OwnedNativeDispatchSetupSnapshot(mta::game_de::RenderSetupDiagnosticV1& state);
bool  OwnedNativeDispatchSetupRefusal(std::uintptr_t wrapper, bool afterHolds, mta::game_de::RenderSetupDiagnosticV1& state);
bool  OwnedNativeDispatchSafetyHeld();
bool  OwnedNativeDispatchDiscovery(void (*afterSnapshot)(), DWORD (*newThreadId)());
bool  OwnedNativeDispatchExitingInstall(std::uintptr_t wrapper, unsigned mode, mta::game_de::RenderSetupDiagnosticV1& state);
bool  OwnedNativeDispatchVmTransactions();

namespace
{
    void Require(bool condition, const char* why, unsigned& checks)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Boundary fixture failed: %s\n", why);
            std::exit(1);
        }
    }
    using namespace mta::game_de;
    class OwnedQueries final : public FrameBoundaryQueries
    {
    public:
        unsigned     stop = 0, calls = 0, closes = 0;
        HRESULT      result = E_NOINTERFACE;
        bool         raise = false, cleanup = false, raiseClose = false;
        std::int32_t Execute(FrameBoundaryQueryStage stage, FrameBoundaryObservationState& state) override
        {
            ++calls;
            if (static_cast<unsigned>(stage) == stop)
            {
                if (raise)
                    RaiseException(0xe0444545, 0, 0, nullptr);
                return result;
            }
            state.factsMask = 255;
            state.nativeTrackedMode = 1;
            return S_OK;
        }
        bool Close() noexcept override
        {
            ++closes;
            if (raiseClose)
                RaiseException(0xe0444546, 0, 0, nullptr);
            return !cleanup;
        }
    };
    void Noop(void*)
    {
    }
    void ResizeNoop(void*, UINT, UINT, bool)
    {
    }
    unsigned                  dispatchForwards = 0;
    HRESULT                   dispatchResult = S_OK;
    HRESULT STDMETHODCALLTYPE OwnedDispatchTarget(IDXGISwapChain*, UINT interval, UINT flags)
    {
        ++dispatchForwards;
        if (flags == 3)
            RaiseException(0xe0444547, 0, 0, nullptr);
        return interval == 7 ? dispatchResult : E_INVALIDARG;
    }
    bool CatchDispatchException()
    {
        __try
        {
            boundary_detail::HookNativeDispatch(nullptr, 7, 3, &OwnedDispatchTarget);
        }
        __except (GetExceptionCode() == 0xe0444547 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }
    HANDLE             discoveryTrigger = nullptr, discoveryReady = nullptr;
    std::atomic<DWORD> discoveryChildId{0};
    void               TriggerLateChild()
    {
        SetEvent(discoveryTrigger);
        WaitForSingleObject(discoveryReady, INFINITE);
        Sleep(5);
    }
    DWORD LateChildId()
    {
        return discoveryChildId.load();
    }
    bool ChangeOwnedNotify(bool install)
    {
        using namespace mta::game_de::boundary_detail;
        if (DetourTransactionBegin() != NO_ERROR)
            return false;
        if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR ||
            (install ? DetourAttach : DetourDetach)(reinterpret_cast<PVOID*>(&runtime.notify), reinterpret_cast<PVOID>(&HookNotify)) != NO_ERROR)
        {
            DetourTransactionAbort();
            return false;
        }
        return DetourTransactionCommit() == NO_ERROR;
    }
}

bool BoundaryFixtureOriginal(void*, std::uint32_t interval)
{
    using namespace mta::game_de;
    auto* selected = boundary_detail::scope;
    if (selected)
    {
        selected->entered = true;
        selected->state = {};
        selected->state.status = static_cast<unsigned>(FrameBoundaryObservationStatus::ObservedCoherent);
        selected->state.endpointProfileId = 1;
        selected->state.factsMask = 255;
        selected->state.nativeTrackedMode = 1;
        selected->state.queryStage = 9;
        selected->state.presentEntries = 1;
        selected->state.presentThreadId = GetCurrentThreadId();
        if (interval == 2 || interval == 3)
        {
            selected->cleanupFailed = true;
            selected->state.status = static_cast<unsigned>(FrameBoundaryObservationStatus::QueryFailed);
            selected->state.queryHresult = static_cast<unsigned>(E_UNEXPECTED);
            selected->state.rejectedEntries = 1;
            if (selected->cleanupFailure)
                selected->cleanupFailure();
            if (interval == 2)
                RaiseException(0xe0424242, 0, 0, nullptr);
        }
    }
    return interval != 0;
}

void PrimeBoundaryDiagnostic(std::uint64_t generation)
{
    using namespace mta::game_de;
    auto& runtime = boundary_detail::runtime;
    runtime.preparation = {};
    runtime.preparation.version = RenderEndpointDiagnosticVersionV1;
    runtime.preparation.generationLow = static_cast<std::uint32_t>(generation);
    runtime.preparation.generationHigh = static_cast<std::uint32_t>(generation >> 32);
    runtime.preparation.preparationGate = static_cast<unsigned>(RenderEndpointPrepareGate::Complete);
    runtime.preparation.preparationReason = static_cast<unsigned>(RenderEndpointPrepareReason::Admitted);
    runtime.preparation.loadedEntryMatch = static_cast<unsigned>(RenderEndpointLoadedMatch::Match);
    AcquireSRWLockExclusive(&runtime.diagnosticLock);
    runtime.diagnostic = runtime.preparation;
    ReleaseSRWLockExclusive(&runtime.diagnosticLock);
}
void PrimeNativeDispatchDiagnostic(std::uint64_t generation)
{
    PrimeBoundaryDiagnostic(generation);
    mta::game_de::boundary_detail::runtime.nativeDispatch = true;
}
bool NativeDispatchFixtureOriginal(void*, std::uint32_t interval)
{
    using namespace mta::game_de;
    auto* selected = boundary_detail::scope;
    if (selected)
    {
        selected->nativeDispatch = {};
        selected->nativeDispatch.endpointProfileId = 2;
        selected->nativeDispatch.status = static_cast<unsigned>(FrameBoundaryObservationStatus::NoScopedPresent);
        if (interval != 4)
        {
            selected->dispatchEntered = true;
            selected->nativeDispatch.status = static_cast<unsigned>(FrameBoundaryObservationStatus::ObservedCoherent);
            selected->nativeDispatch.factsMask = 255;
            selected->nativeDispatch.nativeTrackedMode = 1;
            selected->nativeDispatch.queryStage = 9;
            selected->nativeDispatch.presentEntries = 1;
            selected->nativeDispatch.presentThreadId = GetCurrentThreadId();
        }
    }
    return interval != 0;
}

void CheckFrameBoundaryProbe(const char* customFixture, const char* wrapperFixture, unsigned& checks)
{
    using namespace mta::game_de;
    namespace detail = mta::game_de::boundary_detail;
    using Status = FrameBoundaryObservationStatus;
    Require(OwnedNativeDispatchVmTransactions(), "actual VM-backed vendor commit/abort preserve borrowed counts and bypass a stopped CRT heap owner", checks);
    detail::Roots dispatchRoots;
    dispatchRoots.swapchain = 0x1000;
    dispatchRoots.device = dispatchRoots.contextDevice = 0x2000;
    dispatchRoots.manager = dispatchRoots.contextManager = dispatchRoots.commandManager = 0x3000;
    dispatchRoots.gpuCount = dispatchRoots.afrCount = dispatchRoots.nodeMask = 1;
    dispatchRoots.index = 2;
    FrameBoundaryScope strictUnadmitted;
    strictUnadmitted.knownPresenter = true;
    strictUnadmitted.nativeSwapchain = dispatchRoots.swapchain;
    CustomPresentClassificationState knownCustom;
    knownCustom.status = static_cast<unsigned>(CustomPresentClassificationStatus::KnownNativePresenter);
    FrameBoundaryObservationState dispatchAssociation;
    dispatchAssociation.endpointProfileId = 2;
    Require(detail::ValidateNativeRoots(dispatchRoots, knownCustom, strictUnadmitted, dispatchRoots.swapchain, dispatchAssociation) == S_OK &&
                dispatchAssociation.factsMask == 8 && dispatchAssociation.nativeBufferIndex == 2 && !strictUnadmitted.swapchain,
            "real native-root validator admits the explicit API6 chain when the strict API5 chain is unadmitted", checks);
    Require(detail::ValidateNativeRoots(dispatchRoots, knownCustom, strictUnadmitted, dispatchRoots.swapchain + 8, dispatchAssociation) == S_FALSE &&
                detail::ValidateNativeRoots(dispatchRoots, knownCustom, strictUnadmitted, 0, dispatchAssociation) == S_FALSE,
            "different API6 chain and absent strict API5 chain both refuse native-root association", checks);
    for (unsigned stage = 1; stage <= 8; ++stage)
    {
        for (unsigned fault = 0; fault < 3; ++fault)
        {
            OwnedQueries query;
            query.stop = stage;
            query.raise = fault == 2;
            query.result = fault == 1 ? S_FALSE : E_NOINTERFACE;
            FrameBoundaryObservationState state;
            Require(RunFrameBoundaryQueries(query, state), "query fault cleans local references", checks);
            Require(query.calls == stage && query.closes == 1 && state.queryStage == stage, "fault stops exactly at its stage and closes once", checks);
            Require(state.status == static_cast<unsigned>(fault == 1 ? Status::UnsupportedAssociation : Status::QueryFailed),
                    "query failure never claims coherent associations", checks);
        }
    }
    for (unsigned fault = 0; fault < 3; ++fault)
    {
        OwnedQueries query;
        query.cleanup = fault == 1;
        query.raiseClose = fault == 2;
        FrameBoundaryObservationState state;
        Require(RunFrameBoundaryQueries(query, state) == (fault == 0), "cleanup failure is fatal independently of successful metadata", checks);
        Require(query.calls == 8 && query.closes == 1 && state.queryStage == 9, "complete queries close exactly once", checks);
        Require(state.status == static_cast<unsigned>(fault == 0 ? Status::ObservedCoherent : Status::QueryFailed), "cleanup outranks coherent result", checks);
    }
    ResetFrameBoundaryObservation();
    std::array<unsigned char, 0x120> viewport{}, other{};
    std::array<unsigned char, 0x900> owner{};
    std::array<unsigned char, 0x40>  device{};
    auto       put = [](std::uintptr_t address, std::uintptr_t value) { std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value)); };
    const auto view = reinterpret_cast<std::uintptr_t>(viewport.data()), own = reinterpret_cast<std::uintptr_t>(owner.data());
    put(view + 0x18, own);
    put(own + 0x8a8, reinterpret_cast<std::uintptr_t>(device.data()));
    put(reinterpret_cast<std::uintptr_t>(other.data()) + 0x18, own);
    Require(detail::Identity(view, true) && detail::Identity(view, false), "same native identity correlates without retained raw pointer", checks);
    Require(!detail::Identity(reinterpret_cast<std::uintptr_t>(other.data()), false), "another viewport does not inherit correlation", checks);
    detail::runtime.resize = &ResizeNoop;
    detail::runtime.destroy = &Noop;
    detail::runtime.notify = &Noop;
    detail::runtime.armed.store(true);
    detail::HookResize(viewport.data(), 640, 480, false);
    Require(detail::runtime.resizeEntries == 1 && detail::runtime.epoch == 2 && detail::runtime.resizing == 0,
            "same-viewport resize has paired epoch and callback counter", checks);
    detail::HookResize(other.data(), 640, 480, false);
    Require(detail::runtime.resizeEntries == 1 && detail::runtime.foreignEntries == 1 && detail::runtime.epoch == 4,
            "unmatched resize conservatively advances epoch", checks);
    std::thread foreign([&] { detail::HookNotify(device.data()); });
    foreign.join();
    Require(detail::runtime.notifyEntries == 0 && detail::runtime.foreignEntries == 2 && FrameBoundaryInFlight() == 0,
            "TLS-less foreign notification never claims pre-release", checks);
    FrameBoundaryScope captured;
    captured.entered = true;
    captured.epoch = detail::runtime.epoch.load();
    captured.state.status = static_cast<unsigned>(Status::ObservedCoherent);
    captured.state.presentEntries = 1;
    captured.state.factsMask = 255;
    captured.state.queryStage = 9;
    detail::runtime.epoch.fetch_add(1);
    LeaveFrameBoundaryScope(captured);
    Require(captured.state.status == static_cast<unsigned>(Status::ResizeOverlap) && captured.state.rejectedEntries == 1,
            "post-query lifecycle race downgrades completed callback", checks);
    FrameBoundaryScope partial;
    partial.entered = true;
    partial.epoch = detail::runtime.epoch.load();
    partial.state.status = static_cast<unsigned>(Status::UnsupportedAssociation);
    partial.state.queryStage = 4;
    partial.state.queryHresult = S_FALSE;
    partial.state.factsMask = 25;
    partial.state.presentEntries = partial.state.rejectedEntries = 1;
    detail::runtime.epoch.fetch_add(1);
    detail::runtime.resizing.fetch_add(1);
    LeaveFrameBoundaryScope(partial);
    Require(partial.state.status == static_cast<unsigned>(Status::UnsupportedAssociation) && partial.state.queryStage == 4 &&
                partial.state.queryHresult == S_FALSE && partial.state.factsMask == 25 && partial.state.rejectedEntries == 1 &&
                partial.state.lifecycleFlags == 1 && partial.state.resizeEpoch == detail::runtime.epoch.load(),
            "late lifecycle conflict preserves partial negative query and records lifecycle evidence", checks);
    detail::runtime.resizing.fetch_sub(1);
    detail::HookDestroy(viewport.data());
    Require(detail::runtime.destroyed && detail::runtime.inFlight == 0, "same-owner destruction invalidates later association queries", checks);
    ResetFrameBoundaryObservation();
    Require(!detail::Identity(view, false) && detail::runtime.epoch == 0 && detail::runtime.resizeEntries == 0 && !detail::runtime.destroyed,
            "new generation clears digest correlation and lifecycle counters", checks);
    Require(detail::Identity(view, true), "new generation independently adopts its viewport", checks);
    detail::runtime.armed.store(true);
    detail::HookDestroy(other.data());
    Require(!detail::runtime.destroyed && detail::runtime.foreignEntries == 1,
            "proven different-address destruction advances foreign evidence without poisoning selected lifetime", checks);
    AcquireSRWLockExclusive(&detail::runtime.identityLock);
    detail::HookDestroy(viewport.data());
    ReleaseSRWLockExclusive(&detail::runtime.identityLock);
    Require(detail::runtime.destroyed && detail::runtime.foreignEntries == 2 && FrameBoundaryInFlight() == 0,
            "contended destruction identity poisons lifetime rather than missing selected destruction", checks);
    ResetFrameBoundaryObservation();
    Require(detail::Identity(view, true), "new generation clears unknown destruction poison", checks);
    detail::runtime.armed.store(true);
    put(view + 0x18, 1);
    detail::HookDestroy(viewport.data());
    put(view + 0x18, own);
    Require(detail::runtime.destroyed && detail::Identity(view, false),
            "unreadable destruction roots remain sticky even if the old same-address tuple becomes readable again", checks);
    ResetFrameBoundaryObservation();
    Require(detail::Identity(view, true), "same-address reuse fixture independently adopts identity", checks);
    std::array<unsigned char, 0x900> replacementOwner{};
    put(reinterpret_cast<std::uintptr_t>(replacementOwner.data()) + 0x8a8, reinterpret_cast<std::uintptr_t>(device.data()));
    put(view + 0x18, reinterpret_cast<std::uintptr_t>(replacementOwner.data()));
    detail::runtime.armed.store(true);
    detail::HookDestroy(viewport.data());
    Require(detail::runtime.destroyed && detail::runtime.foreignEntries == 1,
            "changed owner tuple at selected address is unknown lifetime rather than proven foreign destruction", checks);
    put(view + 0x18, own);
    ResetFrameBoundaryObservation();

    // This copies only the pinned 44-byte function into disposable owned memory;
    // no copied game instruction or native callback is ever executed.
    std::array<unsigned char, 44> code{};
    std::ifstream                 input(customFixture, std::ios::binary);
    input.seekg(66);
    input.read(reinterpret_cast<char*>(code.data()), code.size());
    Require(input.gcount() == static_cast<std::streamsize>(code.size()), "read exact private notify fingerprint", checks);
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    Require(memory != nullptr, "owned executable patch fixture allocation", checks);
    std::memcpy(memory, code.data(), code.size());
    detail::runtime.image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    detail::CacheDiagnosticModules();
    std::uint32_t branch = 0, branchOwner = 0, branchRva = 0;
    detail::DescribeOwner(reinterpret_cast<std::uintptr_t>(&Noop), branchOwner, branchRva);
    Require(branchOwner == static_cast<unsigned>(RenderEndpointOwnerClass::GameImage) && branchRva != 0,
            "worker-cached owner identifies own executable without callback loader APIs", checks);
    memory[128] = 0xc3;
    const std::int32_t directOffset = 123;
    memory[0] = 0xe9;
    std::memcpy(memory + 1, &directOffset, sizeof(directOffset));
    detail::DescribeBranch(reinterpret_cast<std::uintptr_t>(memory), branch, branchOwner, branchRva);
    Require(branch == static_cast<unsigned>(RenderEndpointBranchKind::RelativeE9) &&
                branchOwner == static_cast<unsigned>(RenderEndpointOwnerClass::PrivateExecutable) && !branchRva,
            "one relative branch reports private executable metadata without following a hook chain", checks);
    memory[0] = 0xff;
    memory[1] = 0x25;
    const std::int32_t slotOffset = 58;
    std::memcpy(memory + 2, &slotOffset, sizeof(slotOffset));
    const auto ownTarget = reinterpret_cast<std::uintptr_t>(&Noop);
    std::memcpy(memory + 64, &ownTarget, sizeof(ownTarget));
    detail::DescribeBranch(reinterpret_cast<std::uintptr_t>(memory), branch, branchOwner, branchRva);
    Require(
        branch == static_cast<unsigned>(RenderEndpointBranchKind::IndirectFF25) && branchOwner == static_cast<unsigned>(RenderEndpointOwnerClass::GameImage),
        "one indirect branch reads only its bounded pointer slot and reports cached owner", checks);
    detail::DescribeBranch(1, branch, branchOwner, branchRva);
    Require(branch == static_cast<unsigned>(RenderEndpointBranchKind::ReadFault) && !branchOwner && !branchRva,
            "unreadable diagnostic code never calls a foreign endpoint", checks);
    std::uintptr_t branchAddress = 0;
    Require(!detail::RelativeAddress((std::numeric_limits<std::uintptr_t>::max)(), 5, 0, branchAddress) && !detail::RelativeAddress(0, 0, -1, branchAddress),
            "branch metadata rejects address overflow and underflow", checks);
    ReleaseFrameBoundaryDiagnosticModules();
    std::memcpy(memory, code.data(), code.size());
    PrimeBoundaryDiagnostic(0x100000002ull);
    detail::runtime.preparation.errorDomain = static_cast<unsigned>(RenderEndpointErrorDomain::Win32);
    detail::runtime.preparation.errorCode = 123;
    detail::PreparationFailure(RenderEndpointPrepareGate::LoadedCodeHash, RenderEndpointPrepareReason::Mismatch);
    Require(detail::runtime.preparation.preparationGate == 13 && detail::runtime.preparation.preparationReason == 3 &&
                !detail::runtime.preparation.errorDomain && !detail::runtime.preparation.errorCode,
            "signature mismatch clears unrelated prior OS error metadata", checks);
    FrameBoundaryScope diagnosticScope;
    diagnosticScope.diagnostic = detail::runtime.preparation;
    diagnosticScope.diagnostic.slotStatus = static_cast<unsigned>(RenderEndpointSlotStatus::Read);
    diagnosticScope.diagnostic.slotOwnerClass = static_cast<unsigned>(RenderEndpointOwnerClass::OtherImage);
    diagnosticScope.diagnostic.slotRva = 0x2468;
    PublishFrameBoundaryEndpointDiagnostic(diagnosticScope, 0x300000004ull);
    RenderEndpointDiagnosticV1 diagnostic;
    Require(ReadFrameBoundaryEndpointDiagnostic(0x100000002ull, diagnostic) && diagnostic.generationHigh == 1 && diagnostic.acceptedSnapshotsLow == 4 &&
                diagnostic.acceptedSnapshotsHigh == 3 && diagnostic.slotRva == 0x2468,
            "separate diagnostic cache coherently publishes named scalar generations and counters", checks);
    diagnostic.version = 99;
    Require(!ReadFrameBoundaryEndpointDiagnostic(0x100000003ull, diagnostic) && diagnostic.version == 99,
            "stale diagnostic generation leaves caller output untouched", checks);
    detail::runtime.dxgi = 0x12345000;
    ResetFrameBoundaryObservation();
    Require(!detail::runtime.dxgi && !detail::runtime.diagnostic.version, "new generation clears old system endpoint candidate before any failed module lookup",
            checks);
    detail::runtime.preparation.version = 1;
    detail::PreparationFailure(RenderEndpointPrepareGate::ModuleLookup, RenderEndpointPrepareReason::OperationFailed, RenderEndpointErrorDomain::Win32,
                               ERROR_MOD_NOT_FOUND);
    std::array<std::uintptr_t, 9> oldTable{};
    oldTable[8] = 0x12345000 + 0x19000;
    std::uintptr_t oldSwapchain = reinterpret_cast<std::uintptr_t>(oldTable.data());
    put(view + 0x60, reinterpret_cast<std::uintptr_t>(&oldSwapchain));
    FrameBoundaryScope               failedLookup;
    CustomPresentClassificationState absent;
    EnterFrameBoundaryScope(failedLookup, 0, viewport.data(), GetCurrentThreadId(), absent);
    Require(failedLookup.diagnostic.preparationGate == 1 && failedLookup.diagnostic.slotStatus == 1 && !failedLookup.diagnostic.slotEqualsPinned,
            "failed lookup cannot match a slot at the prior generation's candidate base", checks);
    LeaveFrameBoundaryScope(failedLookup);
    const std::array<std::array<unsigned char, 16>, 4> prologues{{
        {0x48, 0x89, 0x5c, 0x24, 0x10},        // full pinned Resize starts with one 5B mov
        {0x40, 0x53, 0x48, 0x83, 0xec, 0x20},  // notify push2 + sub4
        {0x40, 0x56, 0x48, 0x83, 0xec, 0x40},  // destructor push2 + sub4
        {0x48, 0x89, 0x5c, 0x24, 0x10}         // full pinned System32 Present first mov
    }};
    const std::array<std::size_t, 4>                   stolen{5, 6, 6, 5};
    for (std::size_t index = 0; index < prologues.size(); ++index)
    {
        std::array<unsigned char, 64> copied{};
        std::size_t                   consumed = 0;
        while (consumed < 5)
        {
            PVOID      instructionTarget = nullptr, pool = nullptr;
            LONG       extra = 0;
            const auto next = static_cast<const unsigned char*>(DetourCopyInstruction(
                copied.data() + consumed, &pool, const_cast<unsigned char*>(prologues[index].data() + consumed), &instructionTarget, &extra));
            Require(next > prologues[index].data() + consumed && extra == 0, "pinned prologue instruction copies without expansion", checks);
            consumed = static_cast<std::size_t>(next - prologues[index].data());
        }
        Require(consumed == stolen[index], "Detours actual AMD64 decoder verifies exact hardcoded stolen length", checks);
    }
    const auto savedRva = detail::runtime.notifyCode.rva;
    detail::runtime.image = 0;
    detail::runtime.notifyCode.rva = reinterpret_cast<std::uintptr_t>(memory);
    detail::runtime.notify = reinterpret_cast<detail::Notify>(memory);
    Require(detail::CheckFunction(0, detail::runtime.notifyCode), "complete original notify fingerprint before attach", checks);
    Require(ChangeOwnedNotify(true), "attach actual Detours to owned unexecuted notification copy", checks);
    Require(detail::CaptureOwnedPatch(0, detail::runtime.notifyCode, 6, reinterpret_cast<void*>(&detail::HookNotify)),
            "own E9 relay, CC byte, and untouched tail match", checks);
    detail::runtime.committed = true;
    std::array<unsigned char, 44> recovered{};
    Require(ReadFrameBoundaryOwnedOriginal(reinterpret_cast<std::uintptr_t>(memory), recovered.data(), recovered.size()) && recovered == code,
            "API5 normalizes only its complete verified owned patch", checks);
    for (const auto offset : std::array<std::size_t, 3>{0, 5, 43})
    {
        memory[offset] ^= 1;
        Require(!ReadFrameBoundaryOwnedOriginal(reinterpret_cast<std::uintptr_t>(memory), recovered.data(), recovered.size()),
                "entry, padding or tail tamper refuses normalization", checks);
        memory[offset] ^= 1;
    }
    memory[43] ^= 1;
    Require(!detail::CaptureOwnedPatch(0, detail::runtime.notifyCode, 6, reinterpret_cast<void*>(&detail::HookNotify)) && !detail::runtime.notifyCode.captured,
            "post-commit failed full-tail inspection leaves no usable normalized-byte cache", checks);
    Require(!detail::OwnedForDetach(0, detail::runtime.notifyCode), "End retains own storage while the untouched tail is foreign", checks);
    memory[43] ^= 1;
    Require(detail::OwnedForDetach(0, detail::runtime.notifyCode), "cleanup retry requires restored exact own branch and original tail", checks);
    detail::runtime.committed = false;
    Require(!ReadFrameBoundaryOwnedOriginal(reinterpret_cast<std::uintptr_t>(memory), recovered.data(), recovered.size()),
            "retired or wrong generation cannot normalize old patch", checks);
    Require(ChangeOwnedNotify(false) && std::memcmp(memory, code.data(), code.size()) == 0, "successful detach restores every original function byte", checks);
    detail::runtime.notifyCode.rva = savedRva;
    detail::runtime.notify = nullptr;
    VirtualFree(memory, 0, MEM_RELEASE);
    auto* multiple = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    Require(multiple != nullptr, "owned multi-hook validation-failure fixture", checks);
    const auto        resizeRva = detail::runtime.resizeCode.rva, destroyRva = detail::runtime.destroyCode.rva, presentRva = detail::runtime.presentCode.rva;
    detail::Function* functions[]{&detail::runtime.resizeCode, &detail::runtime.notifyCode, &detail::runtime.destroyCode, &detail::runtime.presentCode};
    for (std::size_t i = 0; i < 4; ++i)
    {
        auto& function = *functions[i];
        function.rva = reinterpret_cast<std::uintptr_t>(multiple + i * 2048);
        function.original = {};
        function.installed = {};
        function.captured = false;
        std::memcpy(function.original.data(), prologues[i].data(), prologues[i].size());
        std::memcpy(multiple + i * 2048, function.original.data(), function.bytes);
    }
    detail::runtime.image = detail::runtime.dxgi = 0;
    detail::runtime.endpoint = detail::runtime.prepared = true;
    detail::runtime.resize = reinterpret_cast<detail::Resize>(functions[0]->rva);
    detail::runtime.notify = reinterpret_cast<detail::Notify>(functions[1]->rva);
    detail::runtime.destroy = reinterpret_cast<detail::Notify>(functions[2]->rva);
    detail::runtime.present = reinterpret_cast<detail::Present>(functions[3]->rva);
    Require(DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR && ChangeFrameBoundaryHooks(true) &&
                DetourTransactionCommit() == NO_ERROR,
            "one transaction installs four owned unexecuted hook fixtures", checks);
    multiple[1215] ^= 1;
    Require(!FrameBoundaryHooksCommitted(true) && !detail::runtime.armed && !functions[0]->captured && functions[1]->captured && functions[2]->captured &&
                functions[3]->captured,
            "failed first post-commit capture still inspects every later installed function", checks);
    Require(!FrameBoundaryHooksOwned(), "post-commit foreign tail forces retention rather than blind rollback", checks);
    multiple[1215] ^= 1;
    Require(FrameBoundaryHooksOwned() && functions[0]->captured, "cleanup retries an initially uncaptured but now exactly owned patch", checks);
    Require(DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR && ChangeFrameBoundaryHooks(false) &&
                DetourTransactionCommit() == NO_ERROR && FrameBoundaryHooksCommitted(false),
            "validated retry detaches the entire owned transaction", checks);
    bool restored = true;
    for (std::size_t i = 0; i < 4; ++i)
        restored = restored && std::memcmp(multiple + i * 2048, functions[i]->original.data(), functions[i]->bytes) == 0;
    Require(restored, "multi-hook retry restores every complete original byte sequence", checks);
    detail::runtime.resizeCode.rva = resizeRva;
    detail::runtime.notifyCode.rva = savedRva;
    detail::runtime.destroyCode.rva = destroyRva;
    detail::runtime.presentCode.rva = presentRva;
    VirtualFree(multiple, 0, MEM_RELEASE);
    ResetFrameBoundaryObservation();
    // These are copied disk bytes in our own unexecuted allocation. Only our
    // executable's harmless target is called; no game code or DXGI is invoked.
    std::array<unsigned char, 429> wrapperBytes{};
    std::ifstream                  wrapperInput(wrapperFixture, std::ios::binary);
    wrapperInput.read(reinterpret_cast<char*>(wrapperBytes.data()), wrapperBytes.size());
    Require(wrapperInput.gcount() == static_cast<std::streamsize>(wrapperBytes.size()), "owned callsite fixture has the complete pinned wrapper", checks);
    auto* wrapperMemory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    Require(wrapperMemory != nullptr, "owned wrapper allocation", checks);
    std::memcpy(wrapperMemory, wrapperBytes.data(), wrapperBytes.size());
    const auto oldWrapper = detail::runtime.wrapperCode;
    detail::runtime.wrapperCode.rva = reinterpret_cast<std::uintptr_t>(wrapperMemory);
    detail::runtime.image = 0;
    detail::runtime.nativeDispatch = true;
    detail::runtime.wrapperHook = reinterpret_cast<std::uintptr_t>(&detail::HookNativeDispatch);
    Require(detail::CheckFunction(0, detail::runtime.wrapperCode) && detail::PrepareDispatchRelay(),
            "exact whole-wrapper pin prepares an owned near leaf relay", checks);
    MEMORY_BASIC_INFORMATION relayPage{};
    Require(VirtualQuery(detail::runtime.dispatchRelay, &relayPage, sizeof(relayPage)) == sizeof(relayPage) && relayPage.Protect == PAGE_EXECUTE_READ,
            "generated leaf is RX before any source callsite patch", checks);
    const auto                              wrapperAddress = reinterpret_cast<std::uintptr_t>(wrapperMemory);
    const auto                              relayAddress = reinterpret_cast<std::uintptr_t>(detail::runtime.dispatchRelay);
    const auto                              gateResize = detail::runtime.resize;
    const auto                              gateNotify = detail::runtime.notify;
    const auto                              gateDestroy = detail::runtime.destroy;
    const auto                              gatePresent = detail::runtime.present;
    const bool                              gateCommitted = detail::runtime.committed, gateEndpoint = detail::runtime.endpoint;
    constexpr std::array<std::uintptr_t, 5> gateTrampolines{0x110000, 0x120000, 0x130000, 0x140000, 0x150000};
    detail::runtime.resize = reinterpret_cast<detail::Resize>(gateTrampolines[1]);
    detail::runtime.notify = reinterpret_cast<detail::Notify>(gateTrampolines[2]);
    detail::runtime.destroy = reinterpret_cast<detail::Notify>(gateTrampolines[3]);
    detail::runtime.present = reinterpret_cast<detail::Present>(gateTrampolines[4]);
    detail::runtime.committed = detail::runtime.endpoint = true;
    const std::array<std::uintptr_t, 5> gateEntries{
        wrapperAddress, detail::runtime.image + detail::runtime.resizeCode.rva, detail::runtime.image + detail::runtime.notifyCode.rva,
        detail::runtime.image + detail::runtime.destroyCode.rva, detail::runtime.dxgi + detail::runtime.presentCode.rva};
    constexpr std::array<std::size_t, 5> gateStolen{7, 5, 6, 6, 5};
    for (std::size_t entry = 0; entry < gateEntries.size(); ++entry)
    {
        for (std::size_t byte = 0; byte < gateStolen[entry]; ++byte)
            Require(!NativeDispatchInstructionSafe(gateEntries[entry] + byte, gateTrampolines[0]),
                    "every modified vendor entry byte refuses a stopped control IP before commit", checks);
        Require(NativeDispatchInstructionSafe(gateEntries[entry] + gateStolen[entry], gateTrampolines[0]),
                "first unchanged entry byte is beyond its relocation window", checks);
    }
    for (const auto trampoline : gateTrampolines)
    {
        for (std::size_t byte = 0; byte < 96; ++byte)
            Require(!NativeDispatchInstructionSafe(trampoline + byte, gateTrampolines[0]),
                    "complete pinned96B vendor trampoline includes inbound rbCodeIn before lifetime increment", checks);
        Require(NativeDispatchInstructionSafe(trampoline + 96, gateTrampolines[0]), "first byte beyond owned trampoline is not a relocation dependency",
                checks);
    }
    detail::runtime.resize = gateResize;
    detail::runtime.notify = gateNotify;
    detail::runtime.destroy = gateDestroy;
    detail::runtime.present = gatePresent;
    detail::runtime.committed = gateCommitted;
    detail::runtime.endpoint = gateEndpoint;
    for (std::size_t index = 0; index < 5; ++index)
        Require(!NativeDispatchInstructionSafe(wrapperAddress + detail::dispatchOffset + index),
                "every byte in the replaced instruction window refuses suspended IP", checks);
    Require(NativeDispatchInstructionSafe(wrapperAddress + detail::dispatchOffset + 5), "unchanged native return address lies beyond the patch window", checks);
    for (std::size_t index = 0; index < 20; ++index)
        Require(!NativeDispatchInstructionSafe(relayAddress + index), "every leaf instruction and literal refuses free while a control IP is inside", checks);
    Require(!NativeDispatchInstructionSafe(detail::runtime.dispatchHookBegin) && !NativeDispatchInstructionSafe(detail::runtime.dispatchHookEnd - 1),
            "first/last C++ hook instruction counter gaps retain code", checks);
    Require(ChangeNativeDispatchCallsite(true, false) && detail::CurrentWrapper(false), "owned E8 patch normalizes only exact five replaced bytes", checks);
    void* keptRelay = detail::runtime.dispatchRelay;
    ReleaseNativeDispatchRelay();
    Require(detail::runtime.dispatchRelay == keptRelay, "installed callsite retains its leaf storage", checks);
    for (const auto offset : {detail::dispatchOffset + 1, std::size_t{428}})
    {
        wrapperMemory[offset] ^= 1;
        Require(!detail::CurrentWrapper(false) && !ChangeNativeDispatchCallsite(false, false),
                "foreign call/tail modification refuses normalization and cleanup", checks);
        wrapperMemory[offset] ^= 1;
    }
    DWORD relayProtection = 0;
    Require(VirtualProtect(keptRelay, 4096, PAGE_READWRITE, &relayProtection) != FALSE, "owned relay tamper fixture becomes writable", checks);
    static_cast<unsigned char*>(keptRelay)[19] ^= 1;
    DWORD ignoredProtection = 0;
    VirtualProtect(keptRelay, 4096, relayProtection, &ignoredProtection);
    Require(!detail::CurrentWrapper(false), "a changed relay destination cannot reuse an unchanged E8 as ownership proof", checks);
    VirtualProtect(keptRelay, 4096, PAGE_READWRITE, &ignoredProtection);
    static_cast<unsigned char*>(keptRelay)[19] ^= 1;
    VirtualProtect(keptRelay, 4096, relayProtection, &ignoredProtection);
    PVOID wrapperOriginal = wrapperMemory;
    Require(DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR &&
                DetourAttach(&wrapperOriginal, reinterpret_cast<PVOID>(&detail::HookNativeDispatch)) == NO_ERROR && DetourTransactionCommit() == NO_ERROR,
            "owned native-entry detour coexists with the separate owned CALL", checks);
    Require(detail::CurrentWrapper(true), "full wrapper normalizes both owned entry branch and owned callsite, including the original tail", checks);
    Require(ChangeNativeDispatchCallsite(false, true) && DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR &&
                DetourDetach(&wrapperOriginal, reinterpret_cast<PVOID>(&detail::HookNativeDispatch)) == NO_ERROR && DetourTransactionCommit() == NO_ERROR &&
                detail::CurrentWrapper(false),
            "entry/callsite teardown restores all429 original bytes", checks);
    Require(NativeDispatchSafeToResume(false), "post-vendor finalization clears the direct fixture's pending writable-page state", checks);
    DWORD fixtureProtection = 0;
    Require(VirtualProtect(wrapperMemory, 4096, PAGE_EXECUTE_READ, &fixtureProtection) && CaptureNativeDispatchProtectionBaseline() &&
                detail::runtime.dispatchOriginalProtection == PAGE_EXECUTE_READ,
            "true pretransaction protection is refreshed under the owned transaction guard", checks);
    Require(DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR &&
                DetourAttach(&wrapperOriginal, reinterpret_cast<PVOID>(&detail::HookNativeDispatch)) == NO_ERROR && ChangeNativeDispatchCallsite(true, false) &&
                DetourTransactionCommit() == NO_ERROR,
            "vendor and custom patches share the same temporarily writable native page", checks);
    // Model failed own restoration while Detours had that page RWX. Recovery
    // must use the earlier RX baseline, never the vendor's temporary value.
    DWORD temporaryProtection = 0;
    Require(VirtualProtect(wrapperMemory, 4096, PAGE_EXECUTE_READWRITE, &temporaryProtection) != FALSE,
            "owned restoration fault leaves a writable page for the real recovery helper", checks);
    detail::runtime.dispatchProtectionPending = true;
    detail::runtime.dispatchProtection = PAGE_EXECUTE_READWRITE;
    Require(NativeDispatchSafeToResume(true) && VirtualQuery(wrapperMemory, &relayPage, sizeof(relayPage)) == sizeof(relayPage) &&
                relayPage.Protect == PAGE_EXECUTE_READ && !detail::runtime.dispatchProtectionPending,
            "after vendor completion protection-failure recovery proves exact original RX before any resume", checks);
    Require(CaptureNativeDispatchProtectionBaseline() && DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR &&
                DetourDetach(&wrapperOriginal, reinterpret_cast<PVOID>(&detail::HookNativeDispatch)) == NO_ERROR && ChangeNativeDispatchCallsite(false, true) &&
                DetourTransactionCommit() == NO_ERROR && NativeDispatchSafeToResume(false) &&
                VirtualQuery(wrapperMemory, &relayPage, sizeof(relayPage)) == sizeof(relayPage) && relayPage.Protect == PAGE_EXECUTE_READ,
            "detach also restores the true baseline after vendor and custom writes", checks);
    Require(VirtualProtect(wrapperMemory, 4096, fixtureProtection, &temporaryProtection) && CaptureNativeDispatchProtectionBaseline(),
            "owned fixture restores its initial writable permission before later deliberate byte faults", checks);
    wrapperMemory[428] ^= 1;
    Require(!ChangeNativeDispatchCallsite(true, false) && !NativeDispatchPatchPresent() && NativeDispatchSafeToResume(false),
            "pure foreign-tail refusal before any owned write does not safety-hold peers merely because a relay was allocated", checks);
    wrapperMemory[428] ^= 1;
    discoveryTrigger = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    discoveryReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    const HANDLE discoveryStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Require(discoveryTrigger && discoveryReady && discoveryStop, "controlled child-creation fixture events", checks);
    std::thread spawner(
        [&]
        {
            WaitForSingleObject(discoveryTrigger, INFINITE);
            std::thread child(
                [&]
                {
                    discoveryChildId.store(GetCurrentThreadId());
                    SetEvent(discoveryReady);
                    WaitForSingleObject(discoveryStop, INFINITE);
                });
            WaitForSingleObject(discoveryStop, INFINITE);
            child.join();
        });
    Require(OwnedNativeDispatchDiscovery(&TriggerLateChild, &LateChildId),
            "production stable-pass guard finds and holds a child created by a spawner after the first actual snapshot", checks);
    SetEvent(discoveryStop);
    spawner.join();
    CloseHandle(discoveryTrigger);
    CloseHandle(discoveryReady);
    CloseHandle(discoveryStop);
    std::atomic<bool> sleeperReady{false}, sleeperDone{false};
    std::thread       sleeper(
        [&]
        {
            sleeperReady.store(true);
            while (!sleeperDone.load())
                Sleep(1);
        });
    while (!sleeperReady.load())
        Sleep(1);
    const HANDLE sleeperHandle = sleeper.native_handle();
    Require(SuspendThread(sleeperHandle) != static_cast<DWORD>(-1), "owned suspension outlives the Detours transaction", checks);
    CONTEXT suspended{};
    suspended.ContextFlags = CONTEXT_CONTROL;
    Require(GetThreadContext(sleeperHandle, &suspended) && NativeDispatchInstructionSafe(static_cast<std::uintptr_t>(suspended.Rip)),
            "suspended real control context is checked before the custom write", checks);
    Require(DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(sleeperHandle) == NO_ERROR && ChangeNativeDispatchCallsite(true, false),
            "custom CALL is written while both owned and Detours suspend counts are held", checks);
    PVOID invalidTarget = nullptr;
    Require(DetourAttach(&invalidTarget, reinterpret_cast<PVOID>(&detail::HookNativeDispatch)) != NO_ERROR && DetourTransactionCommit() != NO_ERROR,
            "injected transaction error aborts vendor patches", checks);
    Require(SuspendThread(sleeperHandle) == 1 && ResumeThread(sleeperHandle) == 2 && NativeDispatchPatchPresent(),
            "vendor failure resumes only its own count and leaves custom rollback under owned suspension", checks);
    Require(ChangeNativeDispatchCallsite(false, false) && detail::CurrentWrapper(false), "failed transaction custom patch rolls back before owned resume",
            checks);
    Require(ChangeNativeDispatchCallsite(true, false), "owned partial-write fault starts from a verified installed CALL", checks);
    wrapperMemory[detail::dispatchOffset + 2] ^= 0x80;
    // Model a fault after the worker validated ownership and started its own
    // five-byte copy. A pre-write foreign refusal must never acquire this mark.
    detail::runtime.dispatchUnsafeWrite = true;
    bool safetyPending = false;
    Require(!OwnedNativeDispatchCleanup(safetyPending) && safetyPending && OwnedNativeDispatchSafetyHeld(),
            "unknown partial instruction retains every production-owned suspension instead of only retaining relay storage", checks);
    Require(SuspendThread(sleeperHandle) == 2 && ResumeThread(sleeperHandle) == 3, "failed rollback did not resume the safety-held owned count", checks);
    safetyPending = false;
    Require(!OwnedNativeDispatchCleanup(safetyPending) && safetyPending && OwnedNativeDispatchSafetyHeld(),
            "cleanup retry refuses before resuming safety-held threads while instruction bytes remain unknown", checks);
    Require(SuspendThread(sleeperHandle) == 2 && ResumeThread(sleeperHandle) == 3, "unsafe retry also preserves the owned suspend count", checks);
    std::memcpy(wrapperMemory + detail::dispatchOffset, wrapperBytes.data() + detail::dispatchOffset, 5);
    safetyPending = false;
    Require(OwnedNativeDispatchCleanup(safetyPending) && !safetyPending && !OwnedNativeDispatchSafetyHeld() && !NativeDispatchPatchPresent(),
            "cleanup proves restored original bytes while held before balancing its suspension and releasing relay", checks);
    Require(!detail::runtime.dispatchRelay && SuspendThread(sleeperHandle) == 1 && ResumeThread(sleeperHandle) == 2,
            "safe retry frees relay and leaves only the fixture's external hold", checks);
    Require(detail::PrepareDispatchRelay(), "fresh owned relay supports separate in-flight retention test", checks);
    keptRelay = detail::runtime.dispatchRelay;
    detail::runtime.inFlight.store(1);
    ReleaseNativeDispatchRelay();
    Require(detail::runtime.dispatchRelay == keptRelay, "an in-flight callback retains the now-unpatched relay until drain", checks);
    detail::runtime.inFlight.store(0);
    ReleaseNativeDispatchRelay();
    Require(!detail::runtime.dispatchRelay, "quiescent restored callsite permits relay release while owned threads remain held", checks);
    Require(ResumeThread(sleeperHandle) == 1, "owned suspension count is balanced independently of Detours", checks);
    sleeperDone.store(true);
    sleeper.join();
    detail::scope = nullptr;
    for (const HRESULT expected : {S_OK, S_FALSE, E_FAIL})
    {
        dispatchResult = expected;
        const auto prior = dispatchForwards;
        Require(detail::HookNativeDispatch(nullptr, 7, 0, &OwnedDispatchTarget) == expected && dispatchForwards == prior + 1 && !FrameBoundaryInFlight(),
                "unmatched natural dispatch forwards the captured target exactly once and preserves HRESULT", checks);
    }
    const auto priorThrows = dispatchForwards;
    Require(CatchDispatchException() && dispatchForwards == priorThrows + 1 && !FrameBoundaryInFlight() && !detail::dispatchDepth,
            "target SEH propagates unchanged and balances dispatch lifetime", checks);
    detail::runtime.wrapperCode = oldWrapper;
    detail::runtime.nativeDispatch = false;
    VirtualFree(wrapperMemory, 0, MEM_RELEASE);
    ResetFrameBoundaryObservation();
    const auto flowResizeCode = detail::runtime.resizeCode, flowNotifyCode = detail::runtime.notifyCode, flowDestroyCode = detail::runtime.destroyCode,
               flowPresentCode = detail::runtime.presentCode, flowWrapperCode = detail::runtime.wrapperCode;
    for (const bool ownedEndpoint : {false, true})
    {
        // The live failure occurred after admission, with strict DXGI refused.
        // Reproduce the complete production transaction without invoking any
        // copied game bytes. Wrapper/Resize/destructor share their native page
        // offsets so page-permission ordering is exercised too. Lifecycle
        // bodies are owned inert buffers with the already-decoded prologues.
        auto* flowMemory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 3 * 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(flowMemory != nullptr, "complete API6 transaction owns its unexecuted code pages", checks);
        constexpr std::array<std::size_t, 4> flowOffsets{0x8e0, 0x1780, 0x480, 0x2000};
        detail::Function* flowFunctions[]{&detail::runtime.resizeCode, &detail::runtime.notifyCode, &detail::runtime.destroyCode, &detail::runtime.presentCode};
        for (std::size_t function = 0; function < flowOffsets.size(); ++function)
        {
            auto& ownedFunction = *flowFunctions[function];
            ownedFunction.rva = reinterpret_cast<std::uintptr_t>(flowMemory + flowOffsets[function]);
            ownedFunction.original = ownedFunction.installed = {};
            ownedFunction.captured = false;
            ownedFunction.stolen = ownedFunction.detour = 0;
            std::memcpy(ownedFunction.original.data(), prologues[function].data(), prologues[function].size());
            std::memcpy(flowMemory + flowOffsets[function], ownedFunction.original.data(), ownedFunction.bytes);
        }
        const auto flowWrapper = reinterpret_cast<std::uintptr_t>(flowMemory + 0xda0);
        std::memcpy(flowMemory + 0xda0, wrapperBytes.data(), wrapperBytes.size());
        std::array<unsigned char, 3 * 4096> flowExpected{};
        std::memcpy(flowExpected.data(), flowMemory, flowExpected.size());
        DWORD flowOldProtection = 0;
        Require(VirtualProtect(flowMemory, flowExpected.size(), PAGE_EXECUTE_READ, &flowOldProtection) != FALSE,
                "complete API6 fixture begins with every owned code page RX", checks);
        detail::runtime.image = detail::runtime.dxgi = 0;
        detail::runtime.nativeDispatch = detail::runtime.prepared = true;
        detail::runtime.endpoint = ownedEndpoint;
        detail::runtime.wrapperCode.rva = flowWrapper;
        detail::runtime.wrapperHook = reinterpret_cast<std::uintptr_t>(OwnedNativeDispatchWrapperHook());
        detail::runtime.resize = reinterpret_cast<detail::Resize>(flowFunctions[0]->rva);
        detail::runtime.notify = reinterpret_cast<detail::Notify>(flowFunctions[1]->rva);
        detail::runtime.destroy = reinterpret_cast<detail::Notify>(flowFunctions[2]->rva);
        detail::runtime.present = ownedEndpoint ? reinterpret_cast<detail::Present>(flowFunctions[3]->rva) : nullptr;
        Require(detail::CheckFunction(0, detail::runtime.wrapperCode) && detail::PrepareDispatchRelay(),
                "complete production transaction uses the full pinned wrapper and actual native observer hook target", checks);
        const HANDLE flowReady = CreateEventW(nullptr, TRUE, FALSE, nullptr), flowStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Require(flowReady && flowStop, "complete API6 transaction sleeper barriers", checks);
        std::thread flowSleeper(
            [&]
            {
                SetEvent(flowReady);
                WaitForSingleObject(flowStop, INFINITE);
            });
        WaitForSingleObject(flowReady, INFINITE);
        Sleep(5);
        Require(SuspendThread(flowSleeper.native_handle()) == 0, "fixture owns an independent pre-existing sleeper suspension", checks);
        bool flowPending = false;
        for (const bool afterHolds : {false, true})
        {
            RenderSetupDiagnosticV1 refusedSetup;
            Require(OwnedNativeDispatchSetupRefusal(flowWrapper, afterHolds, refusedSetup),
                    "real install pipeline refuses deterministically before writes and balances any holds", checks);
            const auto expectedStage = afterHolds ? RenderSetupStage::WrapperChange : RenderSetupStage::NativeThreadStubRead;
            Require(refusedSetup.version == 1 && refusedSetup.attempted == 1 && refusedSetup.stage == static_cast<unsigned>(expectedStage) &&
                        refusedSetup.failureStage == refusedSetup.stage && !refusedSetup.installSucceeded && !refusedSetup.transactionCommitted &&
                        !refusedSetup.hooksCommitted && !refusedSetup.retainedSuspensions && !refusedSetup.safetyHeldSuspensions &&
                        (afterHolds ? refusedSetup.errorDomain == static_cast<unsigned>(RenderSetupErrorDomain::Detours) &&
                                          refusedSetup.errorCode == ERROR_INVALID_HANDLE && refusedSetup.initialThreads >= 1 &&
                                          refusedSetup.heldThreads >= refusedSetup.initialThreads && refusedSetup.stablePasses >= 2
                                    : !refusedSetup.errorDomain && !refusedSetup.errorCode && !refusedSetup.initialThreads && !refusedSetup.heldThreads),
                    "first setup refusal records exact stage/error and truthful precommit attempt history", checks);
            flowPending = false;
            Require(OwnedNativeDispatchTransaction(false, flowWrapper, flowPending) && !flowPending,
                    "refused install remains safely detachable without a retry or admission change", checks);
            RenderSetupDiagnosticV1 cleanedRefusal;
            OwnedNativeDispatchSetupSnapshot(cleanedRefusal);
            Require(cleanedRefusal.stage == refusedSetup.stage && cleanedRefusal.failureStage == refusedSetup.failureStage &&
                        cleanedRefusal.errorDomain == refusedSetup.errorDomain && cleanedRefusal.errorCode == refusedSetup.errorCode &&
                        !cleanedRefusal.transactionCommitted && !cleanedRefusal.hooksCommitted && !cleanedRefusal.installSucceeded,
                    "successful cleanup cannot overwrite or promote the original install refusal", checks);
            Require(detail::PrepareDispatchRelay(), "next independent owned install prepares fresh leaf storage after refused-attempt cleanup", checks);
        }
        if (!ownedEndpoint)
            for (const unsigned refusalMode : {1u, 2u, 3u, 4u})
            {
                RenderSetupDiagnosticV1 refusedThread;
                Require(OwnedNativeDispatchExitingInstall(flowWrapper, refusalMode, refusedThread),
                        "full owned install keeps live denial, failed wait, other errors and repeated retained exits fail closed", checks);
                flowPending = false;
                Require(OwnedNativeDispatchTransaction(false, flowWrapper, flowPending) && !flowPending,
                        "stable-thread refusal owns no hook transaction and leaves ordinary safe cleanup available", checks);
                Require(detail::PrepareDispatchRelay(), "next stable-thread control receives fresh owned relay storage", checks);
            }
        flowPending = false;
        RenderSetupDiagnosticV1 installedSetup;
        Require(OwnedNativeDispatchExitingInstall(flowWrapper, 0, installedSetup),
                ownedEndpoint ? "complete production API6 install with owned strict endpoint succeeds"
                              : "complete production API6 install with strict endpoint refused succeeds",
                checks);
        OwnedNativeDispatchSetupSnapshot(installedSetup);
        Require(installedSetup.version == 1 && installedSetup.attempted == 1 && installedSetup.stage == static_cast<unsigned>(RenderSetupStage::Complete) &&
                    !installedSetup.failureStage && !installedSetup.errorDomain && !installedSetup.errorCode && installedSetup.transactionCommitted == 1 &&
                    installedSetup.hooksCommitted == 1 && installedSetup.installSucceeded == 1 && installedSetup.initialThreads >= 1 &&
                    installedSetup.heldThreads >= installedSetup.initialThreads && installedSetup.stablePasses >= 2 && !installedSetup.retainedSuspensions &&
                    !installedSetup.safetyHeldSuspensions,
                "complete install diagnostic distinguishes verified ownership and full setup success", checks);
        Require(
            detail::runtime.armed && detail::runtime.committed && detail::runtime.dispatchPatched && FrameBoundaryHooksOwned() && detail::CurrentWrapper(true),
            "one production transaction owns all native entries, lifecycle hooks and CALL before observation", checks);
        Require(SuspendThread(flowSleeper.native_handle()) == 1 && ResumeThread(flowSleeper.native_handle()) == 2,
                "full install balances only production holds and preserves the independent sleeper count", checks);
        for (std::size_t page = 0; page < 3; ++page)
        {
            MEMORY_BASIC_INFORMATION flowPage{};
            Require(VirtualQuery(flowMemory + page * 4096, &flowPage, sizeof(flowPage)) == sizeof(flowPage) && flowPage.Protect == PAGE_EXECUTE_READ,
                    "full install restores every shared/native page to RX before resuming peers", checks);
        }
        flowPending = false;
        Require(OwnedNativeDispatchTransaction(false, flowWrapper, flowPending) && !flowPending,
                "complete production API6 detach succeeds through the same ownership and context guards", checks);
        RenderSetupDiagnosticV1 detachedSetup;
        OwnedNativeDispatchSetupSnapshot(detachedSetup);
        Require(detachedSetup.stage == installedSetup.stage && !detachedSetup.failureStage && detachedSetup.transactionCommitted == 1 &&
                    detachedSetup.hooksCommitted == 1 && detachedSetup.installSucceeded == 1 && !detachedSetup.retainedSuspensions &&
                    !detachedSetup.safetyHeldSuspensions,
                "detach retains historical installation facts while current retained-hold facts clear", checks);
        Require(!detail::runtime.armed && !detail::runtime.committed && !NativeDispatchPatchPresent() && !detail::runtime.dispatchRelay &&
                    std::memcmp(flowMemory, flowExpected.data(), flowExpected.size()) == 0,
                "full detach restores every owned byte, disarms hooks and releases leaf storage", checks);
        Require(SuspendThread(flowSleeper.native_handle()) == 1 && ResumeThread(flowSleeper.native_handle()) == 2,
                "full detach also preserves exactly the independent sleeper suspension", checks);
        for (std::size_t page = 0; page < 3; ++page)
        {
            MEMORY_BASIC_INFORMATION flowPage{};
            Require(VirtualQuery(flowMemory + page * 4096, &flowPage, sizeof(flowPage)) == sizeof(flowPage) && flowPage.Protect == PAGE_EXECUTE_READ,
                    "full detach leaves every owned code page at its exact RX baseline", checks);
        }
        SetEvent(flowStop);
        Require(ResumeThread(flowSleeper.native_handle()) == 1, "fixture balances its own final suspension after production teardown", checks);
        flowSleeper.join();
        CloseHandle(flowReady);
        CloseHandle(flowStop);
        ResetFrameBoundaryObservation();
        detail::runtime.resizeCode = flowResizeCode;
        detail::runtime.notifyCode = flowNotifyCode;
        detail::runtime.destroyCode = flowDestroyCode;
        detail::runtime.presentCode = flowPresentCode;
        detail::runtime.wrapperCode = flowWrapperCode;
        detail::runtime.resize = nullptr;
        detail::runtime.notify = detail::runtime.destroy = nullptr;
        detail::runtime.present = nullptr;
        VirtualFree(flowMemory, 0, MEM_RELEASE);
    }
}
