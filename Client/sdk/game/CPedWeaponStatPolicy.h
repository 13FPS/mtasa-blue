/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share actual MTA ped context and weapon-stat selection ordering
 ****************************************************************************/
#pragma once
class CPed;
class CWeaponStat;
namespace MtaGame
{
    // Only SDK orchestration is shared. Each receiver still supplies its real
    // context, weapon, statistic owner and manager; a CPU record is never
    // substituted for an unavailable live native weapon or stat service.
    struct CPedWeaponStatPolicy final
    {
        template <class Context, class Weapon, class SkillIndex, class StatValue, class Select, class Mismatch>
        static CWeaponStat* Current(const CPed* self, Context&& context, Weapon&& weapon, SkillIndex&& skillIndex, StatValue&& statValue, Select&& select,
                                    Mismatch&& mismatch)
        {
            if (context() != self)
            {
                mismatch();
                return nullptr;
            }
            auto* currentWeapon = weapon();
            if (!currentWeapon)
                return nullptr;
            const auto  type = currentWeapon->GetType();
            const auto  index = skillIndex(type);
            const float level = statValue(index);
            return select(type, level);
        }
        template <class CurrentStat>
        static float Range(CurrentStat&& currentStat)
        {
            auto* stat = currentStat();
            // This is MTA's existing actual-null result, not a replacement for
            // owner refusal. Exceptions from either virtual call propagate.
            return stat ? stat->GetWeaponRange() : 1.0f;
        }
    };
}
