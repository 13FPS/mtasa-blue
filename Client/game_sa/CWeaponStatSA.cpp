/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CWeaponStatSA.cpp
 *  PURPOSE:     Header file for custom weapon stats.
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CGameSA.h"
#include "CWeaponStatManagerSA.h"
#include "CWeaponStatSA.h"
#include <game/CWeaponStatFlagPolicy.h>

extern CGameSA* pGame;

// For Data Storage and won't apply changes.
CWeaponStatSA::CWeaponStatSA(eWeaponType weaponType, eWeaponSkill skillLevel) : m_weaponType{weaponType}, m_skillLevel{skillLevel}
{
    m_pWeaponStats = new CWeaponInfoSAInterface();
}

// For scripting API and applies changes.
CWeaponStatSA::CWeaponStatSA(CWeaponInfoSA* pWeaponInfo, eWeaponType weaponType, eWeaponSkill skillLevel) : m_weaponType{weaponType}, m_skillLevel{skillLevel}
{
    m_pWeaponStats = pWeaponInfo->GetInterface();
}

CWeaponStatSA::~CWeaponStatSA()
{
}

eWeaponType CWeaponStatSA::GetWeaponType()
{
    return m_weaponType;
}

eWeaponSkill CWeaponStatSA::GetWeaponSkillLevel()
{
    return m_skillLevel;
}

void CWeaponStatSA::SetWeaponType(eWeaponType weaponType)
{
    m_weaponType = weaponType;
}

void CWeaponStatSA::SetWeaponSkillLevel(eWeaponSkill skillLevel)
{
    m_skillLevel = skillLevel;
}

void CWeaponStatSA::ToggleFlagBits(DWORD flagBits)
{
    HandleFlagsValueChange(m_pWeaponStats->m_nFlags ^ flagBits);
}

void CWeaponStatSA::SetFlagBits(DWORD flagBits)
{
    HandleFlagsValueChange(m_pWeaponStats->m_nFlags | flagBits);
}

void CWeaponStatSA::ClearFlagBits(DWORD flagBits)
{
    HandleFlagsValueChange(m_pWeaponStats->m_nFlags & ~flagBits);
}

void CWeaponStatSA::HandleFlagsValueChange(DWORD newValue)
{
    // Manager originals are mutable SDK objects. Resolve them at each flag
    // transition so the shared policy preserves the existing classic source.
    const auto original = [](void*, eWeaponType weapon, eWeaponSkill skill) -> MtaGame::WeaponStatOriginal
    {
        CWeaponStat* value = pGame->GetWeaponStatManager()->GetOriginalWeaponStats(weapon, skill);
        return {static_cast<DWORD>(value->GetFlags()), value->GetAnimGroup()};
    };
    MtaGame::ApplyWeaponStatFlags(*this, newValue, original, nullptr);
}
