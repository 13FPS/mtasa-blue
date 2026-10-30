/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One source-qualified hierarchy cache unlink with retained unknowns
 ****************************************************************************/
#pragma once
#include "NativeAnimationHierarchyAccess.h"
#include <atomic>
#include <cstddef>
#include <memory>

namespace mta::game_de
{
    namespace NativeAnimationHierarchyCacheRemoveCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        struct Store
        {
            std::uintptr_t address = 0, value = 0;
        };
        using Write = ClockResult (*)(const Store*, std::size_t, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uint64_t  generation = 0;
            std::uintptr_t hierarchy = 0;
            std::uint32_t  cacheNodes = 0, exceptionCode = 0;
            bool           entered = false, returned = false, effectsVerified = false, noOp = false, debt = false;
            ClockResult    result = ClockResult::NotInitialized;
        };
    }
    class NativeAnimationHierarchyCacheRemove final
    {
    public:
        using CheckOwner = NativeAnimationHierarchyCacheRemoveCalls::CheckOwner;
        using Receipt = NativeAnimationHierarchyCacheRemoveCalls::Receipt;
        using Write = NativeAnimationHierarchyCacheRemoveCalls::Write;
        NativeAnimationHierarchyCacheRemove() noexcept;
        ~NativeAnimationHierarchyCacheRemove();
        NativeAnimationHierarchyCacheRemove(const NativeAnimationHierarchyCacheRemove&) = delete;
        NativeAnimationHierarchyCacheRemove& operator=(const NativeAnimationHierarchyCacheRemove&) = delete;
        ClockResult Remove(std::uintptr_t image, const NativeAnimationHierarchyReferenceToken&, CheckOwner, Receipt* = nullptr, Write = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load(std::memory_order_acquire) || debt.load(std::memory_order_acquire); }
        bool        RecoveryPending() const noexcept { return debt.load(std::memory_order_acquire); }

    private:
        // An interrupted ordered splice owns its prepared old/new graph and
        // module fence. Destruction never guesses which stores took effect.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
