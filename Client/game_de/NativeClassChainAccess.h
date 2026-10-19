/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Guarded value-only native FStructBaseChain subtype observations
 *****************************************************************************/
#pragma once

#include "NativeUiClassMapProfile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    // The caller supplies already qualified UClass identities and read policy.
    // This observation grants no UObject lifetime, GC pin, thread ownership or
    // permission to instantiate a widget or start the native game lifecycle.
    class NativeClassChainAccess final
    {
    public:
        using ReadMemory = bool (*)(std::uintptr_t, void*, std::size_t) noexcept;
        // This bounds diagnostic work; it is not a claimed engine capacity.
        static constexpr std::int32_t maximumIndex = 63;

        explicit NativeClassChainAccess(ReadMemory guardedRead) noexcept : read(guardedRead) {}

        // True means a stable observation, with either relation in output.
        // Refusal preserves output so unreadable/malformed state cannot become
        // a false subtype answer that a later startup decision could trust.
        bool IsChildOf(std::uintptr_t candidateClass, std::uintptr_t expectedBaseClass, bool& output) const noexcept
        {
            Sample first{}, second{};
            if (!Observe(candidateClass, expectedBaseClass, first) || !Observe(candidateClass, expectedBaseClass, second) || !Equal(first, second))
                return false;
            output = second.matches;
            return true;
        }

    private:
        // Read just the native +30 pointer and +38 signed index. Adjacent
        // padding or another UObject's metaclass is not part of this contract.
        using Header = std::array<unsigned char, sizeof(std::uintptr_t) + sizeof(std::int32_t)>;
        static_assert(sizeof(std::uintptr_t) == 8);
        static_assert(NativeUiClassMapProfile::classChainOffset == 0x30 && NativeUiClassMapProfile::classChainIndexOffset == 0x38);
        static_assert(NativeUiClassMapProfile::classChainOffset + sizeof(std::uintptr_t) == NativeUiClassMapProfile::classChainIndexOffset);

        struct Sample
        {
            Header         candidate{}, base{};
            std::uintptr_t selected = 0;
            bool           hasSelected = false, matches = false;
        };

        template <typename T>
        static T Field(const Header& bytes, std::size_t offset) noexcept
        {
            T value{};
            if (offset <= bytes.size() && sizeof(T) <= bytes.size() - offset)
                std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }

        static bool Add(std::uintptr_t base, std::size_t offset, std::uintptr_t& result) noexcept
        {
            if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
                return false;
            result = base + offset;
            return true;
        }

        bool Copy(std::uintptr_t at, void* output, std::size_t bytes) const noexcept
        {
            return read && at && bytes && bytes <= (std::numeric_limits<std::uintptr_t>::max)() - at && read(at, output, bytes);
        }

        bool Observe(std::uintptr_t candidateClass, std::uintptr_t expectedBaseClass, Sample& output) const noexcept
        {
            Sample         next{};
            std::uintptr_t candidateHeader = 0, baseHeader = 0;
            if (!Add(candidateClass, NativeUiClassMapProfile::classChainOffset, candidateHeader) ||
                !Add(expectedBaseClass, NativeUiClassMapProfile::classChainOffset, baseHeader) ||
                !Copy(candidateHeader, next.candidate.data(), next.candidate.size()) || !Copy(baseHeader, next.base.data(), next.base.size()))
                return false;
            const auto candidateIndex = Field<std::int32_t>(next.candidate, sizeof(std::uintptr_t));
            const auto baseIndex = Field<std::int32_t>(next.base, sizeof(std::uintptr_t));
            if (candidateIndex < 0 || candidateIndex > maximumIndex || baseIndex < 0 || baseIndex > maximumIndex)
                return false;
            if (baseIndex <= candidateIndex)
            {
                const auto     chain = Field<std::uintptr_t>(next.candidate, 0);
                std::uintptr_t selectedAddress = 0, repeated = 0;
                const auto     offset = static_cast<std::size_t>(baseIndex) * sizeof(std::uintptr_t);
                if (!Add(chain, offset, selectedAddress) || !Copy(selectedAddress, &next.selected, sizeof(next.selected)) ||
                    !Copy(selectedAddress, &repeated, sizeof(repeated)) || repeated != next.selected)
                    return false;
                next.hasSelected = true;
                // Native compares an FStructBaseChain subobject address, not
                // the UClass pointer or that object's UClass/metaclass field.
                next.matches = next.selected == baseHeader;
            }
            Header candidateAfter{}, baseAfter{};
            if (!Copy(candidateHeader, candidateAfter.data(), candidateAfter.size()) || !Copy(baseHeader, baseAfter.data(), baseAfter.size()) ||
                candidateAfter != next.candidate || baseAfter != next.base)
                return false;
            // This is one bounded indexed array read, never a linked-list walk.
            // The expected base's stored array pointer is not dereferenced.
            output = next;
            return true;
        }

        static bool Equal(const Sample& first, const Sample& second) noexcept
        {
            return first.candidate == second.candidate && first.base == second.base && first.selected == second.selected &&
                   first.hasSelected == second.hasSelected && first.matches == second.matches;
        }

        const ReadMemory read;
    };
}
