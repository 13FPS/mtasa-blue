/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-ordered MTA removal policy for an already empty current block
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockAccess.h"
#include <atomic>
#include <memory>

namespace mta::game_de
{
    namespace NativeAnimationBlockRemovalCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        using GroupFunction = void (*)(std::uintptr_t);
        using InvokeGroup = ClockResult (*)(GroupFunction, std::uintptr_t, std::uint32_t&) noexcept;
        struct Store
        {
            std::uintptr_t address = 0;
            std::uint8_t   bytes = 0;
        };
        using Write = ClockResult (*)(const Store*, std::size_t, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uint64_t generation = 0;
            std::int32_t  index = -1, blockCount = 0, groupCount = 0;
            std::uint32_t matchingGroups = 0, groupCalls = 0, groupReturns = 0, stores = 0, exceptionCode = 0;
            std::uint16_t beforeRefs = 0;
            std::uint8_t  beforeLoaded = 0;
            bool          entered = false, writeEntered = false, returned = false, effectsVerified = false, debt = false;
            ClockResult   result = ClockResult::NotInitialized;
        };
    }
    namespace AnimationBlockRemovalDetail
    {
        ClockResult InvokeOriginal(NativeAnimationBlockRemovalCalls::GroupFunction, std::uintptr_t, std::uint32_t&) noexcept;
        ClockResult WriteStores(const NativeAnimationBlockRemovalCalls::Store*, std::size_t, std::uint32_t&) noexcept;
    }
    class NativeAnimationBlockRemoval final
    {
    public:
        using CheckOwner = NativeAnimationBlockRemovalCalls::CheckOwner;
        using Receipt = NativeAnimationBlockRemovalCalls::Receipt;
        using InvokeGroup = NativeAnimationBlockRemovalCalls::InvokeGroup;
        using Write = NativeAnimationBlockRemovalCalls::Write;
        NativeAnimationBlockRemoval() noexcept;
        ~NativeAnimationBlockRemoval();
        NativeAnimationBlockRemoval(const NativeAnimationBlockRemoval&) = delete;
        NativeAnimationBlockRemoval& operator=(const NativeAnimationBlockRemoval&) = delete;
        // This domain refuses every nonempty block and every matching group
        // that owns children. It grants no general teardown or deregistration.
        ClockResult RemoveEmpty(std::uintptr_t, std::uint64_t, const NativeAnimationBlockIdentity&, CheckOwner, CheckOwner postFrameOwner, Receipt* = nullptr,
                                InvokeGroup = nullptr, Write = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool        RecoveryPending() const noexcept { return debt.load(); }

    private:
        // Even an entered null-child call with lost acknowledgement retains
        // its exact CPU journal. No retry, repair or teardown is inferred.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
