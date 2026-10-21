/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the existing MTA custom weapon-stat data and policy
 ****************************************************************************/
#include "CWeaponStatFactoryDE.h"

namespace mta::game_de
{
    CWeaponStat* CreateWeaponStatDE(eWeaponType weapon, eWeaponSkill skill)
    {
        // Match the existing CGameSA unlisted factory ownership contract.
        return new MtaGame::CWeaponStatOwned(weapon, skill);
    }
}
