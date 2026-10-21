/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the existing MTA custom weapon-stat data and policy
 ****************************************************************************/
#pragma once

#include <windows.h>
#include <cstdint>
#include "CWeaponStat.h"

namespace MtaGame
{
    struct WeaponStatOriginal
    {
        DWORD flags;
        DWORD animGroup;
    };
    using WeaponStatOriginalLookup = WeaponStatOriginal (*)(void*, eWeaponType, eWeaponSkill);

    // Reuse MTA's flag transitions through the real SDK so owned custom data
    // and the classic live wrapper keep identical animation restrictions.
    // The lookup remains lazy and belongs to the original-stat provider;
    // mutable manager originals must not be replaced with a cached default.
    inline void ApplyWeaponStatFlags(CWeaponStat& stat, DWORD desired, WeaponStatOriginalLookup lookup, void* context)
    {
        const auto original = [&]() { return lookup(context, stat.GetWeaponType(), stat.GetWeaponSkillLevel()); };
        if (stat.IsFlagSet(WEAPONTYPE_TWIN_PISTOLS) && !(desired & WEAPONTYPE_TWIN_PISTOLS))
        {
            if ((stat.GetWeaponType() >= WEAPONTYPE_PISTOL && stat.GetWeaponType() <= WEAPONTYPE_SNIPERRIFLE) || stat.GetWeaponType() == WEAPONTYPE_MINIGUN)
                stat.SetAnimGroup(original().animGroup);
        }
        else if (!stat.IsFlagSet(WEAPONTYPE_TWIN_PISTOLS) && (desired & WEAPONTYPE_TWIN_PISTOLS))
        {
            if ((stat.GetWeaponType() >= WEAPONTYPE_PISTOL && stat.GetWeaponType() <= WEAPONTYPE_SNIPERRIFLE) || stat.GetWeaponType() == WEAPONTYPE_MINIGUN)
                stat.SetAnimGroup(17);
        }
        if (!stat.IsFlagSet(WEAPONTYPE_ANIM_RELOAD) && (desired & WEAPONTYPE_ANIM_RELOAD))
        {
            if (!(original().flags & WEAPONTYPE_ANIM_RELOAD))
                desired &= ~WEAPONTYPE_ANIM_RELOAD;
        }
        if (!stat.IsFlagSet(WEAPONTYPE_ANIM_CROUCHFIRE) && (desired & WEAPONTYPE_ANIM_CROUCHFIRE))
        {
            if (!(original().flags & WEAPONTYPE_ANIM_CROUCHFIRE))
                desired &= ~WEAPONTYPE_ANIM_CROUCHFIRE;
        }
        // SetFlags is the SDK's direct initialization operation. Calling it
        // last preserves unrelated bits and avoids recursive flag fixups.
        stat.SetFlags(static_cast<int>(desired));
    }
}
