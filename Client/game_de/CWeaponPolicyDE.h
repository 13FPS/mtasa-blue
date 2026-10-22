/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual weapon wrapper metadata independently of native slots
 *****************************************************************************/
#pragma once

#include <windows.h>  // The existing weapon-info SDK declares DWORD parameters.
#include <game/CWeaponInfo.h>
#include <enums/PedPieceTypes.h>
#include <type_traits>

class CEntity;
class CEntitySAInterface;
#include <game/CWeapon.h>

namespace mta::game_de
{
    // Classic CWeaponSA stores these constructor arguments in its CPU wrapper.
    // Keeping them independent of engine reads preserves owner identity even
    // when a retained native slot is unavailable; neither value owns the ped.
    class CWeaponPolicyDE : public CWeapon
    {
    public:
        CPed*       GetPed() const noexcept override { return owner; }
        eWeaponSlot GetSlot() const noexcept override { return weaponSlot; }

    protected:
        CWeaponPolicyDE(CPed* borrowedOwner, eWeaponSlot slot) noexcept : owner(borrowedOwner), weaponSlot(slot) {}

    private:
        // No default slot or native CWeapon lifetime is manufactured here.
        // The enclosing weapon service must supply the actual wrapper pair.
        CPed*       owner;
        eWeaponSlot weaponSlot;
    };
    // Reload timing still requires the genuine backend stat operation used by
    // CWeaponSA; animation-loop arithmetic is a different fire-time operation.
    static_assert(std::is_abstract_v<CWeaponPolicyDE> && std::is_base_of_v<CWeapon, CWeaponPolicyDE>);
}
