/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the actual MTA weapon-info/live-stat startup row schedule
 *****************************************************************************/
#pragma once
#include <windows.h>
#include "CWeaponInfoIndexPolicy.h"

namespace MtaGame
{
    class CWeaponStartupPolicy final
    {
    public:
        struct Entry
        {
            std::size_t  row;
            eWeaponType  infoType;
            eWeaponType  statType;
            eWeaponSkill skill;
        };

        template <class InitializeRow>
        static void Initialize(InitializeRow&& initializeRow)
        {
            // Preserve the real constructor's two different standard metadata
            // keys: CWeaponInfo receives22+row while CWeaponStat receives row.
            // Normalizing them would silently change existing MTA behavior.
            for (std::size_t row = 0; row < CWeaponInfoIndexPolicy::StandardCount; ++row)
                initializeRow(Entry{row, static_cast<eWeaponType>(WEAPONTYPE_PISTOL + row), static_cast<eWeaponType>(row), WEAPONSKILL_STD});

            // MTA creates poor, professional and special banks after the
            // standard bank; enum order includes standard between poor/pro.
            // Invoke each row synchronously so a failure preserves its exact
            // already-applied prefix without creating or replaying later rows.
            constexpr eWeaponSkill skills[]{WEAPONSKILL_POOR, WEAPONSKILL_PRO, WEAPONSKILL_SPECIAL};
            for (std::size_t group = 0; group < 3; ++group)
                for (std::size_t index = 0; index < CWeaponInfoIndexPolicy::SkillCount; ++index)
                {
                    const auto row = CWeaponInfoIndexPolicy::StandardCount + group * CWeaponInfoIndexPolicy::SkillCount + index;
                    const auto weapon = static_cast<eWeaponType>(WEAPONTYPE_PISTOL + index);
                    initializeRow(Entry{row, weapon, weapon, skills[group]});
                }
        }
    };
}
