/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read current native weather floats without inventing override state
 *****************************************************************************/
#pragma once
#include "NativeWeatherScalarProfile.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <windows.h>

namespace mta::game_de
{
    class NativeWeatherScalarMemory final
    {
    public:
        using Field = NativeWeatherScalarProfile::Field;
        using PageCheck = bool (*)(std::uintptr_t, std::size_t, bool, bool) noexcept;
        NativeWeatherScalarMemory(const NativeWeatherScalarProfile::Binding& qualified, PageCheck policy) noexcept : binding(qualified), pages(policy) {}
        bool Read(Field field, float& output) const noexcept
        {
            const auto index = static_cast<std::size_t>(field);
            if (!binding.image || binding.image > (std::numeric_limits<std::uintptr_t>::max)() - NativeWeatherScalarProfile::ImageSize ||
                index >= NativeWeatherScalarProfile::Fields.size())
                return false;
            const auto rva = NativeWeatherScalarProfile::Fields[index];
            if (rva > NativeWeatherScalarProfile::ImageSize || sizeof(float) > NativeWeatherScalarProfile::ImageSize - rva || !pages ||
                !pages(binding.image + rva, sizeof(float), false, false))
                return false;
            std::uint32_t value = 0;
            SIZE_T        copied = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(binding.image + rva), &value, sizeof(value), &copied) ||
                copied != sizeof(value))
                return false;
            // Classic getters return the engine's actual float. Preserve its
            // bits rather than normalizing to0..1, caching a previous frame or
            // silently publishing0 when access is refused.
            static_assert(sizeof(float) == sizeof(value));
            float actual = 0;
            std::memcpy(&actual, &value, sizeof(value));
            if (!std::isfinite(actual))
                return false;
            std::memcpy(&output, &value, sizeof(value));
            return true;
        }

    private:
        const NativeWeatherScalarProfile::Binding binding;
        const PageCheck                           pages;
    };
}
