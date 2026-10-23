/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One genuine ped ClearWeapon operation, without wrapper-bank publication
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include "NativeWeaponInfoReader.h"
#include <windows.h>
namespace mta::game_de::NativePedWeaponClearCalls
{
    using Ped = NativePedPoolReader::Value;
    using CheckOwner = NativePedPoolReader::CheckOwner;
    using NativeFunction = std::uint8_t* (*)(std::uintptr_t, std::uint32_t);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t, std::uint32_t, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t nativePed = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::uint32_t  handle = 0, type = 0, exceptionCode = 0;
        std::int32_t   selectedSlot = -1;
        bool           matched = false, maySwitch = false, mayAttach = false, entered = false, returned = false;
    };
    // The actual SDK ignores the native pointer return. One entry owns its
    // selection/model/actor/goggles effects; entered failure stops the Runtime
    // with no replay or rollback and no CWeapon wrapper lookup.
    ClockResult Clear(std::uintptr_t, const Ped&, const NativeWeaponInfoReader&, std::uint32_t, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
