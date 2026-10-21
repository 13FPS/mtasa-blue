/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Internal CPU identity for retained native ped reference cells
 ****************************************************************************/
#pragma once
#include <game/CPoolSlotPolicy.h>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace mta::game_de
{
    struct NativePedReferenceToken final
    {
        std::uint64_t version = 0, ownerGeneration = 0;
        std::uint32_t slot = MtaGame::CPoolSlotPolicy::InvalidSlot;

        // A CPU slot version never wraps into a previous token. Exhaustion
        // retires that slot instead of borrowing the native seven-bit handle
        // generation as a substitute for the controller's own identity.
        static constexpr std::uint64_t NextVersion(std::uint64_t current) noexcept
        {
            return current == std::numeric_limits<std::uint64_t>::max() ? 0 : current + 1;
        }
    };
    static_assert(std::is_standard_layout_v<NativePedReferenceToken> && std::is_trivially_copyable_v<NativePedReferenceToken>);
}
