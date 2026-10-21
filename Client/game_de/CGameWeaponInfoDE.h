/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA weapon-info selection without publishing missing wrappers
 *****************************************************************************/
#pragma once

#include "CGameTimingDE.h"
#include <game/CWeaponInfoIndexPolicy.h>

namespace mta::game_de
{
    class CGameWeaponInfoDE : public CGameTimingDE
    {
    public:
        CWeaponInfo* GetWeaponInfo(eWeaponType weapon, eWeaponSkill skill = WEAPONSKILL_STD) override;

        CWeaponStat* CreateWeaponStat(eWeaponType weapon, eWeaponSkill skill) override;

    protected:
        CGameWeaponInfoDE(const NativeGameTimingReader& reader, NativeGameTimingAccess& writer) noexcept : CGameTimingDE(reader, writer) {}

        // The concrete DE weapon subsystem must supply genuine SDK wrappers.
        // Keeping this dependency pure prevents an empty registry or a cast
        // native weapon row from appearing as a usable CGame implementation.
        virtual CWeaponInfo* ResolveWeaponInfoSlot(std::size_t slot) = 0;
    };
    static_assert(std::is_abstract_v<CGameWeaponInfoDE> && std::is_base_of_v<CGame, CGameWeaponInfoDE>);
}
