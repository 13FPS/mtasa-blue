/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Typed weapon intents for the existing qualified engine owner
 *****************************************************************************/
#pragma once

#include <game/CGameClockService.h>
#include <cstddef>
#include <cstdint>

namespace mta::game_de
{
    enum class NativeWeaponFloatField : std::uint32_t
    {
        WeaponRange,
        TargetRange,
        Accuracy,
        MoveSpeed,
        FiringSpeed,
        Radius,
        LifeSpan,
        Spread,
        AnimBreakoutTime,
        AnimLoopFireTime,
        // Actual CWeaponStat timing setters share the same engine owner.
        AnimLoopStart,
        AnimLoopStop,
        Anim2LoopStart,
        Anim2LoopStop,
        Anim2LoopFireTime
    };
    enum class NativeWeaponShortField : std::uint32_t
    {
        Damage,
        MaximumClipAmmo,
        AimOffsetIndex,
        // This intent uses only the low byte of the final aligned word;
        // the neighboring combos-available byte remains independently owned.
        DefaultCombo,
        // Match the SDK's raw BYTE storage intent while preserving every
        // independently owned DefaultCombo bit in the adjacent low byte.
        CombosAvailable
    };
    enum class NativeWeaponDwordField : std::uint32_t
    {
        Skill,
        RequiredStatLevel,
        Flags,
        AnimGroup,
        FireType,
        Model,
        Model2
    };

    // A read-qualified weapon bank is not a write lease. The existing engine
    // generation owner must admit each synchronous intent before calling the
    // memory primitive. Timing/read-only owners refuse by default rather than
    // impersonating a complete writable CWeaponInfo implementation.
    class NativeWeaponInfoWriteAccess
    {
    public:
        virtual ClockResult SetWeaponFloat(std::size_t, NativeWeaponFloatField, float) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetWeaponShort(std::size_t, NativeWeaponShortField, std::int16_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetWeaponDword(std::size_t, NativeWeaponDwordField, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult UpdateWeaponFlags(std::size_t, std::uint32_t, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWeaponInfoWriteAccess() = default;
    };
}
