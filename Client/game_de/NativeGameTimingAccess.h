/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Internal engine-owner writes for the existing CGame timing methods
 *****************************************************************************/
#pragma once

#include <game/CGameClockService.h>
#include <enums/SystemState.h>
#include "NativeCheatProfile.h"
#include "CWeatherTransitionDE.h"
#include "CWeatherScalarsDE.h"
#include "NativeWeaponInfoWriteAccess.h"
#include <cstdint>

namespace mta::game_de
{
    // Read-only startup binding does not authorize mutations. The qualified
    // native owner implements these methods and retains restoration debt after
    // a partial write; this interface is not another exported service protocol.
    class NativeGameTimingAccess : public NativeWeatherTransitionAccess, public NativeWeatherScalarAccess, public NativeWeaponInfoWriteAccess
    {
    public:
        virtual ClockResult SetTimeScale(float value) noexcept = 0;
        virtual ClockResult SetMinuteDuration(std::uint32_t value) noexcept = 0;
        virtual ClockResult Pause(bool value) noexcept = 0;
        // Startup state shares the same natural engine callback admission.
        // Older timing-only owners explicitly refuse this additional write;
        // recognizing a state value never grants an engine transition lease.
        virtual ClockResult SetSystemState(SystemState::Enum) noexcept { return ClockResult::NotInitialized; }
        // The four existing SDK cheat properties use this same synchronous
        // native owner. A timing-only implementation must refuse them without
        // publishing a cached value or pretending to reset the native registry.
        virtual ClockResult ReadCheatFlag(NativeCheatProfile::Flag, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetCheatFlag(NativeCheatProfile::Flag, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeGameTimingAccess() = default;
    };
}
