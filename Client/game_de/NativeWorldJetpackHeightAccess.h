/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the current owner for the actual native jetpack-height value
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
namespace mta::game_de
{
    class NativeWorldJetpackHeightAccess
    {
    public:
        // A refused native observation is not a zero/default height. This
        // reader grants no right to alter shared immutable engine constants.
        virtual ClockResult ReadJetpackMaxHeight(float&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWorldJetpackHeightAccess() = default;
    };
}
