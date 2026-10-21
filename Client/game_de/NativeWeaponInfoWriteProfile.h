/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Proven live animation-index bound for DE weapon mutation
 *****************************************************************************/
#pragma once

#include <array>
#include <cstdint>

namespace mta::game_de::NativeWeaponInfoWriteProfile
{
    // The pinned weapon.dat loader loads this signed count, searches indices
    // [0,count), then stores its selected index at weapon row+0x1c. Checking the
    // actual count avoids treating every DWORD as a safe animation-bank index.
    // Supported image SHA/profile are enforced by NativeWeaponInfoReader first.
    inline constexpr std::uintptr_t               AnimationCountRva = 0x51afc64;
    inline constexpr std::uintptr_t               AnimationCountLoadRva = 0x114fb10;
    inline constexpr std::array<unsigned char, 7> AnimationCountLoad{0x44, 0x8b, 0x15, 0x4d, 0x01, 0x06, 0x04};
    static_assert(AnimationCountLoadRva + AnimationCountLoad.size() + 0x0406014d == AnimationCountRva);
}
