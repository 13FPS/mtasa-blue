/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share exact MTA style-change and extra-move bit policy
 ****************************************************************************/
#pragma once
#include <cstdint>
namespace MtaGame
{
    struct CPedFightingStylePolicy final
    {
        static constexpr bool ChangesStyle(std::uint8_t current, std::uint8_t requested) noexcept { return current != requested; }
        // Same-style callers must return before applying this mask. Changed
        // style keeps all existing extra bits;0/outside1..6 adds no new bit.
        static constexpr std::uint8_t ExtraMask(std::uint8_t styleExtra) noexcept
        {
            return styleExtra > 0 && styleExtra <= 6 ? static_cast<std::uint8_t>(1u << (styleExtra - 1)) : std::uint8_t{0};
        }
    };
}
