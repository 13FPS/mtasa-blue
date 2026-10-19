/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Internal engine-owner writes for the existing CGame timing methods
 *****************************************************************************/
#pragma once

#include <game/CGameClockService.h>
#include <cstdint>

namespace mta::game_de
{
    // Read-only startup binding does not authorize mutations. The qualified
    // native owner implements these methods and retains restoration debt after
    // a partial write; this interface is not another exported service protocol.
    class NativeGameTimingAccess
    {
    public:
        virtual ClockResult SetTimeScale(float value) noexcept = 0;
        virtual ClockResult SetMinuteDuration(std::uint32_t value) noexcept = 0;

    protected:
        ~NativeGameTimingAccess() = default;
    };
}
