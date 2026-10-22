/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share actual MTA garage door-state storage independently of script commands
 ****************************************************************************/
#pragma once
#include <cstdint>
namespace MtaGame
{
    struct CGarageDoorStatePolicy final
    {
        // MTA stores these bytes directly. The DE script open/close commands
        // have transition gates and its IsOpen query uses a different pair;
        // neither command is a substitute for this existing SDK contract.
        static constexpr std::uint8_t StateForOpen(bool open) noexcept { return open ? 3 : 2; }
        static constexpr bool         IsOpen(std::uint8_t state) noexcept { return state == 1 || state == 3; }
    };
}
