/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Resolve current stats through genuine existing SDK services once
 ****************************************************************************/
#include "CPedWeaponStatsDE.h"
#include <game/CGame.h>
#include <game/CStats.h>
#include <game/CWeapon.h>
#include <game/CWeaponStatManager.h>
#include <game/CPedWeaponStatPolicy.h>
namespace mta::game_de
{
    CWeaponStat* CPedWeaponStatsDE::GetCurrentWeaponStat() const
    {
        // Keep both GetStats calls separate, just as the classic method. Each
        // service retains its own fresh admission and can refuse by exception.
        return MtaGame::CPedWeaponStatPolicy::Current(
            this, [this] { return game.GetPedContext(); }, [this] { return GetWeapon(GetCurrentWeaponSlot()); }, [this](eWeaponType type)
            { return game.GetStats()->GetSkillStatIndex(type); }, [this](std::uint16_t index) { return game.GetStats()->GetStatValue(index); },
            [this](eWeaponType type, float level) { return game.GetWeaponStatManager()->GetWeaponStatsFromSkillLevel(type, level); },
            [] { OutputDebugStringA("WARNING: GetCurrentWeaponStat ped context mismatch\n"); });
    }
    float CPedWeaponStatsDE::GetCurrentWeaponRange() const
    {
        return MtaGame::CPedWeaponStatPolicy::Range([this] { return GetCurrentWeaponStat(); });
    }
}
