/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked read-only access to DE's independently decoded weapon bank
 *****************************************************************************/
#pragma once

#include <game/CGameAdapter.h>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace mta::game_de
{
    // This layout is independently established by DE's own initialization and
    // weapon.dat loader stores. It contains no classic native type or pointer.
    struct NativeWeaponInfoValues
    {
        std::int32_t  fireType = 0;
        float         targetRange = 0, weaponRange = 0;
        std::int32_t  model = 0, model2 = 0, slot = 0, flags = 0;
        std::uint32_t animGroup = 0;
        std::int16_t  clipAmmo = 0, damage = 0;
        float         fireOffset[3]{};
        std::uint32_t skill = 0;
        std::int32_t  requiredStat = 0;
        float         accuracy = 0, moveSpeed = 0;
        float         animLoopStart = 0, animLoopEnd = 0, animFireTime = 0;
        float         anim2LoopStart = 0, anim2LoopEnd = 0, anim2FireTime = 0, animBreakout = 0;
        float         speed = 0, radius = 0, lifeSpan = 0, spread = 0;
        std::int16_t  aimOffset = 0;
        std::uint8_t  defaultCombo = 0, combosAvailable = 0;
    };
    static_assert(sizeof(NativeWeaponInfoValues) == 112 && std::is_standard_layout_v<NativeWeaponInfoValues> &&
                  std::is_trivially_copyable_v<NativeWeaponInfoValues>);
    static_assert(offsetof(NativeWeaponInfoValues, targetRange) == 0x04 && offsetof(NativeWeaponInfoValues, weaponRange) == 0x08 &&
                  offsetof(NativeWeaponInfoValues, model) == 0x0c && offsetof(NativeWeaponInfoValues, model2) == 0x10 &&
                  offsetof(NativeWeaponInfoValues, slot) == 0x14 && offsetof(NativeWeaponInfoValues, flags) == 0x18 &&
                  offsetof(NativeWeaponInfoValues, animGroup) == 0x1c && offsetof(NativeWeaponInfoValues, clipAmmo) == 0x20 &&
                  offsetof(NativeWeaponInfoValues, damage) == 0x22 && offsetof(NativeWeaponInfoValues, fireOffset) == 0x24 &&
                  offsetof(NativeWeaponInfoValues, skill) == 0x30 && offsetof(NativeWeaponInfoValues, requiredStat) == 0x34 &&
                  offsetof(NativeWeaponInfoValues, accuracy) == 0x38 && offsetof(NativeWeaponInfoValues, moveSpeed) == 0x3c &&
                  offsetof(NativeWeaponInfoValues, animLoopStart) == 0x40 && offsetof(NativeWeaponInfoValues, animLoopEnd) == 0x44 &&
                  offsetof(NativeWeaponInfoValues, animFireTime) == 0x48 && offsetof(NativeWeaponInfoValues, anim2LoopStart) == 0x4c &&
                  offsetof(NativeWeaponInfoValues, anim2LoopEnd) == 0x50 && offsetof(NativeWeaponInfoValues, anim2FireTime) == 0x54 &&
                  offsetof(NativeWeaponInfoValues, animBreakout) == 0x58 && offsetof(NativeWeaponInfoValues, speed) == 0x5c &&
                  offsetof(NativeWeaponInfoValues, radius) == 0x60 && offsetof(NativeWeaponInfoValues, lifeSpan) == 0x64 &&
                  offsetof(NativeWeaponInfoValues, spread) == 0x68 && offsetof(NativeWeaponInfoValues, aimOffset) == 0x6c &&
                  offsetof(NativeWeaponInfoValues, defaultCombo) == 0x6e && offsetof(NativeWeaponInfoValues, combosAvailable) == 0x6f);

    class NativeWeaponInfoReader final
    {
    public:
        Status Bind() noexcept;
        Status Read(std::size_t sdkSlot, NativeWeaponInfoValues& output) const noexcept;
        void   Reset() noexcept;
        bool   IsBound() const noexcept;

    private:
        // Binding authorizes this thread's checked observations only. It does
        // not invoke the native selector/loader or grant weapon write ownership.
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
