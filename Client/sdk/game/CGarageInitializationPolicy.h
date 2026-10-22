/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's exact ordered garage initialization schedule
 *****************************************************************************/
#pragma once
#include <array>
#include <cstdint>

namespace MtaGame
{
    struct CGarageInitializationPolicy final
    {
        // These are existing MTA SDK garage identities, not an inferred native
        // array order. Each backend must supply its own admitted ID mapping.
        inline static constexpr std::array<std::uint32_t, 16> Indices = {7, 8, 10, 11, 12, 15, 18, 19, 24, 27, 32, 33, 36, 40, 41, 47};

        template <class SetType>
        static void Initialize(SetType&& setType)
        {
            // Preserve the classic prefix if a receiver refuses. This policy
            // adds no rollback, automatic retry, skipped row or success latch.
            for (const auto index : Indices)
                setType(index, std::uint8_t{1});
        }
    };
}
