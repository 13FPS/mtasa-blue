/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CWeaponStatManagerSA.h
 *  PURPOSE:     header file for the custom weapon stats manager.
 *
 *****************************************************************************/

#pragma once

#include <list>
#include <game/CWeaponStatManager.h>
#include "CWeaponStatSA.h"
#include <game/CWeaponStatDefaults.h>

// Straight out of CGameSA.h
#define CLASSSIZE_WeaponInfo      112  // ##SA##
#define NUM_WeaponInfosStdSkill   WEAPONTYPE_LAST_WEAPONTYPE
#define NUM_WeaponInfosOtherSkill 11
#define NUM_WeaponInfosTotal      (NUM_WeaponInfosStdSkill + (3 * NUM_WeaponInfosOtherSkill))  // std, (poor, pro, special)

#define WEAPON_STAT_MAX 112

// The value type is shared with owned DE custom-weapon data; it is not a native row cast.
using sWeaponInfo = MtaGame::WeaponStatData;

class CWeaponStatManagerSA : public CWeaponStatManager
{
public:
    CWeaponStatManagerSA();
    ~CWeaponStatManagerSA();
    CWeaponStat* GetWeaponStats(eWeaponType type, eWeaponSkill skill = WEAPONSKILL_STD);
    CWeaponStat* GetWeaponStatsFromSkillLevel(eWeaponType type, float fSkillLevel);
    CWeaponStat* GetOriginalWeaponStats(eWeaponType type, eWeaponSkill skill = WEAPONSKILL_STD);
    void         Init();
    void         InitLists();
    void         ResetLists();
    bool         LoadDefault(CWeaponStat* pDest, eWeaponType weaponType, eWeaponSkill skill = WEAPONSKILL_STD);
    void         CreateWeaponStat(CWeaponInfo* pInterface, eWeaponType weaponType, eWeaponSkill weaponSkill);
    CWeaponStat* CreateWeaponStatUnlisted(eWeaponType weaponType, eWeaponSkill weaponSkill);
    eWeaponSkill GetWeaponSkillFromSkillLevel(eWeaponType type, float fSkillLevel);

private:
    bool LoadDefaultInternal(CWeaponStatSA* pDest, eWeaponType weaponType, eWeaponSkill skill = WEAPONSKILL_STD);

    std::list<CWeaponStat*> m_OriginalWeaponData;
    std::list<CWeaponStat*> m_WeaponData;
};
