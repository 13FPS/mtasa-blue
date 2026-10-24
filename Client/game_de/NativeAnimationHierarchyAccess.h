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
    };
}
