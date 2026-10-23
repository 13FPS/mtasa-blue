/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual const ped pointer-presence semantics without effects
 ****************************************************************************/
#include "CPedFireDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    bool CPedFireDE::IsOnFire() const
    {
        bool       present = false;
        const auto result = nativeFireAccess.ReadPedOnFire(ReferenceToken(), present);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped fire owner refused IsOnFire: " + std::to_string(static_cast<unsigned>(result)));
        // Actual CPedSA tests only its associated native pointer. Extending
        // this to actor damage/task status would change MTA's SDK meaning.
        return present;
    }
}
