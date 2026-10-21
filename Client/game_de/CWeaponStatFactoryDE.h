/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the existing MTA custom weapon-stat data and policy
 ****************************************************************************/
#pragma once

#include <game/CWeaponStatOwned.h>

namespace mta::game_de
{
    // This is the unlisted CPU factory used by MTA custom weapons. The
    // completed native CWeaponInfoBankDE remains their separate live-info
    // source; neither this factory nor its objects own that bank.
    CWeaponStat* CreateWeaponStatDE(eWeaponType weapon, eWeaponSkill skill);
}
