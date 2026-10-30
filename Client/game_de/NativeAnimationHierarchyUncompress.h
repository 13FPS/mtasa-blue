/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Original bank animation operations with retained unknown effects
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
        enum class Operation : std::uint8_t
        {
            Uncompress,
            QuaternionFlips,
            TotalTime
        };
        struct Receipt
        {
            std::uintptr_t hierarchy = 0, entryRva = 0xff0d10, outputBase = 0;
            std::uint64_t  generation = 0;
            std::uint32_t  exceptionCode = 0, beforeTimeBits = 0, afterTimeBits = 0;
            std::uint32_t  sequences = 0, preparedFrames = 0, modifiedFrames = 0, cacheNodes = 0, modifiedCacheWords = 0;
            // Output addresses are observations of the original's publication,
            // never private allocation tickets or adapter cleanup authority.
            // Byte/store counts describe the prepared plan; convertedFrames
            // and outputBase publish only after the complete effects verify.
            std::uint32_t convertedFrames = 0, encodedBytes = 0, outputBytes = 0, cacheStores = 0;
            bool          entered = false, returned = false, effectsVerified = false, noOp = false, debt = false, cachePath = false;
            bool          decompressionPath = false, sharedInput = false;
            // The victim is independently qualified from its current bank;
            // cache node.data is only a candidate locator, never free authority.
            // Its output base/compressed count publish only after both banks
            // and their shared cache/heap/completion journal verify together.
            std::uintptr_t victimHierarchy = 0, victimOutputBase = 0;
            std::uint32_t  victimSequences = 0, victimPreparedFrames = 0, victimCompressedFrames = 0, victimInputBytes = 0, victimOutputBytes = 0;
            bool           evictionPath = false, victimSharedInput = false;
            // The same existing MTA hierarchy operations also traverse current
            // bank float payloads; this is not private allocation ownership.
            Operation   operation = Operation::Uncompress;
            bool        postprocessPath = false;
            ClockResult result = ClockResult::NotInitialized;
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
        // Only destructive bank conversion requires the final post-original
        // frame owner. Existing time/LRU/no-effect branches keep their owner.
        ClockResult Uncompress(std::uintptr_t, const NativeAnimationHierarchyReferenceToken&, CheckOwner, Receipt* = nullptr, Invoke = nullptr,
                               CheckOwner afterApplicationFrameOwner = nullptr) noexcept;
        ClockResult PostProcess(std::uintptr_t, const NativeAnimationHierarchyReferenceToken&, NativeAnimationHierarchyUncompressCalls::Operation, CheckOwner,
                                Receipt* = nullptr, Invoke = nullptr, CheckOwner afterApplicationFrameOwner = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool        RecoveryPending() const noexcept { return debt.load(); }

    private:
        // The prepared current bank/frame journal survives an entered call
        // whose effects cannot be verified. Destruction performs no rollback,
        // native free, retry or guessed restoration of timestamps or buffers.
        ClockResult Process(std::uintptr_t, const NativeAnimationHierarchyReferenceToken&, NativeAnimationHierarchyUncompressCalls::Operation, CheckOwner,
                            Receipt*, Invoke, CheckOwner) noexcept;
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
