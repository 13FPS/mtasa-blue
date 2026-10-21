/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Select genuine DE SDK weapon wrappers using MTA's common policy
 *****************************************************************************/
#include "CGameWeaponInfoDE.h"
#include "CWeaponStatFactoryDE.h"

#include <stdexcept>

namespace mta::game_de
{
    CWeaponStat* CGameWeaponInfoDE::CreateWeaponStat(eWeaponType weapon, eWeaponSkill skill)
    {
        return CreateWeaponStatDE(weapon, skill);
    }

    CWeaponInfo* CGameWeaponInfoDE::GetWeaponInfo(eWeaponType weapon, eWeaponSkill skill)
    {
        std::size_t index = 0;
        if (!MtaGame::CWeaponInfoIndexPolicy::TryGetIndex(weapon, skill, index))
            return nullptr;

        auto* wrapper = ResolveWeaponInfoSlot(index);
        if (!wrapper)
            throw std::runtime_error("DE CGame weapon info requires an available SDK weapon wrapper");
        return wrapper;
    }
}
