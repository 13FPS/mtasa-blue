/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the actual DWORD world-area property through its current owner
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de
{
    class NativeWorldAreaAccess
    {
    public:
        // A missing/stale native observation is not successful exterior area0.
        virtual ClockResult ReadWorldCurrentArea(std::uint32_t&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWorldAreaAccess() = default;
    };
}
