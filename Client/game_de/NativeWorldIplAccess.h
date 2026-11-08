/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep dynamic IPL removal/restoration on the existing engine owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <game/CGameWorldPolicy.h>

namespace mta::game_de
{
    class NativeWorldIplAccess
    {
    public:
        // The enclosing world lifecycle still admits its other effects before
        // calling these. This fresh check neither removes the world nor grants
        // a producer-side IPL lease. Older owners explicitly refuse the work.
        virtual ClockResult CheckWorldIplStreamingReady() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult DisableWorldIplStreaming() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult EnableWorldIplStreaming(MtaGame::CGameWorldPolicy::IplFilter) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWorldIplAccess() = default;
    };
}
