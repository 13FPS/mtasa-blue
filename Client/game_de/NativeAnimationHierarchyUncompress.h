/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One original bank-hierarchy time conversion with retained unknowns
 ****************************************************************************/
#pragma once
#include "NativeAnimationHierarchyAccess.h"
#include <atomic>
#include <memory>
namespace mta::game_de
{
    namespace NativeAnimationHierarchyUncompressCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        using Function = void (*)(std::uintptr_t);
        using Invoke = ClockResult (*)(Function, std::uintptr_t, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t hierarchy = 0, entryRva = 0xff0d10;
            std::uint64_t  generation = 0;
            std::uint32_t  exceptionCode = 0, beforeTimeBits = 0, afterTimeBits = 0;
            std::uint32_t  sequences = 0, preparedFrames = 0, modifiedFrames = 0, cacheNodes = 0, modifiedCacheWords = 0;
            bool           entered = false, returned = false, effectsVerified = false, noOp = false, debt = false, cachePath = false;
            ClockResult    result = ClockResult::NotInitialized;
        };
    }
    class NativeAnimationHierarchyUncompress final
    {
    public:
        using CheckOwner = NativeAnimationHierarchyUncompressCalls::CheckOwner;
        using Receipt = NativeAnimationHierarchyUncompressCalls::Receipt;
        using Invoke = NativeAnimationHierarchyUncompressCalls::Invoke;
        NativeAnimationHierarchyUncompress() noexcept;
        ~NativeAnimationHierarchyUncompress();
        NativeAnimationHierarchyUncompress(const NativeAnimationHierarchyUncompress&) = delete;
        NativeAnimationHierarchyUncompress& operator=(const NativeAnimationHierarchyUncompress&) = delete;
        ClockResult Uncompress(std::uintptr_t, const NativeAnimationHierarchyReferenceToken&, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool        RecoveryPending() const noexcept { return debt.load(); }

    private:
        // The prepared current bank/frame journal survives an entered call
        // whose effects cannot be verified. Destruction performs no rollback,
        // native free, retry or guessed restoration of compressed timestamps.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
