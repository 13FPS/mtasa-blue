/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine wanted transactions borrowed from the existing engine owner
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>

namespace mta::game_de
{
    class NativeWantedAccess
    {
    public:
        virtual ClockResult ReadWantedLevel(std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetWantedLevel(std::uint32_t, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWantedAccess() = default;
    };
}
