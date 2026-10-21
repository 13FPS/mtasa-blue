/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Constrained native accumulator placement for atomic64 operand changes
 *****************************************************************************/
#pragma once
#include "NativeWeatherOverridePlan.h"

namespace mta::game_de::NativeWeatherOverridePlacementProfile
{
    using Field = NativeWeatherScalarProfile::Field;
    using Binding = NativeWeatherScalarProfile::Binding;
    using Entry = NativeWeatherOverrideProfile::Plan::Entry;
    inline constexpr std::uintptr_t AllocationBytes = 65536;
    struct Candidate
    {
        std::uintptr_t allocation = 0, accumulator = 0;
    };
    class Placement final
    {
    public:
        // The low16 field offset selects an interior slot in an owned64KiB
        // allocation. A stronger16MiB congruence preserves three low disp32
        // bytes where necessary. No native data padding is borrowed or guessed.
        static ClockResult Prepare(const Binding&, Field, Placement&) noexcept;
        std::uintptr_t     Period() const noexcept { return period; }
        std::uintptr_t     Offset() const noexcept { return offset; }
        std::size_t        Count() const noexcept;
        bool               At(std::size_t index, Candidate&) const noexcept;
        bool               Accepts(const Candidate&) const noexcept;

    private:
        std::uintptr_t image = 0, minimum = 0, maximum = 0, period = 0, offset = 0, residue = 0;
    };
    struct Window
    {
        Field                        field;
        std::uintptr_t               writerRva, windowRva;
        std::size_t                  writerSize, unchangedLowBytes;
        std::array<unsigned char, 8> stock;
        std::uintptr_t               ExclusionBegin() const noexcept { return writerRva < windowRva ? writerRva : windowRva; }
        std::uintptr_t               ExclusionEnd() const noexcept
        {
            const auto writerEnd = writerRva + writerSize, windowEnd = windowRva + stock.size();
            return writerEnd > windowEnd ? writerEnd : windowEnd;
        }
    };
    extern const std::array<Window, 37> Windows;
    const Window*                       Find(std::uintptr_t writerRva) noexcept;
    // Validates the complete instruction, including bytes intentionally left
    // outside the atomic window. Only a congruent prepared accumulator can
    // satisfy that invariant; ordinary arbitrary near allocations cannot.
    bool Build(const Window&, const Entry&, std::array<unsigned char, 8>& redirected) noexcept;
}
