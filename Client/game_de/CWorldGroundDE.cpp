/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native ground height without a script or synthetic fallback
 *****************************************************************************/
#include "CWorldGroundDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    float CWorldGroundDE::FindGroundZFor3DPosition(CVector* position)
    {
        if (!position)
            throw std::invalid_argument("DE world FindGroundZFor3DPosition requires a position");
        // Classic passes copied XYZ by value. Own the intent before entering
        // the borrowed owner, so callbacks cannot alias the caller's vector.
        const CVector intent = *position;
        float         height = 0;
        const auto    result = groundOwner.FindWorldGroundZ(intent, height);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE world owner refused FindGroundZFor3DPosition: " + std::to_string(static_cast<unsigned>(result)));
        // The genuine native miss is +0.0; a refusal is never a miss. Return
        // exactly the accepted scalar, including its raw floating-point bits.
        return height;
    }
}
