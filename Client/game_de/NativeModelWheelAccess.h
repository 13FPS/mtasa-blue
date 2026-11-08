/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route real SDK wheel defaults through the existing native owner
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>

namespace mta::game_de
{
    struct NativeModelWheelSizes
    {
        float front = 0.0f, rear = 0.0f;
    };
    enum class NativeModelWheelGroup : std::uint32_t
    {
        Front,
        Rear,
        All = 0xff
    };
    class NativeModelWheelAccess
    {
    public:
        virtual ~NativeModelWheelAccess() = default;
        // Defaults and native write uncertainty belong to this same owner;
        // an older implementation cannot manufacture a reset acknowledgement.
        virtual ClockResult ReadModelWheelSizes(std::uint32_t, NativeModelWheelSizes&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetModelWheelSize(std::uint32_t, NativeModelWheelGroup, float) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ResetModelWheelSizes(std::uint32_t, const NativeModelWheelSizes*) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ResetAllModelWheelSizes() noexcept { return ClockResult::NotInitialized; }
    };
}
