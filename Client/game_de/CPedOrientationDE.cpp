/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw MTA radians and the genuine DE orientation effects
 ****************************************************************************/
#include "CPedOrientationDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedOrientationDE::SetOrientation(float x, float y, float z)
    {
        // CEntity's SDK already supplies radians. The named native script's
        // degree conversion is caller syntax, not part of this operation.
        // The same registered owner admits the complete three-phase effect;
        // neither a scalar heading write nor a CPU last-good update replaces it.
        const auto result = nativeOrientationAccess.SetPedOrientation(ReferenceToken(), CVector(x, y, z));
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped pose owner refused SetOrientation: " + std::to_string(static_cast<unsigned>(result)));
    }
}
