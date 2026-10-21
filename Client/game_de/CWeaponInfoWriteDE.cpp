/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual SDK setter intent without unqualified native writes
 *****************************************************************************/
#include "CWeaponInfoWriteDE.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CWeaponInfoWriteDE::RequireWrite(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE CWeaponInfo ") + operation + " unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) +
                                     ")");
    }

    void CWeaponInfoWriteDE::SetWeaponRange(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::WeaponRange, value), "SetWeaponRange");
    }

    void CWeaponInfoWriteDE::SetTargetRange(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::TargetRange, value), "SetTargetRange");
    }

    void CWeaponInfoWriteDE::SetAccuracy(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::Accuracy, value), "SetAccuracy");
    }

    void CWeaponInfoWriteDE::SetMoveSpeed(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::MoveSpeed, value), "SetMoveSpeed");
    }

    void CWeaponInfoWriteDE::SetFiringSpeed(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::FiringSpeed, value), "SetFiringSpeed");
    }

    void CWeaponInfoWriteDE::SetRadius(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::Radius, value), "SetRadius");
    }

    void CWeaponInfoWriteDE::SetLifeSpan(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::LifeSpan, value), "SetLifeSpan");
    }

    void CWeaponInfoWriteDE::SetSpread(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::Spread, value), "SetSpread");
    }

    void CWeaponInfoWriteDE::SetAnimBreakoutTime(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::AnimBreakoutTime, value), "SetAnimBreakoutTime");
    }

    void CWeaponInfoWriteDE::SetWeaponAnimLoopFireTime(float value)
    {
        RequireWrite(native.SetWeaponFloat(sdkSlot, NativeWeaponFloatField::AnimLoopFireTime, value), "SetWeaponAnimLoopFireTime");
    }

    void CWeaponInfoWriteDE::SetDamagePerHit(short value)
    {
        RequireWrite(native.SetWeaponShort(sdkSlot, NativeWeaponShortField::Damage, value), "SetDamagePerHit");
    }

    void CWeaponInfoWriteDE::SetMaximumClipAmmo(short value)
    {
        RequireWrite(native.SetWeaponShort(sdkSlot, NativeWeaponShortField::MaximumClipAmmo, value), "SetMaximumClipAmmo");
    }

    void CWeaponInfoWriteDE::SetSkill(eWeaponSkill value)
    {
        // Native loader and consumers establish only poor/std/pro/special.
        // Lookup's legacy unknown-skill fallback does not authorize storing it.
        if (static_cast<std::uint32_t>(value) > 3)
            RequireWrite(ClockResult::InvalidArgument, "SetSkill");
        RequireWrite(native.SetWeaponDword(sdkSlot, NativeWeaponDwordField::Skill, static_cast<std::uint32_t>(value)), "SetSkill");
    }

    void CWeaponInfoWriteDE::SetRequiredStatLevel(float value)
    {
        // Widen before checking: float(INT_MAX) rounds up to 2147483648.
        // Refuse non-finite/out-of-range inputs before the SDK's truncating cast.
        const double widened = value;
        if (!std::isfinite(widened) || widened < static_cast<double>((std::numeric_limits<std::int32_t>::min)()) ||
            widened > static_cast<double>((std::numeric_limits<std::int32_t>::max)()))
            RequireWrite(ClockResult::InvalidArgument, "SetRequiredStatLevel");
        const auto integer = static_cast<std::int32_t>(value);
        RequireWrite(native.SetWeaponDword(sdkSlot, NativeWeaponDwordField::RequiredStatLevel, static_cast<std::uint32_t>(integer)), "SetRequiredStatLevel");
    }

    void CWeaponInfoWriteDE::SetFlags(int value)
    {
        static_assert(sizeof(value) == sizeof(std::uint32_t));
        RequireWrite(native.SetWeaponDword(sdkSlot, NativeWeaponDwordField::Flags, static_cast<std::uint32_t>(value)), "SetFlags");
    }

    void CWeaponInfoWriteDE::SetFlag(DWORD mask)
    {
        RequireWrite(native.UpdateWeaponFlags(sdkSlot, mask, true), "SetFlag");
    }

    void CWeaponInfoWriteDE::ClearFlag(DWORD mask)
    {
        RequireWrite(native.UpdateWeaponFlags(sdkSlot, mask, false), "ClearFlag");
    }

    void CWeaponInfoWriteDE::SetAnimGroup(DWORD value)
    {
        RequireWrite(native.SetWeaponDword(sdkSlot, NativeWeaponDwordField::AnimGroup, value), "SetAnimGroup");
    }
}
