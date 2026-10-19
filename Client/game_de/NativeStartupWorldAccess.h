/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Guarded observations of native startup world prerequisites
 *****************************************************************************/
#pragma once

#include "NativeClassChainAccess.h"
#include "NativeStartupWorldProfile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    // The caller qualifies the profile, live owner and native callback. This
    // observes a conservative prerequisite; it does not emulate GetWorld's
    // nullable result or grant GTAGameMode/UWorld type, GC or startup authority.
    class NativeStartupWorldAccess final
    {
    public:
        using ReadMemory = NativeClassChainAccess::ReadMemory;

        struct Value
        {
            std::uintptr_t outerAddress = 0, levelAddress = 0, worldAddress = 0;
        };

        NativeStartupWorldAccess(std::uintptr_t imageBase, ReadMemory guardedRead) noexcept : image(imageBase), read(guardedRead) {}

        // A native early-null result is a refused prerequisite here. Keep the
        // caller's previous value on every malformed, moving or unreadable path.
        bool Read(std::uintptr_t ownerAddress, Value& output) const noexcept
        {
            Sample first{}, second{};
            if (!Observe(ownerAddress, first) || !Observe(ownerAddress, second) || !Equal(first, second))
                return false;
            output = second.value;
            return true;
        }

    private:
        // A corrupt outer chain must not stall the application's frame owner.
        // This is a diagnostic work budget, not a native object/registry limit.
        static constexpr std::size_t maximumOuterDepth = 64;
        using ClassHeader = std::array<unsigned char, sizeof(std::uintptr_t) + sizeof(std::int32_t)>;
        using ItemHeader = std::array<unsigned char, sizeof(std::uintptr_t) + sizeof(std::uint32_t)>;
        static_assert(sizeof(std::uintptr_t) == 8);
        static_assert(NativeStartupWorldProfile::objectItemBytes == 24 && NativeStartupWorldProfile::itemFlagsOffset == sizeof(std::uintptr_t));
        static_assert(NativeUiClassMapProfile::classChainOffset + sizeof(std::uintptr_t) == NativeUiClassMapProfile::classChainIndexOffset);

        struct ClassState
        {
            ClassHeader    candidate{}, base{};
            std::uintptr_t selectedAddress = 0, selected = 0;
            bool           hasSelected = false;
        };
        struct Node
        {
            std::uintptr_t address = 0, classAddress = 0, outerAddress = 0;
            ClassState     type{};
            bool           matches = false;
        };
        struct Sample
        {
            Value                               value{};
            std::uintptr_t                      getter = 0, levelClass = 0, chunks = 0, chunkAddress = 0, chunk = 0, itemAddress = 0;
            std::int32_t                        objectCount = 0, outerIndex = -1;
            std::uint32_t                       ownerFlags = 0, outerFlags = 0;
            ItemHeader                          item{};
            std::size_t                         count = 0;
            std::array<Node, maximumOuterDepth> path{};
        };

        template <typename T, std::size_t N>
        static T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            if (offset <= N && sizeof(T) <= N - offset)
                std::memcpy(&value, bytes.data() + offset, sizeof(T));
            return value;
        }
        static bool Add(std::uintptr_t base, std::size_t offset, std::uintptr_t& output) noexcept
        {
            if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
                return false;
            output = base + offset;
            return true;
        }
        static bool Multiply(std::size_t value, std::size_t scale, std::size_t& output) noexcept
        {
            if (scale && value > (std::numeric_limits<std::size_t>::max)() / scale)
                return false;
            output = value * scale;
            return true;
        }
        bool Copy(std::uintptr_t at, void* output, std::size_t bytes) const noexcept
        {
            return read && at && bytes && bytes <= (std::numeric_limits<std::uintptr_t>::max)() - at && read(at, output, bytes);
        }
        template <typename T>
        bool At(std::uintptr_t base, std::size_t offset, T& output) const noexcept
        {
            std::uintptr_t address = 0;
            return Add(base, offset, address) && Copy(address, &output, sizeof(T));
        }

        bool CaptureClass(std::uintptr_t candidate, std::uintptr_t expected, ClassState& output) const noexcept
        {
            ClassState     next{};
            std::uintptr_t candidateHeader = 0, baseHeader = 0;
            if (!Add(candidate, NativeUiClassMapProfile::classChainOffset, candidateHeader) ||
                !Add(expected, NativeUiClassMapProfile::classChainOffset, baseHeader) || !Copy(candidateHeader, next.candidate.data(), next.candidate.size()) ||
                !Copy(baseHeader, next.base.data(), next.base.size()))
                return false;
            const auto candidateIndex = Field<std::int32_t>(next.candidate, sizeof(std::uintptr_t));
            const auto baseIndex = Field<std::int32_t>(next.base, sizeof(std::uintptr_t));
            if (candidateIndex < 0 || candidateIndex > NativeClassChainAccess::maximumIndex || baseIndex < 0 ||
                baseIndex > NativeClassChainAccess::maximumIndex)
                return false;
            if (baseIndex <= candidateIndex)
            {
                std::size_t offset = 0;
                if (!Multiply(static_cast<std::size_t>(baseIndex), sizeof(std::uintptr_t), offset) ||
                    !Add(Field<std::uintptr_t>(next.candidate, 0), offset, next.selectedAddress) ||
                    !Copy(next.selectedAddress, &next.selected, sizeof(next.selected)))
                    return false;
                next.hasSelected = true;
            }
            output = next;
            return true;
        }
        static bool Equal(const ClassState& first, const ClassState& second) noexcept
        {
            return first.candidate == second.candidate && first.base == second.base && first.selectedAddress == second.selectedAddress &&
                   first.selected == second.selected && first.hasSelected == second.hasSelected;
        }

        bool Observe(std::uintptr_t owner, Sample& output) const noexcept
        {
            namespace Profile = NativeStartupWorldProfile;
            Sample         next{};
            std::uintptr_t expectedGetter = 0;
            if (!Add(image, Profile::getterRva, expectedGetter) || !At(image, Profile::ownerGetWorldSlotRva, next.getter) || next.getter != expectedGetter ||
                !At(image, Profile::levelClassCacheRva, next.levelClass) || !next.levelClass || !At(owner, Profile::objectFlagsOffset, next.ownerFlags) ||
                (next.ownerFlags & Profile::ownerRejectMask) || !At(owner, Profile::objectOuterOffset, next.value.outerAddress) || !next.value.outerAddress ||
                !At(next.value.outerAddress, Profile::objectFlagsOffset, next.outerFlags) || (next.outerFlags & Profile::outerRejectMask) ||
                !At(next.value.outerAddress, Profile::objectIndexOffset, next.outerIndex) || !At(image, Profile::objectCountRva, next.objectCount) ||
                next.objectCount <= 0 || next.outerIndex < 0 || next.outerIndex >= next.objectCount || !At(image, Profile::objectChunksRva, next.chunks) ||
                !next.chunks)
                return false;

            const auto  index = static_cast<std::uint32_t>(next.outerIndex);
            std::size_t chunkOffset = 0, itemOffset = 0;
            if (!Multiply(index >> Profile::chunkShift, sizeof(std::uintptr_t), chunkOffset) || !Add(next.chunks, chunkOffset, next.chunkAddress) ||
                !Copy(next.chunkAddress, &next.chunk, sizeof(next.chunk)) || !next.chunk ||
                !Multiply(index & Profile::chunkMask, Profile::objectItemBytes, itemOffset) || !Add(next.chunk, itemOffset, next.itemAddress) ||
                !Copy(next.itemAddress, next.item.data(), next.item.size()) ||
                (Field<std::uint32_t>(next.item, Profile::itemFlagsOffset) & Profile::itemRejectMask))
                return false;
            // Native GetWorld only checks the entry flags. Requiring its object
            // pointer to equal the supplied outer is an extra conservative
            // coherence check; serial fields do not pin a raw object's lifetime.
            if (Field<std::uintptr_t>(next.item, Profile::itemObjectOffset) != next.value.outerAddress)
                return false;

            auto address = next.value.outerAddress;
            while (address && next.count < maximumOuterDepth)
            {
                for (std::size_t i = 0; i < next.count; ++i)
                    if (next.path[i].address == address)
                        return false;
                auto& node = next.path[next.count++];
                node.address = address;
                if (!At(address, Profile::objectClassOffset, node.classAddress) || !node.classAddress ||
                    !At(address, Profile::objectOuterOffset, node.outerAddress) || !CaptureClass(node.classAddress, next.levelClass, node.type))
                    return false;
                ClassState after{};
                // The separate helper owns native subtype semantics. Retaining
                // its consumed metadata around that check lets whole-path
                // comparisons reject a changed type dependency as well.
                if (!NativeClassChainAccess(read).IsChildOf(node.classAddress, next.levelClass, node.matches) ||
                    !CaptureClass(node.classAddress, next.levelClass, after) || !Equal(node.type, after))
                    return false;
                if (node.matches)
                {
                    next.value.levelAddress = address;
                    if (!At(address, Profile::levelWorldOffset, next.value.worldAddress) || !next.value.worldAddress)
                        return false;
                    output = next;
                    return true;
                }
                address = node.outerAddress;
            }
            return false;
        }

        static bool Equal(const Sample& first, const Sample& second) noexcept
        {
            if (first.value.outerAddress != second.value.outerAddress || first.value.levelAddress != second.value.levelAddress ||
                first.value.worldAddress != second.value.worldAddress || first.getter != second.getter || first.levelClass != second.levelClass ||
                first.chunks != second.chunks || first.chunkAddress != second.chunkAddress || first.chunk != second.chunk ||
                first.itemAddress != second.itemAddress || first.objectCount != second.objectCount || first.outerIndex != second.outerIndex ||
                first.ownerFlags != second.ownerFlags || first.outerFlags != second.outerFlags || first.item != second.item || first.count != second.count)
                return false;
            for (std::size_t i = 0; i < first.count; ++i)
            {
                const auto& a = first.path[i];
                const auto& b = second.path[i];
                if (a.address != b.address || a.classAddress != b.classAddress || a.outerAddress != b.outerAddress || a.matches != b.matches ||
                    !Equal(a.type, b.type))
                    return false;
            }
            return true;
        }

        const std::uintptr_t image;
        const ReadMemory     read;
    };
}
