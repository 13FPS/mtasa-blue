/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve boundary bytes through congruent private scratch placement
 *****************************************************************************/
#include "NativeWeatherOverridePlacement.h"

namespace mta::game_de::NativeWeatherOverridePlacementProfile
{
    // Unlike an arbitrary near pointer, each group's candidate preserves every
    // displacement byte preceding the selected aligned64 word. The remaining
    // change is atomic even where the complete disp32 crosses aligned16.
    const std::array<Window, 37> Windows = {{
        {Field::WetRoads, 0x107e2c8, 0x107e2c8, 8, 0, {0xf3, 0x0f, 0x11, 0x05, 0x24, 0x2d, 0x28, 0x04}},
        {Field::WetRoads, 0x107e2f5, 0x107e2f8, 8, 0, {0x05, 0xf7, 0x2c, 0x28, 0x04, 0x66, 0x89, 0x05}},
        {Field::WetRoads, 0x107e328, 0x107e328, 8, 0, {0xf3, 0x0f, 0x11, 0x05, 0xc4, 0x2c, 0x28, 0x04}},
        {Field::WetRoads, 0x107e35d, 0x107e360, 8, 0, {0x05, 0x8f, 0x2c, 0x28, 0x04, 0x66, 0x89, 0x05}},
        {Field::WetRoads, 0x107e388, 0x107e388, 8, 0, {0xf3, 0x0f, 0x11, 0x05, 0x64, 0x2c, 0x28, 0x04}},
        {Field::WetRoads, 0x107f889, 0x107f890, 8, 3, {0x04, 0x66, 0x89, 0x35, 0x58, 0x17, 0x28, 0x04}},
        {Field::WetRoads, 0x107fd45, 0x107fd48, 10, 1, {0x12, 0x28, 0x04, 0x00, 0x00, 0x80, 0x3f, 0x66}},
        {Field::WetRoads, 0x107fd88, 0x107fd88, 8, 0, {0xf3, 0x0f, 0x11, 0x05, 0x64, 0x12, 0x28, 0x04}},
        {Field::WetRoads, 0x10806f0, 0x10806f0, 8, 0, {0xf3, 0x0f, 0x11, 0x05, 0xfc, 0x08, 0x28, 0x04}},
        {Field::Sandstorm, 0x1146926, 0x1146928, 7, 0, {0x25, 0xf7, 0xd4, 0x1c, 0x04, 0x44, 0x89, 0x25}},
        {Field::Sandstorm, 0x1149974, 0x1149978, 6, 2, {0x1c, 0x04, 0x89, 0x2d, 0x5c, 0x25, 0x5e, 0x04}},
        {Field::WetRoads, 0x1291c16, 0x1291c18, 9, 0, {0x0f, 0x11, 0x15, 0xd5, 0xf3, 0x06, 0x04, 0x8d}},
        {Field::Sandstorm, 0x1291d12, 0x1291d18, 8, 2, {0x08, 0x04, 0x41, 0xb9, 0x6f, 0x6c, 0x06, 0x00}},
        {Field::Foggyness, 0x1291d7e, 0x1291d80, 9, 0, {0x0f, 0x11, 0x1d, 0x61, 0xf2, 0x06, 0x04, 0x66}},
        {Field::Foggyness, 0x1291d98, 0x1291da0, 9, 3, {0x04, 0x66, 0x41, 0x83, 0xf8, 0x09, 0x74, 0x05}},
        {Field::Fog, 0x1291db5, 0x1291db8, 8, 0, {0x05, 0x63, 0x20, 0x08, 0x04, 0x66, 0x83, 0xfa}},
        {Field::Fog, 0x1291dc7, 0x1291dc8, 8, 0, {0x0f, 0x11, 0x05, 0x51, 0x20, 0x08, 0x04, 0xb9}},
        {Field::Rainbow, 0x1291e7e, 0x1291e80, 8, 0, {0x11, 0x0d, 0x7e, 0xf1, 0x06, 0x04, 0x66, 0x41}},
        {Field::WaterFog, 0x1292182, 0x1292188, 8, 2, {0x49, 0x04, 0x73, 0x08, 0x89, 0x1d, 0xca, 0x9d}},
        {Field::WaterFog, 0x129218c, 0x1292190, 6, 2, {0x49, 0x04, 0xeb, 0x10, 0x41, 0x0f, 0x2f, 0xc0}},
        {Field::WaterFog, 0x129219a, 0x1292198, 10, 0, {0x76, 0x0a, 0xc7, 0x05, 0xb8, 0x9d, 0x49, 0x04}},
        {Field::WetRoads, 0x1292609, 0x1292610, 9, 2, {0x06, 0x04, 0xc6, 0x05, 0xf0, 0x08, 0xda, 0x03}},
        {Field::WetRoads, 0x12927ab, 0x12927b0, 8, 1, {0xe8, 0x06, 0x04, 0xc3, 0x0f, 0x57, 0xc0, 0xf3}},
        {Field::WetRoads, 0x12927b7, 0x12927b8, 8, 0, {0x0f, 0x11, 0x05, 0x35, 0xe8, 0x06, 0x04, 0xc3}},
        {Field::RainFog, 0x1292ecd, 0x1292ed0, 8, 0, {0x0d, 0xa3, 0x90, 0x49, 0x04, 0x0f, 0x2f, 0xca}},
        {Field::RainFog, 0x1292ee2, 0x1292ee8, 8, 2, {0x49, 0x04, 0x41, 0x0f, 0x2f, 0xca, 0xf3, 0x0f}},
        {Field::RainFog, 0x1292efb, 0x1292f00, 8, 1, {0x90, 0x49, 0x04, 0xeb, 0x10, 0x0f, 0x2f, 0xc8}},
        {Field::RainFog, 0x1292f0a, 0x1292f10, 8, 2, {0x49, 0x04, 0x0f, 0x28, 0xc8, 0xf3, 0x0f, 0x11}},
        {Field::WetRoads, 0x12a3afb, 0x12a3b00, 8, 1, {0xd4, 0x05, 0x04, 0xc3, 0x0f, 0x57, 0xc0, 0xf3}},
        {Field::WetRoads, 0x12a3b07, 0x12a3b08, 8, 0, {0x0f, 0x11, 0x05, 0xe5, 0xd4, 0x05, 0x04, 0xc3}},
        {Field::Rain, 0x107fd64, 0x107fd68, 10, 2, {0x29, 0x04, 0x00, 0x00, 0x80, 0x3f, 0xc7, 0x05}},
        {Field::Rain, 0x114691f, 0x1146920, 7, 0, {0x89, 0x25, 0xb2, 0x48, 0x1d, 0x04, 0x44, 0x89}},
        {Field::Rain, 0x114996e, 0x1149970, 6, 0, {0x64, 0x18, 0x1d, 0x04, 0x89, 0x2d, 0xaa, 0xa4}},
        {Field::Rain, 0x1291c91, 0x1291c98, 9, 2, {0x08, 0x04, 0x0f, 0x57, 0xc9, 0x66, 0x83, 0xfa}},
        {Field::Rain, 0x1292487, 0x1292488, 8, 0, {0x0f, 0x11, 0x05, 0x49, 0x8d, 0x08, 0x04, 0x44}},
        {Field::Rain, 0x12c9d40, 0x12c9d40, 6, 0, {0x89, 0x05, 0x92, 0x14, 0x05, 0x04, 0x48, 0x8b}},
        {Field::Rain, 0x12c9d86, 0x12c9d88, 6, 0, {0x4c, 0x14, 0x05, 0x04, 0xeb, 0x23, 0x8b, 0x05}},
    }};
    const Window*                Find(std::uintptr_t rva) noexcept
    {
        for (const auto& window : Windows)
            if (window.writerRva == rva)
                return &window;
        return nullptr;
    }
    ClockResult Placement::Prepare(const Binding& binding, Field field, Placement& output) noexcept
    {
        NativeWeatherOverrideProfile::Range range;
        const auto                          result = NativeWeatherOverrideProfile::Plan::AccumulatorRange(binding, field, range);
        if (result != ClockResult::Ok)
            return result;
        if (binding.image % AllocationBytes)
            return ClockResult::InvalidArgument;
        Placement candidate;
        candidate.image = binding.image;
        const auto nativeField = binding.image + NativeWeatherScalarProfile::Fields[static_cast<std::size_t>(field)];
        candidate.offset = nativeField % AllocationBytes;
        candidate.period = AllocationBytes;
        std::size_t count = 0;
        for (const auto& window : Windows)
            if (window.field == field)
            {
                if (window.unchangedLowBytes > 3 || window.windowRva % 8)
                    return ClockResult::BindingChanged;
                const auto period = static_cast<std::uintptr_t>(1) << (window.unchangedLowBytes * 8);
                if (period > candidate.period)
                    candidate.period = period;
                ++count;
            }
        if (count != NativeWeatherOverrideProfile::WriterCounts[static_cast<std::size_t>(field)] || range.maximum < candidate.offset)
            return ClockResult::BindingChanged;
        candidate.residue = (nativeField - candidate.offset) % candidate.period;
        auto       low = range.minimum > candidate.offset ? range.minimum - candidate.offset : 0;
        auto       high = range.maximum - candidate.offset;
        const auto allocationLimit = (std::numeric_limits<std::uintptr_t>::max)() - AllocationBytes;
        if (high > allocationLimit)
            high = allocationLimit;
        std::uintptr_t first = candidate.residue;
        if (low > first)
        {
            const auto delta = low - first;
            const auto steps = delta / candidate.period + (delta % candidate.period != 0);
            if (steps > ((std::numeric_limits<std::uintptr_t>::max)() - first) / candidate.period)
                return ClockResult::UnsupportedBuild;
            first += steps * candidate.period;
        }
        if (!first)
            first = candidate.period;
        if (first > high)
            return ClockResult::UnsupportedBuild;
        candidate.minimum = first;
        candidate.maximum = high - (high - first) % candidate.period;
        output = candidate;
        return ClockResult::Ok;
    }
    std::size_t Placement::Count() const noexcept
    {
        return !period || minimum > maximum ? 0 : static_cast<std::size_t>((maximum - minimum) / period + 1);
    }
    bool Placement::Accepts(const Candidate& candidate) const noexcept
    {
        if (!period || !candidate.allocation || candidate.allocation < minimum || candidate.allocation > maximum || candidate.allocation % AllocationBytes ||
            candidate.allocation % period != residue || candidate.accumulator != candidate.allocation + offset)
            return false;
        // Reserving arbitrary bytes inside the game image would not create
        // ownership. Only an independent private block is a candidate.
        return candidate.allocation + AllocationBytes <= image || candidate.allocation >= image + NativeWeatherScalarProfile::ImageSize;
    }
    bool Placement::At(std::size_t index, Candidate& output) const noexcept
    {
        if (index >= Count())
            return false;
        Candidate candidate{minimum + index * period, minimum + index * period + offset};
        if (!Accepts(candidate))
            return false;
        output = candidate;
        return true;
    }
    bool Build(const Window& window, const Entry& entry, std::array<unsigned char, 8>& output) noexcept
    {
        const auto& writer = entry.writer;
        if (window.field != writer.field || window.writerRva != writer.rva || window.writerSize != writer.size || window.windowRva % 8 ||
            writer.size > writer.original.size() || writer.displacement > writer.size || writer.size - writer.displacement < 4 || window.unchangedLowBytes > 3)
            return false;
        const auto operand = writer.rva + writer.displacement;
        const auto prefix = window.windowRva > operand ? window.windowRva - operand : 0;
        if (prefix != window.unchangedLowBytes || operand + 4 > window.windowRva + window.stock.size())
            return false;
        auto changed = window.stock;
        for (std::size_t index = 0; index < writer.size; ++index)
        {
            const auto rva = writer.rva + index;
            const bool inside = rva >= window.windowRva && rva - window.windowRva < window.stock.size();
            const bool displacement = index >= writer.displacement && index - writer.displacement < 4;
            if ((!inside || !displacement) && entry.redirected[index] != writer.original[index])
                return false;
            if (inside)
            {
                if (window.stock[rva - window.windowRva] != writer.original[index])
                    return false;
                changed[rva - window.windowRva] = entry.redirected[index];
            }
        }
        output = changed;
        return true;
    }
}
