/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep typed builtin object properties on the existing native owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <array>
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
        UprootLimit,
        CollisionDamageMultiplier,
        CollisionDamageEffect,
        CollisionSpecialResponseCase,
        CameraAvoidObject,
        CausesExplosion,
        FxType,
        FxOffsetX,
        FxOffsetY,
        FxOffsetZ,
        SmashMultiplier,
        BreakVelocityX,
        BreakVelocityY,
        BreakVelocityZ,
        BreakVelocityRandomness,
        BreakMode,
        SparksOnImpact,
        Count
    };
    using NativeObjectGroupVectorBits = std::array<std::uint32_t, 3>;
    class NativeObjectGroupPropertiesAccess
    {
    public:
        virtual ~NativeObjectGroupPropertiesAccess() = default;
        virtual ClockResult ReadObjectGroupProperty(std::uint8_t, NativeObjectGroupProperty, float&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult HasOriginalObjectGroupProperties(std::uint8_t, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetObjectGroupProperty(std::uint8_t, NativeObjectGroupProperty, float, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RestoreObjectGroupProperties(std::uint8_t, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RestoreAllObjectGroupProperties() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadObjectGroupPropertyRaw(std::uint8_t, NativeObjectGroupProperty, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        // Enum requests keep their full SDK value for equality before the
        // native byte store narrows them. Boolean callers supply zero or one.
        virtual ClockResult SetObjectGroupPropertyRaw(std::uint8_t, NativeObjectGroupProperty, std::uint32_t, bool&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Vector components are one SDK change, with the actual CVector
        // equality policy and a retained known native prefix if entry stops.
        virtual ClockResult ReadObjectGroupVector(std::uint8_t, NativeObjectGroupProperty, NativeObjectGroupVectorBits&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetObjectGroupVector(std::uint8_t, NativeObjectGroupProperty, const NativeObjectGroupVectorBits&, bool&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
