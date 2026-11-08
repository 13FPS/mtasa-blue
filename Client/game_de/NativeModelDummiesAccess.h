/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same-owner dummy vectors and cached originals without leaked holds
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <array>
#include <cstdint>

namespace mta::game_de
{
    struct NativeModelDummyVector
    {
        float x = 0, y = 0, z = 0;
    };
    using NativeModelDummyPositions = std::array<NativeModelDummyVector, 14>;
    class NativeModelDummiesAccess
    {
    public:
        virtual ~NativeModelDummiesAccess() = default;
        // Old capabilities refuse rather than inventing zero vectors, cache
        // misses or a completed reset. Private reference tokens never escape
        // the retained Runtime domain through this SDK-facing interface.
        virtual ClockResult ReadModelVehicleDummyPositions(std::uint32_t, NativeModelDummyPositions&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadCachedModelVehicleDummyDefault(std::uint32_t, std::uint32_t, NativeModelDummyVector&, bool&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetModelVehicleDummyPosition(std::uint32_t, std::uint32_t, const NativeModelDummyVector&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ResetModelVehicleDummies(std::uint32_t, bool) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ResetAllModelVehicleDummies() noexcept { return ClockResult::NotInitialized; }
    };
}
