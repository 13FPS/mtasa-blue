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
#include "NativeWeaponInfoVectorAccess.h"
#include "NativeWeatherOverrideAccess.h"
#include "NativeStatsWriteAccess.h"
#include "NativeStartupCompletionAccess.h"
#include <cstdint>

class CStats;

namespace mta::game_de
{
    // Read-only startup binding does not authorize mutations. The qualified
    // native owner implements these methods and retains restoration debt after
    // a partial write; this interface is not another exported service protocol.
    class NativeGameTimingAccess : public NativeWeatherTransitionAccess,
                                   public NativeWeatherScalarAccess,
                                   public NativeWeaponInfoWriteAccess,
                                   public NativeWeaponInfoVectorAccess,
                                   public NativeWeatherOverrideAccess,
                                   public NativeStatsWriteAccess
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
        // Shared names/permissions do not manufacture native bindings. The
        // same engine owner qualifies each admitted read, or explicitly refuses.
        virtual ClockResult ReadWorldCheat(MtaGame::CWorldCheatPolicy::Id, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetCheatFlag(NativeCheatProfile::Flag, bool) noexcept { return ClockResult::NotInitialized; }
        // Shared MTA lookup can publish only a genuine bank wrapper qualified
        // by this native owner, never a partial provider or a cached fake row.
        virtual ClockResult GetWeaponInfoSlot(std::size_t, CWeaponInfo*&) noexcept { return ClockResult::NotInitialized; }
        // A complete SDK object borrows the independently qualified native
        // reader and this callback owner; publication grants no later lease.
        virtual ClockResult GetStats(CStats*&) noexcept { return ClockResult::NotInitialized; }
        // Publish the complete runtime-owned SDK service only after its native
        // owner qualifies it. A borrowed object never grants a later callback.
        virtual ClockResult GetWeather(CWeather*&) noexcept { return ClockResult::NotInitialized; }
        // The real request belongs to a distinct admitted normal-frame scope;
        // a gameplay timer or earlier observation never grants lifecycle calls.
        virtual ClockResult StartGame() noexcept { return ClockResult::NotInitialized; }
        // Native pump facts are scoped to an actually entered request in this
        // owner generation. They do not certify player or network readiness.
        virtual ClockResult ReadStartupProgress(NativeStartupCompletionAccess::Value&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeGameTimingAccess() = default;
    };
}
