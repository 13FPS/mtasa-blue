/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's model/satchel/grant/returned-wrapper order on DE
 *****************************************************************************/
#include "CPedGiveWeaponDE.h"
#include <game/CGame.h>
#include <game/CModelInfo.h>
#include <game/CPedGiveWeaponPolicy.h>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    CWeapon* CPedGiveWeaponDE::GiveWeapon(eWeaponType weaponType, std::uint32_t ammo, eWeaponSkill skill)
    {
        // Ordinary model preparation may call back into SDK services. Fix the
        // immutable registered receiver first; each later grant independently
        // qualifies it rather than holding a native gate across that itinerary.
        const auto selected = ReferenceToken();
        return MtaGame::CPedGiveWeaponPolicy::Give(
            weaponType, ammo, skill, WEAPONTYPE_UNARMED, WEAPONTYPE_REMOTE_SATCHEL_CHARGE, BLOCKING,
            [this](eWeaponType requestedType, eWeaponSkill requestedSkill) { return weaponGame.GetWeaponInfo(requestedType, requestedSkill); },
            [this](std::uint32_t model) { return weaponGame.GetModelInfo(model); }, [this] { (void)GiveWeapon(WEAPONTYPE_DETONATOR, 1, WEAPONSKILL_STD); },
            [this, &selected](eWeaponType requestedType, std::uint32_t requestedAmmo)
            {
                std::uint32_t slot = 0;
                const auto    result = nativeGrantAccess.GrantPedWeapon(selected, requestedType, requestedAmmo, slot);
                if (result != ClockResult::Ok)
                    throw std::runtime_error("DE ped weapon grant owner refused GiveWeapon: " + std::to_string(static_cast<unsigned>(result)));
                return slot;
            },
            [this](std::uint32_t slot) { return GetWeapon(static_cast<eWeaponSlot>(slot)); });
    }
}
