/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current bank hierarchy reads through one borrowed native owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>

namespace mta::game_de
{
    struct NativeAnimationHierarchyReferenceToken
    {
        std::uint64_t  ownerGeneration = 0;
        std::uintptr_t locator = 0;
    };
    struct NativeAnimationHierarchyValues
    {
        std::uintptr_t interfaceAddress = 0;
        std::uint32_t  hash = 0, totalTimeBits = 0;
        std::uint16_t  sequenceCount = 0;
        std::uint8_t   runningCompressed = 0;
        std::int32_t   blockId = 0;
        // Bank hashes are defined. Fresh private storage explicitly reports
        // unknown until a supported name operation defines its original bits.
        bool hashDefined = true;
    };
    class NativeAnimationHierarchyAccess
    {
    public:
        virtual ~NativeAnimationHierarchyAccess() = default;
        // The token is immediate callback-generation affinity, not a lease.
        // Scalars do not acquire or validate a dynamic child array.
        virtual ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken&, NativeAnimationHierarchyValues&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Only nested borrows admit the current pointer and full native
        // capacity cookie. Published WORD count and capacity remain distinct.
        virtual ClockResult ReadAnimationHierarchySequences(const NativeAnimationHierarchyReferenceToken&, std::uintptr_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ReadAnimationHierarchySequence(const NativeAnimationHierarchyReferenceToken&, std::uint32_t, std::uintptr_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // These private CPU40 operations do not expose a general MTA factory.
        // Only the same current owner may create, initialize or retire its
        // neverpublished empty/cache-free records; children remain separate.
        virtual ClockResult CreatePrivateAnimationHierarchy(std::uint64_t, NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult InitializePrivateAnimationHierarchy(const NativeAnimationHierarchyReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult DestroyPrivateAnimationHierarchy(const NativeAnimationHierarchyReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        // Genuine MTA scalar setters remain restricted to issued private,
        // cache-free records. Linked children retain their own qualified holds.
        virtual ClockResult SetPrivateAnimationHierarchyRunningCompressed(const NativeAnimationHierarchyReferenceToken&, bool) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPrivateAnimationHierarchyBlockID(const NativeAnimationHierarchyReferenceToken&, std::int32_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Hierarchy SetName stores only its already-computed source key.
        // Names are not retained, truncated or interned by the native owner.
        virtual ClockResult SetPrivateAnimationHierarchyNameHash(const NativeAnimationHierarchyReferenceToken&, std::uint32_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Preserve the source WORD separately from capacity. Array changes
        // require the owner's prepared-child/acknowledged-detach protocol;
        // a null tag is an explicit request, never generic unpin permission.
        virtual ClockResult SetPrivateAnimationHierarchySequenceCount(const NativeAnimationHierarchyReferenceToken&, std::uint16_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPrivateAnimationHierarchySequences(const NativeAnimationHierarchyReferenceToken&, std::uintptr_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // These remain owner-qualified source operations: cache0 is the
        // exact guarded no-op, while removal permits only a known no-child
        // row and clears pointer/count/time without detaching or freeing.
        virtual ClockResult RemovePrivateAnimationHierarchyFromUncompressedCache(const NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult RemoveEmptyPrivateAnimationHierarchySequences(const NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Genuine payload operations require the owner's attached-command
        // holds and accepted uncompressed row/frame authority. Neither a
        // compressed bool nor readable bytes supplies that admission.
        virtual ClockResult RemovePrivateAnimationHierarchyQuaternionFlips(const NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult CalculatePrivateAnimationHierarchyTotalTime(const NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Current-bank products are immediate observations only. The same
        // existing row/block qualifier must resolve them; private custom
        // records cannot enter this factory through a readable raw locator.
        virtual ClockResult ResolveBankAnimationHierarchyReference(std::uint64_t, std::uintptr_t, NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ResolveBankAnimationHierarchyAt(std::uint64_t, std::uint32_t, NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Custom factories must resolve an actual issued private record,
        // never adopt an ordinary native-bank locator through generic Read.
        virtual ClockResult ResolvePrivateAnimationHierarchyReference(std::uint64_t, std::uintptr_t, NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
