/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep mutable original stats and borrowed live records distinct
 *****************************************************************************/
#include "CWeaponStatManagerPolicyDE.h"
#include <algorithm>
#include <stdexcept>

namespace mta::game_de
{
    CWeaponStatManagerPolicyDE::CWeaponStatManagerPolicyDE()
    {
        Init();
        // Preserve the actual classic loops and insertion order, including its
        // duplicate standard-skill entries and zero-default metadata after 42.
        // Reindexing these would change SDK first-match and mutable originals.
        for (int index = 0; index <= WEAPONTYPE_EXTINGUISHER; ++index)
            AddOriginal(static_cast<eWeaponType>(WEAPONTYPE_PISTOL + index), WEAPONSKILL_STD);
        for (int skill = 0; skill < 3; ++skill)
            for (int index = 0; index < 11; ++index)
                AddOriginal(static_cast<eWeaponType>(WEAPONTYPE_PISTOL + index), static_cast<eWeaponSkill>(skill));
    }

    void CWeaponStatManagerPolicyDE::AddOriginal(eWeaponType type, eWeaponSkill skill)
    {
        // The SDK has no virtual destructor. Own the concrete CPU type so
        // exceptional construction and retirement never delete through it.
        auto  value = std::make_unique<MtaGame::CWeaponStatOwned>(type, skill, ReadOriginal, this);
        auto* pointer = value.get();
        ownedOriginals.push_back(std::move(value));
        originals.push_back(pointer);
    }

    CWeaponStat* CWeaponStatManagerPolicyDE::GetWeaponStats(eWeaponType type, eWeaponSkill skill)
    {
        return MtaGame::CWeaponStatListPolicy::Find(live, type, skill);
    }

    CWeaponStat* CWeaponStatManagerPolicyDE::GetOriginalWeaponStats(eWeaponType type, eWeaponSkill skill)
    {
        return MtaGame::CWeaponStatListPolicy::Find(originals, type, skill);
    }

    MtaGame::CWeaponStatListPolicy::Selection CWeaponStatManagerPolicyDE::Select(eWeaponType type, float level)
    {
        auto* poor = GetWeaponStats(type, WEAPONSKILL_POOR);
        auto* normal = GetWeaponStats(type, WEAPONSKILL_STD);
        auto* professional = GetWeaponStats(type, WEAPONSKILL_PRO);
        return MtaGame::CWeaponStatListPolicy::Select(poor, normal, professional, level);
    }

    CWeaponStat* CWeaponStatManagerPolicyDE::GetWeaponStatsFromSkillLevel(eWeaponType type, float level)
    {
        return Select(type, level).stat;
    }

    eWeaponSkill CWeaponStatManagerPolicyDE::GetWeaponSkillFromSkillLevel(eWeaponType type, float level)
    {
        return Select(type, level).skill;
    }

    void CWeaponStatManagerPolicyDE::Init()
    {
        MtaGame::CWeaponStatDefaults::Initialize();
    }

    void CWeaponStatManagerPolicyDE::ResetLists()
    {
        MtaGame::CWeaponStatListPolicy::ResetLists();
    }

    bool CWeaponStatManagerPolicyDE::LoadDefault(CWeaponStat* destination, eWeaponType type, eWeaponSkill skill)
    {
        return MtaGame::CWeaponStatDefaults::Load(destination, type, skill);
    }

    CWeaponStat* CWeaponStatManagerPolicyDE::CreateWeaponStatUnlisted(eWeaponType type, eWeaponSkill skill)
    {
        // This is an unlisted CPU object, just as in MTA. Its lazy original
        // provider borrows this manager's ordinary lifetime; the caller owns
        // the concrete object and must retire it before its provider.
        return new MtaGame::CWeaponStatOwned(type, skill, ReadOriginal, this);
    }

    MtaGame::WeaponStatOriginal CWeaponStatManagerPolicyDE::ReadOriginal(void* context, eWeaponType type, eWeaponSkill skill)
    {
        auto& manager = *static_cast<CWeaponStatManagerPolicyDE*>(context);
        auto* original = manager.GetOriginalWeaponStats(type, skill);
        if (!original)
            throw std::out_of_range("DE weapon stat metadata has no current manager original");
        return {static_cast<DWORD>(original->GetFlags()), original->GetAnimGroup()};
    }

    bool CWeaponStatManagerPolicyDE::RegisterLive(CWeaponStat& value)
    {
        if (std::find(live.begin(), live.end(), &value) != live.end())
            return false;
        live.push_back(&value);
        return true;
    }

    bool CWeaponStatManagerPolicyDE::UnregisterLive(CWeaponStat& value) noexcept
    {
        const auto found = std::find(live.begin(), live.end(), &value);
        if (found == live.end())
            return false;
        live.erase(found);
        return true;
    }
}
