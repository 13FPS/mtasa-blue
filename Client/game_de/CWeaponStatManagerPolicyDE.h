/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual MTA weapon-stat manager data and list lifecycle policy
 *****************************************************************************/
#pragma once
#include <windows.h>
#include <game/CWeaponStatManager.h>
#include <game/CWeaponStatOwned.h>
#include <game/CWeaponStatListPolicy.h>
#include <list>
#include <memory>
#include <type_traits>

namespace mta::game_de
{
    // The CPU originals and lookup policy are complete, but native live-wrapper
    // creation still needs a complete CWeaponStat provider. Keep that SDK method
    // pure and do not publish this as CGame::GetWeaponStatManager.
    class CWeaponStatManagerPolicyDE : public CWeaponStatManager
    {
    public:
        CWeaponStat* GetWeaponStats(eWeaponType, eWeaponSkill = WEAPONSKILL_STD) override;
        CWeaponStat* GetWeaponStatsFromSkillLevel(eWeaponType, float) override;
        CWeaponStat* GetOriginalWeaponStats(eWeaponType, eWeaponSkill = WEAPONSKILL_STD) override;
        void         Init() override;
        void         ResetLists() override;
        bool         LoadDefault(CWeaponStat*, eWeaponType, eWeaponSkill = WEAPONSKILL_STD) override;
        CWeaponStat* CreateWeaponStatUnlisted(eWeaponType, eWeaponSkill) override;
        eWeaponSkill GetWeaponSkillFromSkillLevel(eWeaponType, float) override;

        CWeaponStatManagerPolicyDE(const CWeaponStatManagerPolicyDE&) = delete;
        CWeaponStatManagerPolicyDE& operator=(const CWeaponStatManagerPolicyDE&) = delete;
        CWeaponStatManagerPolicyDE(CWeaponStatManagerPolicyDE&&) = delete;
        CWeaponStatManagerPolicyDE& operator=(CWeaponStatManagerPolicyDE&&) = delete;

    protected:
        CWeaponStatManagerPolicyDE();
        ~CWeaponStatManagerPolicyDE() = default;

        // A future complete native factory must retain its own typed wrappers
        // and existing engine owner. This registry only borrows each actual
        // object until that owner unregisters it before destruction/retirement.
        bool                               RegisterLive(CWeaponStat&);
        bool                               UnregisterLive(CWeaponStat&) noexcept;
        static MtaGame::WeaponStatOriginal ReadOriginal(void*, eWeaponType, eWeaponSkill);

    private:
        void                                                  AddOriginal(eWeaponType, eWeaponSkill);
        MtaGame::CWeaponStatListPolicy::Selection             Select(eWeaponType, float);
        std::list<std::unique_ptr<MtaGame::CWeaponStatOwned>> ownedOriginals;
        std::list<CWeaponStat*>                               originals;
        std::list<CWeaponStat*>                               live;
    };
    static_assert(std::is_abstract_v<CWeaponStatManagerPolicyDE> && std::is_base_of_v<CWeaponStatManager, CWeaponStatManagerPolicyDE>);
}
