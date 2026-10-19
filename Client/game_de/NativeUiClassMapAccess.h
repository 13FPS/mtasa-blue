/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded observations of DE's configured startup UI class dictionary
 *****************************************************************************/
#pragma once

#include "NativeUiClassMapProfile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>

namespace mta::game_de
{
    // The qualified native owner supplies the map address and guarded read
    // policy. Like NativePadStateAccess, this helper grants no engine thread,
    // UObject lifetime, class-type or lifecycle-call authority of its own.
    class NativeUiClassMapAccess final
    {
    public:
        enum class Key
        {
            Loading,
            SimpleFade
        };

        struct Value
        {
            std::uintptr_t classAddress = 0;
            bool           keyPresent = false;
        };

        using ReadMemory = bool (*)(std::uintptr_t, void*, std::size_t) noexcept;

        NativeUiClassMapAccess(std::uintptr_t mapAddress, ReadMemory guardedRead) noexcept : address(mapAddress), read(guardedRead) {}

        bool Lookup(Key key, Value& output) const noexcept
        {
            if (key != Key::Loading && key != Key::SimpleFade)
                return false;
            Sample first{}, second{};
            if (!Observe(key, first) || !Observe(key, second) || !Equal(first, second))
                return false;
            // A matching null value is distinct from an absent key. Neither
            // result establishes that a class is a live UITransitionScreen or
            // that native startup can safely invoke its dynamic providers.
            output = second.value;
            return true;
        }

    private:
        // These are conservative observation budgets, not native capacities.
        // A malformed/cyclic map must not stall the application's frame owner.
        static constexpr std::size_t maximumSlots = 4096, maximumBuckets = 4096, maximumChain = 64, maximumKeyUnits = 11;
        using Header = std::array<unsigned char, NativeUiClassMapProfile::headerBytes>;
        using Entry = std::array<unsigned char, NativeUiClassMapProfile::entryBytes>;
        static_assert(sizeof(std::uintptr_t) == 8 && sizeof(char16_t) == 2);
        static_assert(NativeUiClassMapProfile::slotsOffset + sizeof(std::uintptr_t) <= std::tuple_size_v<Header> &&
                      NativeUiClassMapProfile::slotCountOffset + sizeof(std::int32_t) <= std::tuple_size_v<Header> &&
                      NativeUiClassMapProfile::freeCountOffset + sizeof(std::int32_t) <= std::tuple_size_v<Header> &&
                      NativeUiClassMapProfile::inlineHashOffset + sizeof(std::int32_t) <= std::tuple_size_v<Header> &&
                      NativeUiClassMapProfile::secondaryHashOffset + sizeof(std::uintptr_t) <= std::tuple_size_v<Header> &&
                      NativeUiClassMapProfile::hashSizeOffset + sizeof(std::int32_t) <= std::tuple_size_v<Header>);
        static_assert(NativeUiClassMapProfile::keyDataOffset + sizeof(std::uintptr_t) <= std::tuple_size_v<Entry> &&
                      NativeUiClassMapProfile::keyCountOffset + sizeof(std::int32_t) <= std::tuple_size_v<Entry> &&
                      NativeUiClassMapProfile::valueOffset + sizeof(std::uintptr_t) <= std::tuple_size_v<Entry> &&
                      NativeUiClassMapProfile::nextOffset + sizeof(std::int32_t) <= std::tuple_size_v<Entry>);
        static_assert(NativeUiClassMapProfile::keyLoadingCount == sizeof(u"Loading") / sizeof(char16_t) &&
                      NativeUiClassMapProfile::keySimpleFadeCount == sizeof(u"SimpleFade") / sizeof(char16_t));

        struct Node
        {
            std::int32_t                          index = -1;
            Entry                                 entry{};
            std::array<char16_t, maximumKeyUnits> key{};
        };

        struct Sample
        {
            Header                         header{};
            std::int32_t                   bucket = -1;
            std::size_t                    count = 0;
            std::array<Node, maximumChain> path{};
            Value                          value{};
        };

        template <typename T, std::size_t N>
        static T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            if (offset <= N && sizeof(T) <= N - offset)
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

        static char16_t Fold(char16_t value) noexcept
        {
            // The pinned native comparison table folds ASCII A..Z to a..z.
            // Differing non-ASCII units remain unequal in the native finder.
            return value >= u'A' && value <= u'Z' ? static_cast<char16_t>(value + (u'a' - u'A')) : value;
        }

        bool Observe(Key key, Sample& output) const noexcept
        {
            namespace Profile = NativeUiClassMapProfile;
            Sample next{};
            if (!Copy(address, next.header.data(), next.header.size()))
                return false;
            const auto slots = Field<std::uintptr_t>(next.header, Profile::slotsOffset);
            const auto count = Field<std::int32_t>(next.header, Profile::slotCountOffset);
            const auto freeCount = Field<std::int32_t>(next.header, Profile::freeCountOffset);
            if (count < 0 || static_cast<std::size_t>(count) > maximumSlots || freeCount < 0 || freeCount > count)
                return false;
            if (count != freeCount)
            {
                const auto buckets = Field<std::int32_t>(next.header, Profile::hashSizeOffset);
                const auto secondary = Field<std::uintptr_t>(next.header, Profile::secondaryHashOffset);
                if (!slots || buckets <= 0 || static_cast<std::size_t>(buckets) > maximumBuckets || (buckets & (buckets - 1)) || (!secondary && buckets != 1))
                    return false;
                std::uintptr_t table = secondary, bucketAddress = 0;
                if (!table && !Add(address, Profile::inlineHashOffset, table))
                    return false;
                const auto hash = key == Key::Loading ? Profile::keyLoadingHash : Profile::keySimpleFadeHash;
                const auto bucket = static_cast<std::size_t>(hash & static_cast<std::uint32_t>(buckets - 1));
                if (!Add(table, bucket * sizeof(std::int32_t), bucketAddress) || !Copy(bucketAddress, &next.bucket, sizeof(next.bucket)))
                    return false;
                const char16_t*   wanted = key == Key::Loading ? u"Loading" : u"SimpleFade";
                const std::size_t units = key == Key::Loading ? Profile::keyLoadingCount : Profile::keySimpleFadeCount;
                auto              index = next.bucket;
                while (index != -1)
                {
                    if (index < 0 || index >= count || next.count == maximumChain)
                        return false;
                    for (std::size_t prior = 0; prior < next.count; ++prior)
                        if (next.path[prior].index == index)
                            return false;
                    auto& node = next.path[next.count++];
                    node.index = index;
                    std::uintptr_t element = 0;
                    if (!Add(slots, static_cast<std::size_t>(index) * Profile::entryBytes, element) || !Copy(element, node.entry.data(), node.entry.size()))
                        return false;
                    const auto keyCount = Field<std::int32_t>(node.entry, Profile::keyCountOffset);
                    if (keyCount < 0)
                        return false;
                    if (static_cast<std::size_t>(keyCount) == units)
                    {
                        const auto text = Field<std::uintptr_t>(node.entry, Profile::keyDataOffset);
                        if (!Copy(text, node.key.data(), units * sizeof(char16_t)) || node.key[units - 1] != 0)
                            return false;
                        bool match = true;
                        for (std::size_t unit = 0; unit + 1 < units; ++unit)
                        {
                            if (!node.key[unit])
                                return false;
                            if (Fold(node.key[unit]) != Fold(wanted[unit]))
                                match = false;
                        }
                        if (match)
                        {
                            next.value = {Field<std::uintptr_t>(node.entry, Profile::valueOffset), true};
                            break;
                        }
                    }
                    index = Field<std::int32_t>(node.entry, Profile::nextOffset);
                }
            }
            Header after{};
            if (!Copy(address, after.data(), after.size()) || after != next.header)
                return false;
            // Refuse boundedly on sparse chains or allocation movement. Reading
            // raw headers does not infer an unproved allocator capacity field.
            output = next;
            return true;
        }

        static bool Equal(const Sample& first, const Sample& second) noexcept
        {
            if (first.header != second.header || first.bucket != second.bucket || first.count != second.count ||
                first.value.classAddress != second.value.classAddress || first.value.keyPresent != second.value.keyPresent)
                return false;
            for (std::size_t index = 0; index < first.count; ++index)
                if (first.path[index].index != second.path[index].index || first.path[index].entry != second.path[index].entry ||
                    first.path[index].key != second.path[index].key)
                    return false;
            return true;
        }

        const std::uintptr_t address;
        const ReadMemory     read;
    };
}
