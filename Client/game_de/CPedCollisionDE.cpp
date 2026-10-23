/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward actual collision-use bool once without hidden physics effects
 ****************************************************************************/
#include "CPedCollisionDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedCollisionDE::SetUsesCollision(bool usesCollision)
    {
        const auto result = nativeCollisionAccess.SetPedUsesCollision(ReferenceToken(), usesCollision);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped collision owner refused SetUsesCollision: " + std::to_string(static_cast<unsigned>(result)));
        // Actual classic SDK and genuine DE0619 both change only this raw
        // collision-use flag. Do not add an actor rebuild, world removal or
        // automatic retry after a possibly accepted native write.
    }
}
