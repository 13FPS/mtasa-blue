/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Whole aligned64 changes after exact congruent-slot qualification
 *****************************************************************************/
#include "NativeWeatherOverrideAtomic.h"
#include "NativeWeatherOverridePlacement.h"

namespace mta::game_de::NativeWeatherOverrideAtomicProfile
{
    // Placement preserves every displacement byte outside these aligned64
    // windows. All seven complete native groups therefore need no128-bit CPU
    // assumption. Rain's shared serializer direction remains unqualified.
    const std::array<Window, 30> Windows = {{{Field::WetRoads, 0x107e2c8, 0x107e2c8, 8, 8, {0xf3, 0x0f, 0x11, 0x05, 0x24, 0x2d, 0x28, 0x04}},
                                             {Field::WetRoads, 0x107e2f5, 0x107e2f8, 8, 8, {0x05, 0xf7, 0x2c, 0x28, 0x04, 0x66, 0x89, 0x05}},
                                             {Field::WetRoads, 0x107e328, 0x107e328, 8, 8, {0xf3, 0x0f, 0x11, 0x05, 0xc4, 0x2c, 0x28, 0x04}},
                                             {Field::WetRoads, 0x107e35d, 0x107e360, 8, 8, {0x05, 0x8f, 0x2c, 0x28, 0x04, 0x66, 0x89, 0x05}},
                                             {Field::WetRoads, 0x107e388, 0x107e388, 8, 8, {0xf3, 0x0f, 0x11, 0x05, 0x64, 0x2c, 0x28, 0x04}},
                                             {Field::WetRoads, 0x107f889, 0x107f890, 8, 8, {0x04, 0x66, 0x89, 0x35, 0x58, 0x17, 0x28, 0x04}},
                                             {Field::WetRoads, 0x107fd45, 0x107fd48, 10, 8, {0x12, 0x28, 0x04, 0x00, 0x00, 0x80, 0x3f, 0x66}},
                                             {Field::WetRoads, 0x107fd88, 0x107fd88, 8, 8, {0xf3, 0x0f, 0x11, 0x05, 0x64, 0x12, 0x28, 0x04}},
                                             {Field::WetRoads, 0x10806f0, 0x10806f0, 8, 8, {0xf3, 0x0f, 0x11, 0x05, 0xfc, 0x08, 0x28, 0x04}},
                                             {Field::Sandstorm, 0x1146926, 0x1146928, 7, 8, {0x25, 0xf7, 0xd4, 0x1c, 0x04, 0x44, 0x89, 0x25}},
                                             {Field::Sandstorm, 0x1149974, 0x1149978, 6, 8, {0x1c, 0x04, 0x89, 0x2d, 0x5c, 0x25, 0x5e, 0x04}},
                                             {Field::WetRoads, 0x1291c16, 0x1291c18, 9, 8, {0x0f, 0x11, 0x15, 0xd5, 0xf3, 0x06, 0x04, 0x8d}},
                                             {Field::Sandstorm, 0x1291d12, 0x1291d18, 8, 8, {0x08, 0x04, 0x41, 0xb9, 0x6f, 0x6c, 0x06, 0x00}},
                                             {Field::Foggyness, 0x1291d7e, 0x1291d80, 9, 8, {0x0f, 0x11, 0x1d, 0x61, 0xf2, 0x06, 0x04, 0x66}},
                                             {Field::Foggyness, 0x1291d98, 0x1291da0, 9, 8, {0x04, 0x66, 0x41, 0x83, 0xf8, 0x09, 0x74, 0x05}},
                                             {Field::Fog, 0x1291db5, 0x1291db8, 8, 8, {0x05, 0x63, 0x20, 0x08, 0x04, 0x66, 0x83, 0xfa}},
                                             {Field::Fog, 0x1291dc7, 0x1291dc8, 8, 8, {0x0f, 0x11, 0x05, 0x51, 0x20, 0x08, 0x04, 0xb9}},
                                             {Field::Rainbow, 0x1291e7e, 0x1291e80, 8, 8, {0x11, 0x0d, 0x7e, 0xf1, 0x06, 0x04, 0x66, 0x41}},
                                             {Field::WaterFog, 0x1292182, 0x1292188, 8, 8, {0x49, 0x04, 0x73, 0x08, 0x89, 0x1d, 0xca, 0x9d}},
                                             {Field::WaterFog, 0x129218c, 0x1292190, 6, 8, {0x49, 0x04, 0xeb, 0x10, 0x41, 0x0f, 0x2f, 0xc0}},
                                             {Field::WaterFog, 0x129219a, 0x1292198, 10, 8, {0x76, 0x0a, 0xc7, 0x05, 0xb8, 0x9d, 0x49, 0x04}},
                                             {Field::WetRoads, 0x1292609, 0x1292610, 9, 8, {0x06, 0x04, 0xc6, 0x05, 0xf0, 0x08, 0xda, 0x03}},
                                             {Field::WetRoads, 0x12927ab, 0x12927b0, 8, 8, {0xe8, 0x06, 0x04, 0xc3, 0x0f, 0x57, 0xc0, 0xf3}},
                                             {Field::WetRoads, 0x12927b7, 0x12927b8, 8, 8, {0x0f, 0x11, 0x05, 0x35, 0xe8, 0x06, 0x04, 0xc3}},
                                             {Field::RainFog, 0x1292ecd, 0x1292ed0, 8, 8, {0x0d, 0xa3, 0x90, 0x49, 0x04, 0x0f, 0x2f, 0xca}},
                                             {Field::RainFog, 0x1292ee2, 0x1292ee8, 8, 8, {0x49, 0x04, 0x41, 0x0f, 0x2f, 0xca, 0xf3, 0x0f}},
                                             {Field::RainFog, 0x1292efb, 0x1292f00, 8, 8, {0x90, 0x49, 0x04, 0xeb, 0x10, 0x0f, 0x2f, 0xc8}},
                                             {Field::RainFog, 0x1292f0a, 0x1292f10, 8, 8, {0x49, 0x04, 0x0f, 0x28, 0xc8, 0xf3, 0x0f, 0x11}},
                                             {Field::WetRoads, 0x12a3afb, 0x12a3b00, 8, 8, {0xd4, 0x05, 0x04, 0xc3, 0x0f, 0x57, 0xc0, 0xf3}},
                                             {Field::WetRoads, 0x12a3b07, 0x12a3b08, 8, 8, {0x0f, 0x11, 0x05, 0xe5, 0xd4, 0x05, 0x04, 0xc3}}}};
    bool                         Supports(Field field, bool) noexcept
    {
        return NativeWeatherOverrideProfile::Admits(field);
    }
    const Window* Find(std::uintptr_t writerRva) noexcept
    {
        for (const auto& window : Windows)
            if (window.writerRva == writerRva)
                return &window;
        return nullptr;
    }
    bool Build(const Window& window, const Entry& entry, std::array<unsigned char, 16>& output) noexcept
    {
        const auto* constrained = NativeWeatherOverridePlacementProfile::Find(window.writerRva);
        if (!constrained || window.field != constrained->field || window.writerSize != constrained->writerSize || window.windowRva != constrained->windowRva ||
            window.windowSize != 8)
            return false;
        for (std::size_t index = 0; index < window.stock.size(); ++index)
            if (window.stock[index] != (index < constrained->stock.size() ? constrained->stock[index] : 0))
                return false;
        std::array<unsigned char, 8> redirected{};
        if (!NativeWeatherOverridePlacementProfile::Build(*constrained, entry, redirected))
            return false;
        auto changed = window.stock;
        std::memcpy(changed.data(), redirected.data(), redirected.size());
        output = changed;
        return true;
    }
}
