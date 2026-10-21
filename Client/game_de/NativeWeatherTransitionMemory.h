/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrowed checked weather field access without native reset calls
 *****************************************************************************/
#pragma once
#include "NativeWeatherTransitionProfile.h"
#include <array>
#include <cstring>
#include <limits>
#include <intrin.h>
#include <windows.h>

namespace mta::game_de
{
    class NativeWeatherTransitionMemory final
    {
    public:
        using Field = NativeWeatherTransitionProfile::Field;
        using PageCheck = bool (*)(std::uintptr_t, std::size_t, bool, bool) noexcept;
        enum class WriteResult
        {
            Refused,
            Conflict,
            Unchanged,
            Written,
            Unconfirmed
        };
        struct Snapshot
        {
            std::array<std::uint32_t, 5> bits{};
        };
        // This object borrows qualification and a synchronous engine-owner
        // scope. No read/write here is a lifecycle grant. The owner must journal
        // and resolve partial paired mutations before leaving its callback.
        NativeWeatherTransitionMemory(const NativeWeatherTransitionProfile::Binding& qualified, PageCheck pageCheck) noexcept
            : binding(qualified), pages(pageCheck)
        {
        }
        std::uintptr_t ImageIdentity() const noexcept { return binding.image; }

        bool Read(Field field, std::uint32_t& output) const noexcept
        {
            std::uintptr_t target = 0;
            std::size_t    size = 0;
            std::uint32_t  value = 0;
            SIZE_T         copied = 0;
            if (!Range(field, false, target, size) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(target), &value, size, &copied) ||
                copied != size)
                return false;
            output = value;
            return true;
        }

        bool Read(Snapshot& output) const noexcept
        {
            Snapshot first, second;
            for (std::size_t index = 0; index < first.bits.size(); ++index)
                if (!Read(static_cast<Field>(index), first.bits[index]))
                    return false;
            for (std::size_t index = 0; index < second.bits.size(); ++index)
                if (!Read(static_cast<Field>(index), second.bits[index]))
                    return false;
            if (first.bits != second.bits)
                return false;
            output = second;
            return true;
        }

        bool ReadClockSeconds(std::uint16_t& output) const noexcept
        {
            const auto sample = [&](std::array<unsigned char, 2>& value)
            { return ClockByte(NativeWeatherTransitionProfile::MinuteRva, value[0]) && ClockByte(NativeWeatherTransitionProfile::SecondRva, value[1]); };
            std::array<unsigned char, 2> first{}, second{};
            // CClock permits minute60; the native weather updater uses the
            // low byte of seconds. Match that exact clock domain, not wall time
            // or a cached float interpolation from an earlier callback.
            if (!sample(first) || !sample(second) || first != second || second[0] > 60 || second[1] > 59)
                return false;
            output = static_cast<std::uint16_t>(second[0] * 60 + second[1]);
            return true;
        }

        WriteResult WriteIf(Field field, std::uint32_t expected, std::uint32_t replacement) const noexcept
        {
            const auto index = static_cast<std::size_t>(field);
            if (index >= NativeWeatherTransitionProfile::Fields.size() ||
                (NativeWeatherTransitionProfile::Fields[index].size == 2 && ((expected | replacement) > 0xffff)))
                return WriteResult::Refused;
            if (((field == Field::OldType || field == Field::NewType) && replacement >= NativeWeatherTransitionProfile::WeatherTypeCount) ||
                (field == Field::ForcedType && replacement >= NativeWeatherTransitionProfile::WeatherTypeCount && replacement != 0xffff))
                return WriteResult::Refused;
            std::uintptr_t target = 0;
            std::size_t    size = 0;
            std::uint32_t  observed = 0;
            if (!Range(field, true, target, size) || !CompareExchange(target, size, expected, replacement, observed))
                return WriteResult::Refused;
            if (observed != expected)
                return WriteResult::Conflict;
            std::uint32_t readback = 0;
            if (!Read(field, readback) || readback != replacement)
                return WriteResult::Unconfirmed;
            return expected == replacement ? WriteResult::Unchanged : WriteResult::Written;
        }

    private:
        bool ValidBinding() const noexcept
        {
            return binding.image && binding.image <= (std::numeric_limits<std::uintptr_t>::max)() - NativeWeatherTransitionProfile::ImageSize;
        }
        bool Range(Field field, bool writable, std::uintptr_t& address, std::size_t& size) const noexcept
        {
            const auto index = static_cast<std::size_t>(field);
            if (!ValidBinding() || index >= NativeWeatherTransitionProfile::Fields.size())
                return false;
            const auto& info = NativeWeatherTransitionProfile::Fields[index];
            if ((info.size != 2 && info.size != 4) || info.rva > NativeWeatherTransitionProfile::ImageSize ||
                info.size > NativeWeatherTransitionProfile::ImageSize - info.rva)
                return false;
            const auto target = binding.image + info.rva;
            if (!pages || !pages(target, info.size, writable, false))
                return false;
            address = target;
            size = info.size;
            return true;
        }
        bool ClockByte(std::uintptr_t rva, unsigned char& output) const noexcept
        {
            if (!ValidBinding() || rva >= NativeWeatherTransitionProfile::ImageSize || !pages || !pages(binding.image + rva, 1, false, false))
                return false;
            SIZE_T        copied = 0;
            unsigned char value = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(binding.image + rva), &value, 1, &copied) || copied != 1)
                return false;
            output = value;
            return true;
        }
        static bool CompareExchange(std::uintptr_t target, std::size_t size, std::uint32_t expected, std::uint32_t replacement,
                                    std::uint32_t& observed) noexcept
        {
            // Atomic width follows independently proved native instructions.
            // A protection race after page admission remains a refused access;
            // a stale expected value never overwrites a foreign field edit.
            __try
            {
                if (size == 2)
                    observed = static_cast<std::uint16_t>(_InterlockedCompareExchange16(reinterpret_cast<volatile short*>(target),
                                                                                        static_cast<short>(replacement), static_cast<short>(expected)));
                else
                    observed = static_cast<std::uint32_t>(
                        _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(target), static_cast<LONG>(replacement), static_cast<LONG>(expected)));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
        const NativeWeatherTransitionProfile::Binding binding;
        const PageCheck                               pages;
    };
}
