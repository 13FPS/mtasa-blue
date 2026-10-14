/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned fixtures for API5 query, lifetime, and patch admission faults
 *****************************************************************************/
// Private inclusion exposes the real helpers without creating shipping bypasses.
#include "../../Client/game_de/NativeFrameBoundaryProbe.cpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>

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

void CheckFrameBoundaryProbe(const char* customFixture, unsigned& checks)
{
    using namespace mta::game_de;
    namespace detail = mta::game_de::boundary_detail;
    using Status = FrameBoundaryObservationStatus;
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
            PVOID      target = nullptr, pool = nullptr;
            LONG       extra = 0;
            const auto next = static_cast<const unsigned char*>(
                DetourCopyInstruction(copied.data() + consumed, &pool, const_cast<unsigned char*>(prologues[index].data() + consumed), &target, &extra));
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
}
