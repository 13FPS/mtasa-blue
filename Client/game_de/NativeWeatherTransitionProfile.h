/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact native weather-transition storage for the existing MTA SDK
 *****************************************************************************/
#pragma once
#include "NativeClockProfile.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativeWeatherTransitionProfile
{
    enum class Field : std::uint8_t
    {
        OldType,
        NewType,
        ForcedType,
        PreviousClockSeconds,
        InterpolationBits
    };
    struct FieldInfo
    {
        std::uintptr_t rva;
        std::size_t    size;
    };
    inline constexpr std::uintptr_t ImageSize = NativeClockProfile::imageSize;
    // Native force/release handlers and weather progression independently use
    // WORDs. Reusing the classic one-byte release would leave a nonnegative
    // forced type instead of the native signed -1 sentinel.
    inline constexpr std::array<FieldInfo, 5> Fields = {{{0x5300ff0, 2}, {0x5300fec, 2}, {0x5301008, 2}, {0x5314500, 2}, {0x5300dbc, 4}}};
    inline constexpr std::uintptr_t           MinuteRva = NativeClockProfile::minutesRva, SecondRva = NativeClockProfile::secondsRva;
    // The actual TIMECYC loader initializes23 weather columns and8 hour rows;
    // its independent consumer indexes hour*23+weather without clamping.
    // Extra-colour columns21/22 remain valid; the enum's MAX entry is not data.
    inline constexpr std::uint32_t  WeatherTypeCount = 23, HourCount = 8;
    inline constexpr std::uintptr_t TableRva = 0x523ae20;
    struct Pin
    {
        std::uintptr_t                 rva;
        std::size_t                    size;
        std::array<unsigned char, 160> bytes;
    };
    extern const std::array<Pin, 12> Code;
    struct Binding
    {
        std::uintptr_t image = 0;
    };
    bool Qualify(Binding& output) noexcept;
    bool Revalidate(const Binding& binding) noexcept;
}
