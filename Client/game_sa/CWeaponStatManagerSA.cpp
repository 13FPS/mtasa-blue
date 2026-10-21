/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CWeaponStatManagerSA.h
 *  PURPOSE:     source file for the custom weapon stats manager.
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CWeaponStatManagerSA.h"
#include <game/CWeaponStatListPolicy.h>

CWeaponStatManagerSA::CWeaponStatManagerSA()
{
    Init();
    InitLists();
}

void CWeaponStatManagerSA::InitLists()
{
    CWeaponStatSA* pWeaponStat = nullptr;
    // Make our list nicer by starting at the non-skill weapons so we don't have duplicates of STD weapon skill
    for (int i = 0; i <= WEAPONTYPE_EXTINGUISHER; i++)
    {
        eWeaponType weaponType = (eWeaponType)(WEAPONTYPE_PISTOL + i);
        // Storage for Original weapon data ( for resetting )
        pWeaponStat = new CWeaponStatSA(weaponType, WEAPONSKILL_STD);
        m_OriginalWeaponData.push_back(pWeaponStat);
        // Store our defaults for this weapon.
        LoadDefaultInternal(pWeaponStat, weaponType);
    }

    for (int skill = 0; skill < 3; skill++)
    {
        for (int i = 0; i < NUM_WeaponInfosOtherSkill; i++)
        {
            eWeaponType weaponType = (eWeaponType)(WEAPONTYPE_PISTOL + i);
            // Storage for Original weapon data ( for resetting )
            pWeaponStat = new CWeaponStatSA(weaponType, (eWeaponSkill)skill);
            m_OriginalWeaponData.push_back(pWeaponStat);
            // Store our defaults for this weapon.
            LoadDefaultInternal(pWeaponStat, weaponType, (eWeaponSkill)skill);
        }
    }
}

// Don't call me for now seems to screw up rejoining
void CWeaponStatManagerSA::ResetLists()
{
    MtaGame::CWeaponStatListPolicy::ResetLists();
}

CWeaponStatManagerSA::~CWeaponStatManagerSA()
{
    for (std::list<CWeaponStat*>::iterator iter = m_OriginalWeaponData.begin(); iter != m_OriginalWeaponData.end(); iter++)
    {
        if ((*iter))
            delete (*iter);
    }

    for (std::list<CWeaponStat*>::iterator iter = m_WeaponData.begin(); iter != m_WeaponData.end(); iter++)
    {
        if ((*iter))
            delete (*iter);
    }
}

void CWeaponStatManagerSA::CreateWeaponStat(CWeaponInfo* pInterface, eWeaponType weaponType, eWeaponSkill weaponSkill)
{
    CWeaponStatSA* pWeaponStat = nullptr;

    // Storage for new weapon data ( for script use )
    pWeaponStat = new CWeaponStatSA(((CWeaponInfoSA*)(pInterface)), weaponType, weaponSkill);
    m_WeaponData.push_back(pWeaponStat);
    LoadDefaultInternal(pWeaponStat, weaponType, weaponSkill);
}

CWeaponStat* CWeaponStatManagerSA::CreateWeaponStatUnlisted(eWeaponType weaponType, eWeaponSkill weaponSkill)
{
    CWeaponStatSA* pWeaponStat = nullptr;

    // Storage for new weapon data ( for script use )
    pWeaponStat = new CWeaponStatSA(weaponType, weaponSkill);
    LoadDefaultInternal(pWeaponStat, weaponType, weaponSkill);
    return pWeaponStat;
}

CWeaponStat* CWeaponStatManagerSA::GetWeaponStats(eWeaponType type, eWeaponSkill skill)
{
    return MtaGame::CWeaponStatListPolicy::Find(m_WeaponData, type, skill);
}

CWeaponStat* CWeaponStatManagerSA::GetWeaponStatsFromSkillLevel(eWeaponType type, float fSkillLevel)
{
    CWeaponStat* pPoor = GetWeaponStats(type, WEAPONSKILL_POOR);
    CWeaponStat* pStd = GetWeaponStats(type, WEAPONSKILL_STD);
    CWeaponStat* pPro = GetWeaponStats(type, WEAPONSKILL_PRO);
    return MtaGame::CWeaponStatListPolicy::Select(pPoor, pStd, pPro, fSkillLevel).stat;
}

CWeaponStat* CWeaponStatManagerSA::GetOriginalWeaponStats(eWeaponType type, eWeaponSkill skill)
{
    return MtaGame::CWeaponStatListPolicy::Find(m_OriginalWeaponData, type, skill);
}
void CWeaponStatManagerSA::Init()
{
    // Keep classic and DE custom-weapon resets on the same existing MTA defaults.
    MtaGame::CWeaponStatDefaults::Initialize();
}

bool CWeaponStatManagerSA::LoadDefault(CWeaponStat* pDest, eWeaponType weaponType, eWeaponSkill weaponSkill)
{
    return MtaGame::CWeaponStatDefaults::Load(pDest, weaponType, weaponSkill);
}

bool CWeaponStatManagerSA::LoadDefaultInternal(CWeaponStatSA* pDest, eWeaponType weaponType, eWeaponSkill weaponSkill)
{
    return MtaGame::CWeaponStatDefaults::Load(pDest, weaponType, weaponSkill);
}

eWeaponSkill CWeaponStatManagerSA::GetWeaponSkillFromSkillLevel(eWeaponType type, float fSkillLevel)
{
    CWeaponStat* pPoor = GetWeaponStats(type, WEAPONSKILL_POOR);
    CWeaponStat* pStd = GetWeaponStats(type, WEAPONSKILL_STD);
    CWeaponStat* pPro = GetWeaponStats(type, WEAPONSKILL_PRO);
    return MtaGame::CWeaponStatListPolicy::Select(pPoor, pStd, pPro, fSkillLevel).skill;
}
