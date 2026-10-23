/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One genuine native grant with preloaded weapon/model admission
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include "NativeWeaponInfoReader.h"
namespace mta::game_de::NativePedWeaponGrantCalls
{
    using Ped = NativePedPoolReader::Value;
    using CheckOwner = NativePedPoolReader::CheckOwner;
    using NativeFunction = std::uint32_t (*)(std::uintptr_t, std::uint32_t, std::uint32_t, std::int32_t);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t&, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t nativePed = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::uint32_t  handle = 0, type = 0, ammo = 0, rawSlot = 0, exceptionCode = 0;
        bool           entered = false, returned = false;
    };
    // This is the native callback behind MTA's existing model preparation, not
    // a replacement for that policy or its still-abstract CWeapon wrapper bank.
    // All potentially consumed native skill rows must already have their models
    // loaded. No CPU skill choice, model request, implicit equip or rollback is
    // added. An entered failure stops the current owner without replay.
    ClockResult Grant(std::uintptr_t image, const Ped&, const NativeWeaponInfoReader&, std::uint32_t fullType, std::uint32_t ammo, std::uint32_t& rawSlot,
                      CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
