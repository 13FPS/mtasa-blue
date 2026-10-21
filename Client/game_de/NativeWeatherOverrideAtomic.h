/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only admit native operand changes contained in one aligned CAS
 *****************************************************************************/
#pragma once
#include "NativeWeatherOverridePlan.h"

namespace mta::game_de::NativeWeatherOverrideAtomicProfile
{
    using Field = NativeWeatherOverrideProfile::Field;
    using Entry = NativeWeatherOverrideProfile::Plan::Entry;
    struct Window
    {
        Field                         field;
        std::uintptr_t                writerRva, windowRva;
        std::size_t                   writerSize, windowSize;
        std::array<unsigned char, 16> stock;
        // A parked thread may be in instruction bytes preceding the changed
        // window. Reject the union, not merely the displacement's four bytes.
        std::uintptr_t ExclusionBegin() const noexcept { return writerRva < windowRva ? writerRva : windowRva; }
        std::uintptr_t ExclusionEnd() const noexcept
        {
            const auto writerEnd = writerRva + writerSize, windowEnd = windowRva + windowSize;
            return writerEnd > windowEnd ? writerEnd : windowEnd;
        }
    };
    extern const std::array<Window, 37> Windows;
    // The retained argument keeps callers source-compatible; congruent slots
    // make every admitted group independent of optional128-bit CPU support.
    bool          Supports(Field, bool compareExchange128) noexcept;
    const Window* Find(std::uintptr_t writerRva) noexcept;
    // Neighbor bytes are pinned as carefully as the instruction. The atomic
    // compare must preserve them, including neighboring native writer bytes.
    bool Build(const Window&, const Entry&, std::array<unsigned char, 16>& redirected) noexcept;
}
