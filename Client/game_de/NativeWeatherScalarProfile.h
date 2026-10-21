/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact native producers for the existing eight MTA weather getters
 *****************************************************************************/
#pragma once
#include "NativeWeatherTransitionProfile.h"

namespace mta::game_de::NativeWeatherScalarProfile
{
    enum class Field : std::uint8_t
    {
        Rain,
        WetRoads,
        Foggyness,
        Fog,
        RainFog,
        WaterFog,
        Sandstorm,
        Rainbow
    };
    using Binding = NativeWeatherTransitionProfile::Binding;
    inline constexpr auto ImageSize = NativeWeatherTransitionProfile::ImageSize;
    // Heat-haze control at531b1dc and headlight spectrum at5301000 are
    // different native effects. RainFog is the AddRain haze alpha; WaterFog is
    // the control generated from the timecycle water-fog alpha/fade state.
    inline constexpr std::array<std::uintptr_t, 8> Fields = {0x531b1d8, 0x5300ff4, 0x5300fe8, 0x5313e20, 0x572bf78, 0x572bf5c, 0x5313e24, 0x5301004};
    struct Pin
    {
        std::uintptr_t                 rva;
        std::size_t                    size;
        std::array<unsigned char, 192> bytes;
    };
    extern const std::array<Pin, 13> Code;
    struct NamedValue
    {
        std::uintptr_t nameRva, recordRva;
        std::uint32_t  value;
        const char*    name;
        bool           propertyOffset;
    };
    inline constexpr std::array<NamedValue, 7> Names = {{{0x4251dd0, 0x42532a0, 9, "FoggySF", false},
                                                         {0x4251f88, 0x4253340, 19, "SandstormDesert", false},
                                                         {0x4251dd8, 0x4253290, 8, "RainySF", false},
                                                         {0x4251f78, 0x4253310, 16, "RainyCountry", false},
                                                         {0x4278fc0, 0x427aab0, 0x26a8, "RainyAmount", true},
                                                         {0x4279180, 0x427a9c0, 0x26b0, "SandstormAmount", true},
                                                         {0x4279238, 0x427a360, 0x27c0, "WetRoadActor2", true}}};
    inline constexpr std::uintptr_t            RainbowIntensityNameRva = 0x431a4f0, RainbowMeshNameRva = 0x431a750;
    bool                                       Qualify(Binding&) noexcept;
    bool                                       Revalidate(const Binding&) noexcept;
}
