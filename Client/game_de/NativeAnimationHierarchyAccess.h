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
        // empty/cache-free records. They do not attach children or define names.
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
    };
}
