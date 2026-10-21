/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Typed weather suppression intents for the existing native owner
 *****************************************************************************/
#pragma once
#include "NativeWeatherScalarProfile.h"
#include <game/CGameClockService.h>

namespace mta::game_de
{
    // Observing a native scalar does not authorize changing its writers. The
    // same retained engine owner qualifies suppression, scalar commit and exact
    // writer restoration; older read-only implementations refuse explicitly.
    class NativeWeatherOverrideAccess
    {
    public:
        virtual ClockResult SetWeatherScalarOverride(NativeWeatherScalarProfile::Field, float) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ResetWeatherScalarOverride(NativeWeatherScalarProfile::Field) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWeatherOverrideAccess() = default;
    };
}
