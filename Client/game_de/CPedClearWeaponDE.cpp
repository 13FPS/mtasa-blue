/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward MTA's single-weapon clear once to its current native owner
 *****************************************************************************/
#include "CPedClearWeaponDE.h"
#include <game/CWeapon.h>
#include <array>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedClearWeaponDE::ClearWeapons()
    {
        // Classic has a stable wrapper even for an unarmed slot. Verify the
        // bank is complete before changing ammo or removing any weapon.
        std::array<CWeapon*, WEAPONSLOT_MAX> weapons{};
        for (std::size_t slot = 0; slot < weapons.size(); ++slot)
        {
            weapons[slot] = GetWeapon(static_cast<eWeaponSlot>(slot));
            if (!weapons[slot])
                throw std::runtime_error("DE ped weapon slot missing during ClearWeapons");
        }
        for (CWeapon* weapon : weapons)
        {
            weapon->SetAmmoInClip(0);
            weapon->SetAmmoTotal(0);
            weapon->Remove();
        }
    }

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
