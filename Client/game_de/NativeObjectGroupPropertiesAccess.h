/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep supported builtin object properties on the existing native owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>

namespace mta::game_de
{
    enum class NativeObjectGroupProperty : std::uint32_t
    {
        Mass,
        TurnMass,
        AirResistance,
        Elasticity,
        Buoyancy,
        Count
    };
    class NativeObjectGroupPropertiesAccess
    {
    public:
        virtual ~NativeObjectGroupPropertiesAccess() = default;
        virtual ClockResult ReadObjectGroupProperty(std::uint8_t, NativeObjectGroupProperty, float&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult HasOriginalObjectGroupProperties(std::uint8_t, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetObjectGroupProperty(std::uint8_t, NativeObjectGroupProperty, float, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RestoreObjectGroupProperties(std::uint8_t, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RestoreAllObjectGroupProperties() noexcept { return ClockResult::NotInitialized; }
    };
}
