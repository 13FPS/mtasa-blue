/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward the actual player rebuild once to its registered owner
 ****************************************************************************/
#include "CPedRebuildDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedRebuildDE::RebuildPlayer()
    {
        // Copy the borrowed receiver before admission. The genuine DE entry
        // takes one ped argument; classic's private false argument is not a
        // shared SDK option or an extra argument for this engine.
        const auto selected = ReferenceToken();
        const auto result = nativeRebuildAccess.RebuildPed(selected);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped rebuild owner refused RebuildPlayer: " + std::to_string(static_cast<unsigned>(result)));
    }
}
