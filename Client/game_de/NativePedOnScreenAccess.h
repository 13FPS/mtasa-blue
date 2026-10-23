/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the current owner for the actual native on-screen query
 *****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>
namespace mta::game_de
{
    class NativePedOnScreenAccess
    {
    public:
        // The native query may resolve lazy geometry before its camera test.
        // Its current owner governs entry and failure; an older owner cannot
        // turn refusal into a successful off-screen or raw-visible result.
        virtual ClockResult ReadPedOnScreen(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedOnScreenAccess() = default;
    };
}
