/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain the real allocation behind a constrained interior accumulator
 *****************************************************************************/
#pragma once
#include "NativeWeatherOverridePlacement.h"

namespace mta::game_de
{
    class NativeWeatherOverrideSlot final
    {
    public:
        NativeWeatherOverrideSlot() = default;
        NativeWeatherOverrideSlot(const NativeWeatherOverrideSlot&) = delete;
        NativeWeatherOverrideSlot& operator=(const NativeWeatherOverrideSlot&) = delete;
        // May return failure with HasAllocation true when allocation succeeded
        // but subsequent validation failed. The caller retains this same slot
        // and native module until explicit Release succeeds. Its destructor
        // never assumes that native instruction references have disappeared.
        static ClockResult Allocate(const NativeWeatherOverridePlacementProfile::Placement&, NativeWeatherOverrideSlot&) noexcept;
        bool               HasAllocation() const noexcept { return allocation != 0; }
        bool               Valid() const noexcept;
        bool               Release() noexcept;
        std::uintptr_t     AllocationIdentity() const noexcept { return allocation; }
        std::uintptr_t     Accumulator() const noexcept { return accumulator; }

    private:
        std::uintptr_t allocation = 0, accumulator = 0;
    };
}
