/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current issued custom-sequence operations through one native owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceName.h"
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
        // The same owner admits both issued rows and preserves accepted raw
        // copies across lost acknowledgement. No wrapper adopts their frames.
        virtual ClockResult CopyAnimationSequenceProperties(const NativeAnimationSequenceReferenceToken&, std::uintptr_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Full host-width index and original DWORD stride are owner inputs.
        // The owner checks actual frame bytes, never an inferred WORD count.
        virtual ClockResult ReadAnimationSequenceFrame(const NativeAnimationSequenceReferenceToken&, std::size_t, std::uint32_t, std::uintptr_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // The SDK copies the bounded raw spelling before owner callbacks.
        // Hash/selector projection and accepted native effects stay with the
        // same allocation owner, never a facade-local name cache.
        virtual ClockResult SetAnimationSequenceName(const NativeAnimationSequenceReferenceToken&, const NativeAnimationSequenceName&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
