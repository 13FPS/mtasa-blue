/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same-owner SDK stat intents preserving native reaction transactions
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>

namespace mta::game_de
{
    class NativeStatsWriteAccess
    {
    public:
        virtual ClockResult SetStatValue(unsigned short, float) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ModifyStat(unsigned short, float) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeStatsWriteAccess() = default;
    };
}
