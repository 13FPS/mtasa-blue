/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep all 32 bits of the current native world-area observation
 *****************************************************************************/
#include "CWorldAreaDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    DWORD CWorldAreaDE::GetCurrentArea()
    {
        std::uint32_t value = 0;
        const auto    result = areaOwner.ReadWorldCurrentArea(value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE world owner refused GetCurrentArea: " + std::to_string(static_cast<unsigned>(result)));
        // This SDK getter has no enum/clamp/fallback policy. Return the exact
        // accepted native DWORD, including zero and values above BYTE range.
        return static_cast<DWORD>(value);
    }
}
