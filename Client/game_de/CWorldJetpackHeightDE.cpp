/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Return the accepted native jetpack-height scalar without a fallback
 *****************************************************************************/
#include "CWorldJetpackHeightDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    float CWorldJetpackHeightDE::GetJetpackMaxHeight()
    {
        float      value = 0;
        const auto result = jetpackOwner.ReadJetpackMaxHeight(value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE world owner refused GetJetpackMaxHeight: " + std::to_string(static_cast<unsigned>(result)));
        // Preserve the accepted float bits. A constant matching one observed
        // build would hide an unavailable/stale native owner rather than read it.
        return value;
    }
}
