/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward MTA's single-weapon clear once to its current native owner
 *****************************************************************************/
#include "CPedClearWeaponDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedClearWeaponDE::ClearWeapon(eWeaponType weaponType)
    {
        // A callback may change the enclosing wrapper's borrowed relation.
        // Select the immutable registered receiver before owner admission and
        // retain the full SDK enum without inferring a slot or changing equip.
        const auto selected = ReferenceToken();
        const auto result = nativeClearAccess.ClearPedWeapon(selected, weaponType);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped weapon clear owner refused ClearWeapon: " + std::to_string(static_cast<unsigned>(result)));
    }
}
