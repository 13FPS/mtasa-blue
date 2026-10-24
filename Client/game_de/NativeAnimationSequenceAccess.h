/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current issued custom-sequence operations through one native owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstddef>
#include <cstdint>

namespace mta::game_de
{
    struct NativeAnimationSequenceReferenceToken
    {
        std::uint64_t  ownerGeneration = 0;
        std::uintptr_t firstTag = 0;
        std::size_t    index = 0;
    };

    struct NativeAnimationSequenceValues
    {
        std::uintptr_t interfaceAddress = 0, keyFrames = 0;
        std::uint16_t  frameCount = 0, boneTag = 0;
        bool           bigChunk = false;
    };

    class NativeAnimationSequenceAccess
    {
    public:
        virtual ~NativeAnimationSequenceAccess() = default;
        // Tokens identify a currently issued, unfreed array row. They add no
        // serial capability or stale-address ABA protection to a raw tag.
        // The owner keeps logical union state with that row, not its facade.
        virtual ClockResult InitializeAnimationSequence(const NativeAnimationSequenceReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetAnimationSequenceBoneTag(const NativeAnimationSequenceReferenceToken&, std::int32_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ReadAnimationSequence(const NativeAnimationSequenceReferenceToken&, NativeAnimationSequenceValues&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Undefined high union bits are an owner refusal, never a synthetic
        // hash derived from the DE native name or initialized to zero.
        virtual ClockResult ReadAnimationSequenceHash(const NativeAnimationSequenceReferenceToken&, std::uint32_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
