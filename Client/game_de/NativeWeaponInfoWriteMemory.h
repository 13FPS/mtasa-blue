/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded exact-field native weapon writes without row restoration
 *****************************************************************************/
#pragma once

#include "NativeWeaponInfoReader.h"
#include "NativeWeaponInfoWriteAccess.h"

namespace mta::game_de::NativeWeaponInfoWriteMemory
{
    using CheckOwner = ClockResult (*)() noexcept;
    // These primitives do not grant an engine callback lease. The qualified
    // NativeWeaponInfoWriteAccess owner must check its generation, engine thread
    // and stopping state first. Successful field writes are persistent SDK
    // intent; cleanup must never restore a whole row over subsequent native work.
    // The real owner supplies its synchronous scope check here as well as at
    // dispatch, because profile copies can outlive a generation or callback.
    // An owned-memory unit fixture may omit it; omission grants no engine lease.
    ClockResult SetFloat(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, NativeWeaponFloatField field, float value,
                         CheckOwner checkOwner = nullptr) noexcept;
    ClockResult SetShort(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, NativeWeaponShortField field, std::int16_t value,
                         CheckOwner checkOwner = nullptr) noexcept;
    ClockResult SetDword(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, NativeWeaponDwordField field, std::uint32_t value,
                         CheckOwner checkOwner = nullptr) noexcept;
    ClockResult UpdateFlags(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, std::uint32_t mask, bool enabled,
                            CheckOwner checkOwner = nullptr) noexcept;
}
