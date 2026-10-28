/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional current-owner publication of the qualified startup streaming subset
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>

class CStreaming;

namespace mta::game_de
{
    class NativeStartupStreamingAccess
    {
    public:
        virtual ~NativeStartupStreamingAccess() = default;
        // Initialize requests special models only after publishing each model.
        // This publishes a real borrowed SDK receiver for that limited path;
        // it is neither a complete streaming service nor authority for later
        // effects, which retain their own fresh native-owner validation.
        virtual ClockResult RequireStartupStreamingReceiverReady() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult GetStartupStreaming(CStreaming*&) noexcept { return ClockResult::NotInitialized; }
    };
}
