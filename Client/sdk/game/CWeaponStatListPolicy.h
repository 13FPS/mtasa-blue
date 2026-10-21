/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share existing MTA weapon-stat list and skill-selection behavior
 *****************************************************************************/
#pragma once
#include <windows.h>
#include "CWeaponStat.h"

namespace MtaGame
{
    class CWeaponStatListPolicy final
    {
    public:
        struct Selection
        {
            CWeaponStat* stat;
            eWeaponSkill skill;
        };

        template <class Range>
        static CWeaponStat* Find(const Range& values, eWeaponType type, eWeaponSkill skill)
        {
            // Metadata is mutable through the real SDK. Preserve the first
            // matching list entry rather than caching a type/skill index that
            // would silently stop following later metadata edits or duplicates.
            for (CWeaponStat* value : values)
                if ((value->GetWeaponSkillLevel() == skill || type > WEAPONTYPE_TEC9) && value->GetWeaponType() == type)
                    return value;
            return nullptr;
        }

        static Selection Select(CWeaponStat* poor, CWeaponStat* normal, CWeaponStat* professional, float level)
        {
            if (normal)
            {
                if (poor && professional)
                {
                    // Keep MTA's comparison order, including NaN falling to
                    // poor and a missing poor/pro record selecting standard.
                    if (level >= professional->GetRequiredStatLevel())
                        return {professional, WEAPONSKILL_PRO};
                    if (level >= normal->GetRequiredStatLevel())
                        return {normal, WEAPONSKILL_STD};
                    return {poor, WEAPONSKILL_POOR};
                }
                return {normal, WEAPONSKILL_STD};
            }
            return {nullptr, WEAPONSKILL_STD};
        }

        static void ResetLists() noexcept
        {
            // Classic MTA deliberately disables this reset because resetting
            // its live list breaks rejoining. Sharing that existing policy
            // prevents a port from silently re-enabling native field writes.
        }
    };
}
