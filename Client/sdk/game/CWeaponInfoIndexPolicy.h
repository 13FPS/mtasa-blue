/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share the actual MTA CGame weapon-info slot selection contract
 *****************************************************************************/
#pragma once

#include <cstddef>
#include <cstdint>
#include "CWeaponInfo.h"

namespace MtaGame
{
    // This selects SDK wrapper slots, not native weapon-data addresses. Each
    // backend must populate those slots with its actual CWeaponInfo objects.
    class CWeaponInfoIndexPolicy
    {
    public:
        static constexpr std::size_t StandardCount = WEAPONTYPE_LAST_WEAPONTYPE;
        static constexpr std::size_t SkillCount = WEAPONTYPE_TEC9 - WEAPONTYPE_PISTOL + 1;
        static constexpr std::size_t SlotCount = StandardCount + 3 * SkillCount;

        static bool TryGetIndex(eWeaponType weaponType, eWeaponSkill skill, std::size_t& output) noexcept
        {
            const int weapon = static_cast<int>(weaponType);
            if (!((skill == WEAPONSKILL_STD && weapon >= WEAPONTYPE_UNARMED && weapon < WEAPONTYPE_LAST_WEAPONTYPE) ||
                  (skill != WEAPONSKILL_STD && weapon >= WEAPONTYPE_PISTOL && weapon <= WEAPONTYPE_TEC9)))
                return false;

            std::size_t offset = 0;
            switch (skill)
            {
                case WEAPONSKILL_POOR:
                    offset = StandardCount - WEAPONTYPE_PISTOL;
                    break;
                case WEAPONSKILL_PRO:
                    offset = StandardCount + SkillCount - WEAPONTYPE_PISTOL;
                    break;
                case WEAPONSKILL_SPECIAL:
                    offset = StandardCount + 2 * SkillCount - WEAPONTYPE_PISTOL;
                    break;
                default:
                    // Classic accepts otherwise unknown skill values for the
                    // pistol-to-Tec9 range and falls back to standard slots.
                    // Preserve that existing SDK behavior rather than silently
                    // tightening validation while introducing the DE backend.
                    break;
            }
            output = offset + static_cast<std::size_t>(weapon);
            return true;
        }
    };
}
