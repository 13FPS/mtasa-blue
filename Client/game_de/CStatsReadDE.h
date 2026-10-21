/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA statistics reads over DE's proven native storage
 *****************************************************************************/
#pragma once

#include "NativeStatsReader.h"
#include <game/CStats.h>
#include <type_traits>

namespace mta::game_de
{
    // Native stat mutation also performs stat reactions and special cases.
    // Those two methods stay pure until that real transaction is established.
    class CStatsReadDE : public CStats
    {
    public:
        float          GetStatValue(unsigned short index) override;
        unsigned short GetSkillStatIndex(eWeaponType type) override;

    protected:
        explicit CStatsReadDE(const NativeStatsReader& input) noexcept : native(input) {}

    private:
        static void              Require(Status status, const char* operation);
        const NativeStatsReader& native;
    };
    static_assert(std::is_abstract_v<CStatsReadDE> && std::is_base_of_v<CStats, CStatsReadDE>);
}
