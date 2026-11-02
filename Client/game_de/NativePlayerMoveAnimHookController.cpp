/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain exact movement call patches, unwind/code and current intent
 ****************************************************************************/
#include "NativePlayerMoveAnimHookController.h"
#include "NativePlayerMoveAnimHookProfile.h"
#include "NativeImageAccess.h"
// The shared policy includes CPed's existing unscoped enum forward declaration.
// Match the other DE SDK boundaries without changing the common SDK or hiding
// diagnostics from the native hook implementation below this include.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CPlayerMoveAnimPolicy.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace mta::game_de
{
    namespace HookControllerDetail
    {
        namespace P = NativePlayerMoveAnimHookProfile;
        namespace B = NativePlayerMoveAnimHookBridge;
        namespace I = NativeImageAccess;
        using C = NativePlayerMoveAnimHookController;
        constexpr std::uint32_t AllocationBytes = 65536, SecondStubOffset = B::MaximumBlobBytes;
        constexpr std::size_t   MaximumAllocationCandidates = 65536;
        struct Operation
        {
            std::atomic_flag& flag;
            bool              held;
            explicit Operation(std::atomic_flag& value) noexcept : flag(value), held(!flag.test_and_set()) {}
            ~Operation()
            {
                if (held)
                    flag.clear();
            }
            explicit operator bool() const noexcept { return held; }
        };
        BOOL Query(LPCVOID address, MEMORY_BASIC_INFORMATION* info, SIZE_T bytes) noexcept
        {
            return VirtualQuery(address, info, bytes) == bytes;
        }
        LPVOID Allocate(LPVOID address, SIZE_T bytes, DWORD type, DWORD protection) noexcept
        {
            return VirtualAlloc(address, bytes, type, protection);
        }
        BOOL Protect(LPVOID address, SIZE_T bytes, DWORD protection, PDWORD prior) noexcept
        {
            return VirtualProtect(address, bytes, protection, prior);
        }
        BOOL Write(HANDLE process, LPVOID target, LPCVOID bytes, SIZE_T size, SIZE_T* copied) noexcept
        {
            return WriteProcessMemory(process, target, bytes, size, copied);
        }
        BOOL Flush(HANDLE process, LPCVOID target, SIZE_T bytes) noexcept
        {
            return FlushInstructionCache(process, target, bytes);
        }
        BOOLEAN AddFunctions(PRUNTIME_FUNCTION functions, DWORD count, DWORD64 base) noexcept
        {
            return RtlAddFunctionTable(functions, count, base);
        }
        BOOLEAN DeleteFunctions(PRUNTIME_FUNCTION functions) noexcept
        {
            return RtlDeleteFunctionTable(functions);
        }
        BOOL Free(LPVOID address, SIZE_T bytes, DWORD type) noexcept
        {
            return VirtualFree(address, bytes, type);
        }
        bool Same(const C::Token& a, const C::Token& b) noexcept
        {
            return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
        }
        bool Same(const C::Ped& a, const C::Ped& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        bool Same(const C::Identity& a, const C::Identity& b) noexcept
        {
            return a.image == b.image && a.generation == b.generation && a.admissionSerial == b.admissionSerial && a.engineThread == b.engineThread;
        }
        bool Valid(const C::Owners& owners) noexcept
        {
            return owners.afterApplication && owners.resolvePublication && owners.decisionScope && owners.resolveDecision && owners.qualifyDecisionGroup;
        }
        bool Valid(const C::Operations& operations) noexcept
        {
            return operations.query && operations.allocate && operations.protect && operations.write && operations.flush && operations.addFunctions &&
                   operations.deleteFunctions && operations.free && operations.functionRange;
        }
        LONG Flight(volatile LONG& value) noexcept
        {
            return InterlockedCompareExchange(&value, 0, 0);
        }

        // OS callbacks may be faulted only at this boundary. An exception after
        // entry does not prove whether the kernel operation already committed.
        ClockResult SafeProtect(const C::Operations& operations, LPVOID address, SIZE_T bytes, DWORD desired, DWORD& old, BOOL& accepted) noexcept
        {
            __try
            {
                accepted = operations.protect(address, bytes, desired, &old);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
        ClockResult SafeWrite(const C::Operations& operations, LPVOID address, const void* bytes, SIZE_T size, SIZE_T& copied, BOOL& accepted) noexcept
        {
            __try
            {
                accepted = operations.write(GetCurrentProcess(), address, bytes, size, &copied);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
        ClockResult SafeFlush(const C::Operations& operations, LPCVOID address, SIZE_T size, BOOL& accepted) noexcept
        {
            __try
            {
                accepted = operations.flush(GetCurrentProcess(), address, size);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
        ClockResult SafeAdd(const C::Operations& operations, PRUNTIME_FUNCTION table, DWORD64 base, BOOLEAN& accepted) noexcept
        {
            __try
            {
                accepted = operations.addFunctions(table, 2, base);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
        ClockResult SafeDelete(const C::Operations& operations, PRUNTIME_FUNCTION table, BOOLEAN& accepted) noexcept
        {
            __try
            {
                accepted = operations.deleteFunctions(table);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
        ClockResult SafeFree(const C::Operations& operations, LPVOID allocation, BOOL& accepted) noexcept
        {
            __try
            {
                accepted = operations.free(allocation, 0, MEM_RELEASE);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
        std::uintptr_t Consumer(std::uintptr_t image) noexcept
        {
#if defined(MTA_DE_PLAYER_MOVEANIM_HOOK_CLOTHES_CONSUMER)
            return MTA_DE_PLAYER_MOVEANIM_HOOK_CLOTHES_CONSUMER(image);
#else
            return image + P::ClothesConsumerRva;
#endif
        }
        template <class Data>
        bool Profile(const Data& data) noexcept
        {
            if (!NativePedPoolReader::Revalidate({data.receipt.identity.image}) || data.receipt.uncertain)
                return false;
            const auto                                                          image = data.receipt.identity.image;
            std::array<unsigned char, NativePedRebuildProfile::MaximumPinBytes> observed{};
            for (const auto& pin : P::Code)
            {
                if (!pin.size || pin.size > observed.size() || !I::ReadImage(image, image + pin.rva, observed.data(), pin.size, false, true))
                    return false;
                if (pin.rva == P::ProcessContextRva && data.receipt.sites[0].installed)
                {
                    if (!NativePlayerMoveAnimHookOverlay::MatchesProcess(data.patches[0], image, observed.data(), pin.size))
                        return false;
                }
                else if (pin.rva == NativePedRebuildProfile::EntryRva && data.receipt.sites[1].installed)
                {
                    if (!NativePlayerMoveAnimHookOverlay::MatchesRebuild(data.patches[1], image, observed.data(), pin.size))
                        return false;
                }
                else if (std::memcmp(observed.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        template <class Data>
        void Unknown(Data& data, ClockResult result) noexcept
        {
            data.open.store(false);
            data.ready.store(false);
            data.recovery.store(true);
            data.receipt.uncertain = true;
            data.receipt.phase = C::Phase::Unknown;
            data.receipt.result = result;
        }
        template <class Data>
        ClockResult Finish(Data& data, ClockResult result, C::Receipt* output) noexcept
        {
            data.receipt.result = result;
            if (output)
            {
                *output = data.receipt;
                output->decisionsOpen = data.open.load();
                output->inFlight = static_cast<std::uint32_t>(Flight(data.inFlight));
            }
            return result;
        }
    }

    struct NativePlayerMoveAnimHookController::Data
    {
        struct Entry
        {
            Token        token{};
            Ped          ped{};
            std::int32_t custom = 0;
            bool         published = false;
        };
        const Operations                                        operations;
        Owners                                                  owners;
        Receipt                                                 receipt;
        std::array<NativePlayerMoveAnimHookOverlay::Patch, 2>   patches;
        std::array<RUNTIME_FUNCTION, 2>                         functions{};
        std::array<NativeFunctionRange, 4>                      exclusions{};
        std::array<Entry, NativePedPoolProfile::NativeCapacity> entries{};
        volatile LONG                                           inFlight = 0;
        std::atomic<bool>                                       retained{false}, recovery{false}, open{false}, ready{false};
        mutable std::atomic_flag                                operation = ATOMIC_FLAG_INIT;
        explicit Data(const Operations& selected) noexcept : operations(selected) {}
        std::uint32_t Decide(Kind kind, std::uintptr_t nativePed, std::uint32_t requested) noexcept
        {
            namespace D = HookControllerDetail;
            D::Operation lock(operation);
            if (!lock || !open.load() || !ready.load() || recovery.load() || !nativePed || !owners.decisionScope || !owners.resolveDecision ||
                !owners.qualifyDecisionGroup)
                return requested;
            const Entry* entry = nullptr;
            for (const auto& candidate : entries)
                if (candidate.published && candidate.ped.nativePed == nativePed)
                {
                    if (entry)
                        return requested;
                    entry = &candidate;
                }
            if (!entry || !entry->custom)
                return requested;
            // Unrelated NPCs and empty publications are a CPU-only native
            // pass-through. World/thread/source probes are needed only for an
            // actual controlled override, and still precede every native read.
            if (owners.decisionScope(kind, nativePed) != ClockResult::Ok)
                return requested;
            const auto snapshot = *entry;
            Ped        first, final;
            if (owners.resolveDecision(snapshot.token, first) != ClockResult::Ok || !D::Same(first, snapshot.ped) || !D::Profile(*this))
                return requested;
            std::int32_t incoming = 0;
            std::memcpy(&incoming, &requested, sizeof(incoming));
            const auto chosen = kind == Kind::ProcessCompare ? MtaGame::CPlayerMoveAnimPolicy::ChooseProcessAnimGroup(incoming, snapshot.custom)
                                                             : MtaGame::CPlayerMoveAnimPolicy::ChooseDefaultMotionGroup(incoming, snapshot.custom);
            if (chosen == incoming)
                return requested;
            // Native arrays can change after the setter's earlier validation.
            // A lost current hierarchy means native passthrough, never a stale
            // requested group or a fabricated successful custom selection.
            if (owners.qualifyDecisionGroup(chosen) != ClockResult::Ok || owners.resolveDecision(snapshot.token, final) != ClockResult::Ok ||
                !D::Same(first, final) || !D::Same(final, snapshot.ped) || owners.decisionScope(kind, nativePed) != ClockResult::Ok ||
                owners.qualifyDecisionGroup(chosen) != ClockResult::Ok || !D::Profile(*this) || !open.load() || recovery.load())
                return requested;
            std::uint32_t result = 0;
            std::memcpy(&result, &chosen, sizeof(result));
            return result;
        }
    };

    const NativePlayerMoveAnimHookController::Operations& NativePlayerMoveAnimHookController::DefaultOperations() noexcept
    {
        using namespace HookControllerDetail;
        static const Operations operations{&Query, &Allocate, &Protect, &Write, &Flush, &AddFunctions, &DeleteFunctions, &Free, &FindNativeFunctionRange};
        return operations;
    }
    NativePlayerMoveAnimHookController::NativePlayerMoveAnimHookController(const Operations& selected) noexcept : data(new (std::nothrow) Data(selected))
    {
    }
    NativePlayerMoveAnimHookController::~NativePlayerMoveAnimHookController()
    {
        Close();
        // RtlAddFunctionTable retains the CPU table address as well as code.
        // Quarantine both on abandonment; destructing unique_ptr alone would
        // leave the OS unwinder referencing freed metadata. The real Runtime
        // also fences module unload while this retained state exists.
        if (IsRetained())
            data.release();
    }
    std::uint32_t NativePlayerMoveAnimHookController::DecideProcess(std::uintptr_t ped, std::uint32_t requested, void* context) noexcept
    {
        return context ? static_cast<Data*>(context)->Decide(Kind::ProcessCompare, ped, requested) : requested;
    }
    std::uint32_t NativePlayerMoveAnimHookController::DecideClothes(std::uintptr_t ped, std::uint32_t requested, void* context) noexcept
    {
        return context ? static_cast<Data*>(context)->Decide(Kind::ClothesCall, ped, requested) : requested;
    }
    std::uint32_t NativePlayerMoveAnimHookController::Decide(Kind kind, std::uintptr_t ped, std::uint32_t requested) noexcept
    {
        return data ? data->Decide(kind, ped, requested) : requested;
    }

    ClockResult NativePlayerMoveAnimHookController::Prepare(const Identity& identity, const Owners& owners, Receipt* output) noexcept
    {
        namespace D = HookControllerDetail;
        namespace P = NativePlayerMoveAnimHookProfile;
        namespace B = NativePlayerMoveAnimHookBridge;
        if (!data)
            return ClockResult::NotInitialized;
        auto&        d = *data;
        D::Operation lock(d.operation);
        if (!lock)
            return ClockResult::Busy;
        const auto finish = [&](ClockResult result) { return D::Finish(d, result, output); };
        if (d.retained.load() || D::Flight(d.inFlight))
            return finish(ClockResult::Busy);
        if (sizeof(void*) != 8 || !D::Valid(d.operations) || !D::Valid(owners) || !identity.image || !identity.generation || !identity.admissionSerial ||
            !identity.engineThread || identity.engineThread != GetCurrentThreadId())
            return finish(ClockResult::InvalidArgument);
        auto result = owners.afterApplication();
        if (result != ClockResult::Ok)
            return finish(result);
        d.receipt = {};
        d.receipt.identity = identity;
        d.receipt.phase = Phase::Preparing;
        d.owners = owners;
        if (!D::Profile(d))
            return finish(ClockResult::BindingChanged);
        const std::array<std::uintptr_t, 3> roots{identity.image + P::ProcessContextRva, identity.image + NativePedRebuildProfile::EntryRva,
                                                  identity.image + P::ClothesConsumerRva};
        for (std::size_t index = 0; index != roots.size(); ++index)
            if (!d.operations.functionRange(reinterpret_cast<const void*>(roots[index]), d.exclusions[index + 1]))
                return finish(ClockResult::BindingChanged);
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        if (system.dwAllocationGranularity != D::AllocationBytes)
            return finish(ClockResult::UnsupportedBuild);
        const auto first = (identity.image + P::ProcessCompareRva) & ~std::uintptr_t{D::AllocationBytes - 1};
        const auto maximum = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
        for (std::size_t index = 0; index != D::MaximumAllocationCandidates; ++index)
        {
            const auto     distance = ((index + 1) / 2) * static_cast<std::uintptr_t>(D::AllocationBytes);
            std::uintptr_t candidate = 0;
            if (index & 1)
            {
                if (distance > first)
                    continue;
                candidate = first - distance;
            }
            else
            {
                if (distance > maximum - first)
                    continue;
                candidate = first + distance;
            }
            B::CallPatch process, clothes;
            if (!candidate || candidate > maximum - D::AllocationBytes ||
                !B::EncodeCallPatch(identity.image + P::ProcessCompareRva, candidate, Kind::ProcessCompare, process) ||
                !B::EncodeCallPatch(identity.image + P::ClothesCallRva, candidate + D::SecondStubOffset, Kind::ClothesCall, clothes))
                continue;
            MEMORY_BASIC_INFORMATION page{};
            if (!d.operations.query(reinterpret_cast<const void*>(candidate), &page, sizeof(page)) || page.State != MEM_FREE)
                continue;
            const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (candidate < base || candidate - base > page.RegionSize || D::AllocationBytes > page.RegionSize - (candidate - base))
                continue;
            const auto allocation = reinterpret_cast<std::uintptr_t>(
                d.operations.allocate(reinterpret_cast<void*>(candidate), D::AllocationBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            if (!allocation)
                continue;
            // Publish real resource ownership before every later fallible step.
            d.receipt.allocation = allocation;
            d.receipt.allocationBytes = D::AllocationBytes;
            d.receipt.allocationOwned = true;
            d.retained.store(true);
            if (allocation != candidate)
                return finish(ClockResult::WriteFailure);
            break;
        }
        if (!d.receipt.allocationOwned)
            return finish(ClockResult::WriteFailure);
        const auto               allocation = d.receipt.allocation;
        MEMORY_BASIC_INFORMATION owned{};
        if (!d.operations.query(reinterpret_cast<const void*>(allocation), &owned, sizeof(owned)) ||
            owned.AllocationBase != reinterpret_cast<void*>(allocation) || owned.State != MEM_COMMIT || owned.Type != MEM_PRIVATE ||
            owned.Protect != PAGE_READWRITE || owned.RegionSize < D::AllocationBytes)
            return finish(ClockResult::WriteFailure);
        d.exclusions[0] = {allocation, allocation + D::AllocationBytes};
        for (std::size_t index = 0; index != d.functions.size(); ++index)
        {
            const auto kind = index ? Kind::ClothesCall : Kind::ProcessCompare;
            const auto offset = index ? D::SecondStubOffset : 0u;
            B::Request request;
            request.kind = kind;
            request.decision = index ? &DecideClothes : &DecideProcess;
            request.context = &d;
            request.inFlight = &d.inFlight;
            request.originalConsumer = index ? D::Consumer(identity.image) : 0;
            B::Program   program;
            B::CallPatch patch;
            const auto   site = identity.image + (index ? P::ClothesCallRva : P::ProcessCompareRva);
            if (!B::Emit(request, program) || !B::EncodeCallPatch(site, allocation + offset, kind, patch) || program.size > B::MaximumBlobBytes)
                return finish(ClockResult::WriteFailure);
            std::memcpy(reinterpret_cast<void*>(allocation + offset), program.bytes.data(), program.size);
            d.functions[index] = program.function;
            d.functions[index].BeginAddress += offset;
            d.functions[index].EndAddress += offset;
            d.functions[index].UnwindData += offset;
            auto& entry = d.receipt.sites[index];
            entry.site = site;
            entry.stub = allocation + offset;
            entry.owned = patch.bytes;
            entry.size = patch.size;
            if (index)
                std::copy(P::ClothesCallBytes.begin(), P::ClothesCallBytes.end(), entry.stock.begin());
            else
                entry.stock = P::ProcessCompareBytes;
        }
        d.receipt.unwindEntered = true;
        BOOLEAN added = FALSE;
        result = D::SafeAdd(d.operations, d.functions.data(), allocation, added);
        if (result != ClockResult::Ok)
        {
            D::Unknown(d, result);
            return finish(result);
        }
        if (!added)
            return finish(ClockResult::WriteFailure);
        d.receipt.unwindRegistered = true;
        DWORD old = 0;
        BOOL  protectedRx = FALSE;
        result = D::SafeProtect(d.operations, reinterpret_cast<void*>(allocation), D::AllocationBytes, PAGE_EXECUTE_READ, old, protectedRx);
        if (result != ClockResult::Ok)
        {
            D::Unknown(d, result);
            return finish(result);
        }
        if (!protectedRx)
            return finish(ClockResult::WriteFailure);
        d.receipt.codeRx = true;
        BOOL flushed = FALSE;
        result = D::SafeFlush(d.operations, reinterpret_cast<const void*>(allocation), D::AllocationBytes, flushed);
        if (result != ClockResult::Ok)
        {
            D::Unknown(d, result);
            return finish(result);
        }
        if (!flushed)
            return finish(ClockResult::WriteFailure);
        d.receipt.codeFlushed = true;
        result = owners.afterApplication();
        if (result != ClockResult::Ok || !D::Profile(d))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        d.receipt.phase = Phase::Prepared;
        return finish(ClockResult::Ok);
    }

    ClockResult NativePlayerMoveAnimHookController::Install(CheckOwner parkedOwner, Receipt* output) noexcept
    {
        namespace D = HookControllerDetail;
        if (!data)
            return ClockResult::NotInitialized;
        auto&        d = *data;
        D::Operation lock(d.operation);
        if (!lock)
            return ClockResult::Busy;
        const auto finish = [&](ClockResult result) { return D::Finish(d, result, output); };
        if (!parkedOwner || d.receipt.phase != Phase::Prepared || d.receipt.uncertain || !d.receipt.unwindRegistered || !d.receipt.codeRx ||
            !d.receipt.codeFlushed || D::Flight(d.inFlight))
            return finish(ClockResult::NotInitialized);
        auto result = parkedOwner();
        if (result != ClockResult::Ok || !D::Profile(d))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        d.receipt.phase = Phase::Installing;
        for (std::size_t index = 0; index != d.receipt.sites.size(); ++index)
        {
            auto&                        site = d.receipt.sites[index];
            std::array<unsigned char, 6> before{};
            if (!NativeImageAccess::ReadImage(d.receipt.identity.image, site.site, before.data(), site.size, false, true) ||
                std::memcmp(before.data(), site.stock.data(), site.size))
            {
                // Once an earlier source write entered, foreign bytes destroy
                // the multi-site acknowledgement. A later external rewrite is
                // not evidence that our partially installed prefix can retire.
                const bool prefixEntered =
                    std::any_of(d.receipt.sites.begin(), d.receipt.sites.end(), [](const SiteReceipt& prior) { return prior.writeEntered || prior.installed; });
                if (prefixEntered)
                    D::Unknown(d, ClockResult::BindingChanged);
                return finish(ClockResult::BindingChanged);
            }
            BOOL accepted = FALSE;
            site.protectionEntered = true;
            result = D::SafeProtect(d.operations, reinterpret_cast<void*>(site.site), site.size, PAGE_EXECUTE_READWRITE, site.originalProtection, accepted);
            if (result != ClockResult::Ok)
            {
                D::Unknown(d, result);
                return finish(result);
            }
            if (!accepted)
                return finish(ClockResult::PatchFailure);
            site.protectionChanged = true;
            SIZE_T copied = 0;
            accepted = FALSE;
            site.writeEntered = true;
            result = D::SafeWrite(d.operations, reinterpret_cast<void*>(site.site), site.owned.data(), site.size, copied, accepted);
            site.writeReturned = result == ClockResult::Ok;
            if (result != ClockResult::Ok || !accepted || copied != site.size)
            {
                D::Unknown(d, result == ClockResult::Ok ? ClockResult::PatchFailure : result);
                return finish(d.receipt.result);
            }
            accepted = FALSE;
            result = D::SafeFlush(d.operations, reinterpret_cast<const void*>(site.site), site.size, accepted);
            if (result != ClockResult::Ok || !accepted)
            {
                D::Unknown(d, result == ClockResult::Ok ? ClockResult::PatchFailure : result);
                return finish(d.receipt.result);
            }
            site.flushAcknowledged = true;
            DWORD ignored = 0;
            accepted = FALSE;
            result = D::SafeProtect(d.operations, reinterpret_cast<void*>(site.site), site.size, site.originalProtection, ignored, accepted);
            if (result != ClockResult::Ok || !accepted)
            {
                D::Unknown(d, result == ClockResult::Ok ? ClockResult::RestoreFailure : result);
                return finish(d.receipt.result);
            }
            site.protectionRestored = true;
            std::array<unsigned char, 6> current{};
            if (!NativeImageAccess::ReadImage(d.receipt.identity.image, site.site, current.data(), site.size, false, true) ||
                std::memcmp(current.data(), site.owned.data(), site.size))
            {
                D::Unknown(d, ClockResult::BindingChanged);
                return finish(d.receipt.result);
            }
            site.verified = site.installed = true;
            auto& proof = d.patches[index];
            proof.image = d.receipt.identity.image;
            proof.site = site.site;
            proof.stub = site.stub;
            proof.generation = d.receipt.identity.generation;
            proof.admissionSerial = d.receipt.identity.admissionSerial;
            proof.kind = index ? Kind::ClothesCall : Kind::ProcessCompare;
            proof.bytes = site.owned;
            proof.size = site.size;
            proof.installedVerified = true;
        }
        result = parkedOwner();
        if (result != ClockResult::Ok || !D::Profile(d))
        {
            D::Unknown(d, result == ClockResult::Ok ? ClockResult::BindingChanged : result);
            return finish(d.receipt.result);
        }
        d.receipt.phase = Phase::Live;
        d.ready.store(true);
        d.open.store(true);
        return finish(ClockResult::Ok);
    }

    ClockResult NativePlayerMoveAnimHookController::Detach(CheckOwner parkedOwner, Receipt* output) noexcept
    {
        namespace D = HookControllerDetail;
        Close();
        if (!data)
            return ClockResult::NotInitialized;
        auto&        d = *data;
        D::Operation lock(d.operation);
        if (!lock)
            return ClockResult::Busy;
        const auto finish = [&](ClockResult result) { return D::Finish(d, result, output); };
        if (!parkedOwner || d.receipt.uncertain || D::Flight(d.inFlight))
            return finish(ClockResult::Busy);
        auto result = parkedOwner();
        if (result != ClockResult::Ok)
            return finish(result);
        // Foreign bytes invalidate the acknowledged installed source itself.
        // Retain that first uncertainty instead of later guessing whether an
        // external rewrite made it safe to restore our former native prefix.
        if (!D::Profile(d))
        {
            D::Unknown(d, ClockResult::BindingChanged);
            return finish(d.receipt.result);
        }
        d.receipt.phase = Phase::Closing;
        for (std::size_t remaining = d.receipt.sites.size(); remaining; --remaining)
        {
            auto& site = d.receipt.sites[remaining - 1];
            if (!site.installed)
                continue;
            std::array<unsigned char, 6> current{};
            if (!NativeImageAccess::ReadImage(d.receipt.identity.image, site.site, current.data(), site.size, false, true) ||
                std::memcmp(current.data(), site.owned.data(), site.size))
            {
                D::Unknown(d, ClockResult::BindingChanged);
                return finish(d.receipt.result);
            }
            DWORD old = 0;
            BOOL  accepted = FALSE;
            result = D::SafeProtect(d.operations, reinterpret_cast<void*>(site.site), site.size, PAGE_EXECUTE_READWRITE, old, accepted);
            if (result != ClockResult::Ok || !accepted)
            {
                D::Unknown(d, result == ClockResult::Ok ? ClockResult::RestoreFailure : result);
                return finish(d.receipt.result);
            }
            SIZE_T copied = 0;
            accepted = FALSE;
            result = D::SafeWrite(d.operations, reinterpret_cast<void*>(site.site), site.stock.data(), site.size, copied, accepted);
            if (result != ClockResult::Ok || !accepted || copied != site.size)
            {
                D::Unknown(d, result == ClockResult::Ok ? ClockResult::RestoreFailure : result);
                return finish(d.receipt.result);
            }
            accepted = FALSE;
            result = D::SafeFlush(d.operations, reinterpret_cast<const void*>(site.site), site.size, accepted);
            if (result != ClockResult::Ok || !accepted)
            {
                D::Unknown(d, result == ClockResult::Ok ? ClockResult::RestoreFailure : result);
                return finish(d.receipt.result);
            }
            DWORD ignored = 0;
            accepted = FALSE;
            result = D::SafeProtect(d.operations, reinterpret_cast<void*>(site.site), site.size, old, ignored, accepted);
            if (result != ClockResult::Ok || !accepted)
            {
                D::Unknown(d, result == ClockResult::Ok ? ClockResult::RestoreFailure : result);
                return finish(d.receipt.result);
            }
            if (!NativeImageAccess::ReadImage(d.receipt.identity.image, site.site, current.data(), site.size, false, true) ||
                std::memcmp(current.data(), site.stock.data(), site.size))
            {
                D::Unknown(d, ClockResult::BindingChanged);
                return finish(d.receipt.result);
            }
            site.installed = false;
            d.patches[remaining - 1].Invalidate();
        }
        result = parkedOwner();
        if (result != ClockResult::Ok || !D::Profile(d))
        {
            D::Unknown(d, result == ClockResult::Ok ? ClockResult::BindingChanged : result);
            return finish(d.receipt.result);
        }
        d.receipt.phase = Phase::Detached;
        return finish(ClockResult::Ok);
    }

    ClockResult NativePlayerMoveAnimHookController::Retire(CheckOwner quiescentProducer, Receipt* output) noexcept
    {
        namespace D = HookControllerDetail;
        Close();
        if (!data)
            return ClockResult::NotInitialized;
        auto&        d = *data;
        D::Operation lock(d.operation);
        if (!lock)
            return ClockResult::Busy;
        const auto finish = [&](ClockResult result) { return D::Finish(d, result, output); };
        if (!quiescentProducer || d.receipt.uncertain || D::Flight(d.inFlight) || d.receipt.sites[0].installed || d.receipt.sites[1].installed)
            return finish(ClockResult::Busy);
        auto result = quiescentProducer();
        if (result != ClockResult::Ok)
            return finish(result);
        if (d.receipt.unwindRegistered && !d.receipt.unwindDeleted)
        {
            d.receipt.unwindDeleteEntered = true;
            BOOLEAN deleted = FALSE;
            result = D::SafeDelete(d.operations, d.functions.data(), deleted);
            if (result != ClockResult::Ok || !deleted)
            {
                D::Unknown(d, result == ClockResult::Ok ? ClockResult::RestoreFailure : result);
                return finish(d.receipt.result);
            }
            d.receipt.unwindDeleted = true;
            d.receipt.unwindRegistered = false;
        }
        if (d.receipt.allocationOwned && !d.receipt.freed)
        {
            d.receipt.freeEntered = true;
            BOOL freed = FALSE;
            result = D::SafeFree(d.operations, reinterpret_cast<void*>(d.receipt.allocation), freed);
            if (result != ClockResult::Ok)
            {
                D::Unknown(d, result);
                return finish(result);
            }
            if (!freed)
                return finish(ClockResult::RestoreFailure);
            d.receipt.freed = true;
            d.receipt.allocationOwned = false;
        }
        for (auto& entry : d.entries)
            entry = {};
        d.receipt.publishedOverrides = 0;
        d.receipt.phase = Phase::Retired;
        d.retained.store(false);
        d.ready.store(false);
        return finish(ClockResult::Ok);
    }
    void NativePlayerMoveAnimHookController::Close() noexcept
    {
        if (data)
        {
            data->open.store(false);
            data->ready.store(false);
        }
    }
    bool NativePlayerMoveAnimHookController::Ready() const noexcept
    {
        return data && data->open.load() && data->ready.load() && !data->recovery.load();
    }
    bool NativePlayerMoveAnimHookController::IsRetained() const noexcept
    {
        return data && (data->retained.load() || HookControllerDetail::Flight(data->inFlight));
    }
    bool NativePlayerMoveAnimHookController::RecoveryPending() const noexcept
    {
        return data && data->recovery.load();
    }
    bool NativePlayerMoveAnimHookController::Read(Receipt& output) const noexcept
    {
        if (!data)
            return false;
        HookControllerDetail::Operation lock(data->operation);
        if (!lock)
            return false;
        output = data->receipt;
        output.decisionsOpen = data->open.load();
        output.inFlight = static_cast<std::uint32_t>(HookControllerDetail::Flight(data->inFlight));
        return true;
    }
    bool NativePlayerMoveAnimHookController::Exclusions(std::array<NativeFunctionRange, 4>& output) const noexcept
    {
        if (!data)
            return false;
        HookControllerDetail::Operation lock(data->operation);
        if (!lock || !data->receipt.allocationOwned)
            return false;
        output = data->exclusions;
        return true;
    }
    bool NativePlayerMoveAnimHookController::MatchesProcess(std::uintptr_t image, const unsigned char* span, std::size_t size) const noexcept
    {
        if (!data || !data->owners.afterApplication || data->owners.afterApplication() != ClockResult::Ok)
            return false;
        HookControllerDetail::Operation lock(data->operation);
        return lock && !data->receipt.uncertain && image == data->receipt.identity.image && data->receipt.sites[0].installed &&
               NativePlayerMoveAnimHookOverlay::MatchesProcess(data->patches[0], image, span, size) && data->owners.afterApplication() == ClockResult::Ok;
    }
    bool NativePlayerMoveAnimHookController::MatchesRebuild(std::uintptr_t image, const unsigned char* span, std::size_t size) const noexcept
    {
        if (!data || !data->owners.afterApplication || data->owners.afterApplication() != ClockResult::Ok)
            return false;
        HookControllerDetail::Operation lock(data->operation);
        return lock && !data->receipt.uncertain && image == data->receipt.identity.image && data->receipt.sites[1].installed &&
               NativePlayerMoveAnimHookOverlay::MatchesRebuild(data->patches[1], image, span, size) && data->owners.afterApplication() == ClockResult::Ok;
    }
    ClockResult NativePlayerMoveAnimHookController::Publish(const Token& token, const Ped& supplied, std::int32_t custom) noexcept
    {
        namespace D = HookControllerDetail;
        if (!data)
            return ClockResult::NotInitialized;
        auto&        d = *data;
        D::Operation lock(d.operation);
        if (!lock)
            return ClockResult::Busy;
        if (!Ready() || token.slot >= d.entries.size() || !token.version || token.ownerGeneration != d.receipt.identity.generation)
            return ClockResult::NotInitialized;
        auto result = d.owners.afterApplication();
        if (result != ClockResult::Ok)
            return result;
        Ped first, final;
        if ((result = d.owners.resolvePublication(token, first)) != ClockResult::Ok || !D::Same(first, supplied))
            return result == ClockResult::Ok ? ClockResult::OwnerConflict : result;
        if (custom && (result = d.owners.qualifyDecisionGroup(custom)) != ClockResult::Ok)
            return result;
        if ((result = d.owners.resolvePublication(token, final)) != ClockResult::Ok || !D::Same(first, final) ||
            (result = d.owners.afterApplication()) != ClockResult::Ok)
            return result == ClockResult::Ok ? ClockResult::OwnerConflict : result;
        auto& entry = d.entries[token.slot];
        if (entry.published && !D::Same(entry.token, token))
            return ClockResult::Busy;
        if (!entry.published)
            ++d.receipt.publishedOverrides;
        entry.token = token;
        entry.ped = final;
        entry.custom = custom;
        entry.published = true;
        return ClockResult::Ok;
    }
    ClockResult NativePlayerMoveAnimHookController::Withdraw(const Token& token) noexcept
    {
        if (!data)
            return ClockResult::NotInitialized;
        HookControllerDetail::Operation lock(data->operation);
        if (!lock)
            return ClockResult::Busy;
        if (token.slot >= data->entries.size())
            return ClockResult::InvalidArgument;
        auto& entry = data->entries[token.slot];
        if (!entry.published)
            return ClockResult::Ok;
        if (!HookControllerDetail::Same(entry.token, token))
            return ClockResult::OwnerConflict;
        entry = {};
        --data->receipt.publishedOverrides;
        return ClockResult::Ok;
    }
    ClockResult NativePlayerMoveAnimHookController::WithdrawAll() noexcept
    {
        if (!data)
            return ClockResult::NotInitialized;
        HookControllerDetail::Operation lock(data->operation);
        if (!lock || HookControllerDetail::Flight(data->inFlight))
            return ClockResult::Busy;
        for (auto& entry : data->entries)
            entry = {};
        data->receipt.publishedOverrides = 0;
        return ClockResult::Ok;
    }
    bool NativePlayerMoveAnimHookController::HasOverride(const Token& token) const noexcept
    {
        if (!data)
            return false;
        HookControllerDetail::Operation lock(data->operation);
        return lock && token.slot < data->entries.size() && data->entries[token.slot].published &&
               HookControllerDetail::Same(data->entries[token.slot].token, token);
    }
}
