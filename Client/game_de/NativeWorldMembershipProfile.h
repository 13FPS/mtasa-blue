/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Enumerate the actual normal/repeat/LOD and moving world heads
 ****************************************************************************/
#pragma once
#include "NativeWorldAddProfile.h"
#include "NativePedOrientationProfile.h"
#include <array>
namespace mta::game_de::NativeWorldMembershipProfile
{
    static_assert(sizeof(std::uintptr_t) == 8, "The qualified DE head/node layouts contain native64 pointers");
    // Complete 1131ff0 indexes these finite banks. Repeat cells intentionally
    // alias logical sectors through x/y & 15; each physical head is read once,
    // while every distinct node in it remains a separate membership.
    inline constexpr std::uintptr_t NormalRva = NativeWorldAddProfile::SectorsRva, RepeatRva = 0x5423ea0, LodRva = NativeWorldAddProfile::LodRva,
                                    MovingRva = 0x53d3b18;
    inline constexpr std::size_t NormalSide = NativeWorldAddProfile::SectorSide, RepeatSide = 16, LodSide = NativeWorldAddProfile::LodSide;
    inline constexpr std::size_t NormalHeads = NormalSide * NormalSide * 2, RepeatHeads = RepeatSide * RepeatSide * 3, LodHeads = LodSide * LodSide;
    inline constexpr std::size_t TotalHeads = NormalHeads + RepeatHeads + LodHeads + 1;
    static_assert(NormalRva + NormalHeads * sizeof(std::uint64_t) == RepeatRva);
    static_assert(RepeatRva + RepeatHeads * sizeof(std::uint64_t) == LodRva);
    // The wrapper skips moving insertion for types1/5. Still observe the
    // independently qualified singleton, so unexpected membership is visible.
    inline constexpr std::array<NativePedOrientationProfile::Pin, 2> MovingCode = {{
        {0x115d580, NativePedOrientationProfile::MovingAdd.data(), NativePedOrientationProfile::MovingAdd.size()},
        {0x115d5f0, NativePedOrientationProfile::MovingRemove.data(), NativePedOrientationProfile::MovingRemove.size()},
    }};
}
