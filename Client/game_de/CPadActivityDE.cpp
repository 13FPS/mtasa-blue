/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward the SDK's game-time value through checked native access
 *****************************************************************************/
#include "CPadActivityDE.h"
#include "NativePadActivityAccess.h"
#include <cstdint>
#include <stdexcept>

namespace mta::game_de
{
    void CPadActivityDE::SetLastTimeTouched(DWORD time)
    {
        static_assert(sizeof(DWORD) == sizeof(std::uint32_t));
        // Remote context switching supplies the engine's millisecond domain.
        // Preserve its complete DWORD, including wrap; do not invent wall time
        // or report success when the borrowed native owner refuses the field.
        if (!activity.SetLastTimeTouched(static_cast<std::uint32_t>(time)))
            throw std::runtime_error("DE CPad activity timestamp unavailable");
    }
}
