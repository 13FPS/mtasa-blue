/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real CPU CAS boundaries for admitted sparse object properties
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeObjectGroupPropertiesMemory.h"
#include "../../Client/game_de/NativeObjectGroupPropertiesDomain.h"
#include "../../Client/game_de/CObjectGroupPhysicalPropertiesDE.h"
#include <CVector.h>
#include <windows.h>
#include <intrin.h>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>

// Parse the genuine intrinsic before the host selects its include-only macro.
// Faults mutate this owned image's competing source; every observed write is
// the actual production CAS, not a replacement native property operation.
namespace ClockObjectPropertiesExtendedTest
{
    inline std::uintptr_t           row = 0;
    inline std::array<unsigned, 22> attempts{}, accepted{};
    inline std::size_t              raceWord = 0x58, raceByte = 0x58, unknownWord = 0x58;
    inline bool                     injected = false;
    inline LONG                     CompareExchange(volatile LONG* address, LONG desired, LONG expected) noexcept
    {
        const auto at = reinterpret_cast<std::uintptr_t>(address);
        const bool ours = row && at >= row && at - row < 0x58 && (at - row) % sizeof(LONG) == 0;
        const auto offset = ours ? static_cast<std::size_t>(at - row) : std::size_t{0x58};
        if (ours && offset == raceWord)
        {
            // A real competing BYTE changes after the final raw equality
            // fence. The full containing-DWORD CAS must fail, even if the
            // selected logical field's own byte did not change.
            auto* byte = reinterpret_cast<unsigned char*>(row + raceByte);
            *byte ^= 1;
            raceWord = raceByte = 0x58;
            injected = true;
        }
        const LONG result = ::_InterlockedCompareExchange(address, desired, expected);
        if (ours)
        {
            ++attempts[offset / sizeof(LONG)];
            if (result == expected)
            {
                ++accepted[offset / sizeof(LONG)];
                if (offset == unknownWord)
                {
                    // This is an independent source change to opaque FX
                    // representation, never a production pointer transfer.
                    *reinterpret_cast<unsigned char*>(row + 0x30) ^= 1;
                    unknownWord = 0x58;
                    injected = true;
                }
            }
        }
        return result;
    }
}
namespace NativeObjectPropertiesExtendedFixture
{
    namespace P = mta::game_de::NativeObjectGroupPropertiesProfile;
    namespace M = mta::game_de::NativeObjectGroupPropertiesMemory;
    using Field = mta::game_de::NativeObjectGroupProperty;
    inline std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t result = 0;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }
    inline float Float(std::uint32_t bits) noexcept
    {
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    inline std::uint32_t Read(std::uintptr_t row, Field field) noexcept
    {
        const auto    descriptor = P::Descriptors[static_cast<std::size_t>(field)];
        std::uint32_t value = 0;
        std::memcpy(&value, reinterpret_cast<void*>(row + descriptor.offset), descriptor.bytes);
        return value;
    }
    inline void Store(std::uintptr_t row, Field field, std::uint32_t value) noexcept
    {
        const auto descriptor = P::Descriptors[static_cast<std::size_t>(field)];
        std::memcpy(reinterpret_cast<void*>(row + descriptor.offset), &value, descriptor.bytes);
    }
    inline M::Raw Raw(std::uintptr_t row) noexcept
    {
        M::Raw bytes{};
        std::memcpy(bytes.data(), reinterpret_cast<void*>(row), bytes.size());
        return bytes;
    }
    inline bool UnownedSame(const M::Raw& before, const M::Raw& after) noexcept
    {
        for (std::size_t offset = 0; offset != before.size(); ++offset)
        {
            bool owned = false;
            for (const auto field : P::Descriptors)
                if (offset >= field.offset && offset - field.offset < field.bytes)
                    owned = true;
            if (!owned && before[offset] != after[offset])
                return false;
        }
        return true;
    }
}
