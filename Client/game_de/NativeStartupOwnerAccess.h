/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded observations of the native application's enclosing owner
 *****************************************************************************/
#pragma once
#include "NativeClassChainAccess.h"
#include "NativeStartupOwnerProfile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    // Profiles and the natural application callback are qualified by the
    // caller. Coherent paths do not pin a UObject or grant lifecycle authority.
    class NativeStartupOwnerAccess final
    {
    public:
        using ReadMemory = NativeClassChainAccess::ReadMemory;
        struct Value
        {
            std::uintptr_t gameModeAddress = 0;
        };
        static constexpr std::size_t maximumOuterNodes = 64;
        NativeStartupOwnerAccess(std::uintptr_t imageBase, ReadMemory guardedRead) noexcept : image(imageBase), read(guardedRead) {}

        bool Read(std::uintptr_t rootAddress, Value& output) const noexcept
        {
            Path first{}, second{};
            if (!Observe(rootAddress, first) || !Observe(rootAddress, second) || !Equal(first, second))
                return false;
            output.gameModeAddress = second.owner;
            return true;
        }

    private:
        using Header = std::array<unsigned char, 12>;
        struct Type
        {
            Header         candidate{}, base{};
            std::uintptr_t selected = 0;
            bool           hasSelected = false;
        };
        struct Node
        {
            std::uintptr_t address = 0, outer = 0, classAddress = 0;
            Type           type{};
            bool           matches = false;
        };
        struct Path
        {
            std::uintptr_t                      cachedClass = 0, rootOuter = 0, owner = 0, backreference = 0, vtable = 0, worldTarget = 0;
            std::size_t                         count = 0;
            std::array<Node, maximumOuterNodes> nodes{};
        };
        static_assert(sizeof(std::uintptr_t) == 8);
        static constexpr std::size_t worldSlot = NativeStartupOwnerProfile::gameModeGetWorldSlotRva - NativeStartupOwnerProfile::gameModeVtableRva;
        static_assert(worldSlot == 0x158);

        static bool Add(std::uintptr_t base, std::size_t offset, std::uintptr_t& output) noexcept
        {
            if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
                return false;
            output = base + offset;
            return true;
        }
        bool Copy(std::uintptr_t at, void* output, std::size_t bytes) const noexcept
        {
            return read && at && bytes && bytes <= (std::numeric_limits<std::uintptr_t>::max)() - at && read(at, output, bytes);
        }
        template <typename T>
        bool Field(std::uintptr_t address, std::size_t offset, T& output) const noexcept
        {
            std::uintptr_t at = 0;
            return Add(address, offset, at) && Copy(at, &output, sizeof(output));
        }
        template <typename T>
        static T HeaderField(const Header& header, std::size_t offset) noexcept
        {
            T value{};
            if (offset <= header.size() && sizeof(T) <= header.size() - offset)
                std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        }
        bool CaptureType(std::uintptr_t candidate, std::uintptr_t base, Type& output) const noexcept
        {
            Type           next{};
            std::uintptr_t candidateHeader = 0, baseHeader = 0;
            if (!Add(candidate, NativeStartupOwnerProfile::classChainOffset, candidateHeader) ||
                !Add(base, NativeStartupOwnerProfile::classChainOffset, baseHeader) || !Copy(candidateHeader, next.candidate.data(), next.candidate.size()) ||
                !Copy(baseHeader, next.base.data(), next.base.size()))
                return false;
            const auto candidateIndex = HeaderField<std::int32_t>(next.candidate, 8), baseIndex = HeaderField<std::int32_t>(next.base, 8);
            if (candidateIndex < 0 || candidateIndex > NativeClassChainAccess::maximumIndex || baseIndex < 0 ||
                baseIndex > NativeClassChainAccess::maximumIndex)
                return false;
            if (baseIndex <= candidateIndex)
            {
                const auto     chain = HeaderField<std::uintptr_t>(next.candidate, 0);
                std::uintptr_t slot = 0;
                if (!Add(chain, static_cast<std::size_t>(baseIndex) * sizeof(std::uintptr_t), slot) || !Copy(slot, &next.selected, sizeof(next.selected)))
                    return false;
                next.hasSelected = true;
            }
            output = next;
            return true;
        }
        static bool EqualType(const Type& a, const Type& b) noexcept
        {
            return a.candidate == b.candidate && a.base == b.base && a.selected == b.selected && a.hasSelected == b.hasSelected;
        }
        bool NodeFields(Node& node) const noexcept
        {
            return Field(node.address, NativeStartupOwnerProfile::objectOuterOffset, node.outer) &&
                   Field(node.address, NativeStartupOwnerProfile::objectClassOffset, node.classAddress);
        }
        bool OwnerFields(std::uintptr_t owner, std::uintptr_t& backreference, std::uintptr_t& vtable, std::uintptr_t& worldTarget) const noexcept
        {
            return Field(owner, NativeStartupOwnerProfile::gameModeInterfaceOffset, backreference) && Copy(owner, &vtable, sizeof(vtable)) &&
                   Field(vtable, worldSlot, worldTarget);
        }
        bool Observe(std::uintptr_t root, Path& output) const noexcept
        {
            namespace Profile = NativeStartupOwnerProfile;
            Path           next{};
            std::uintptr_t cache = 0, target = 0;
            if (!Add(image, Profile::gameModeClassCacheRva, cache) || !Add(image, Profile::gameModeGetWorldRva, target) ||
                !Copy(cache, &next.cachedClass, sizeof(next.cachedClass)) || !next.cachedClass || !Field(root, Profile::objectOuterOffset, next.rootOuter))
                return false;
            auto current = next.rootOuter;
            while (current)
            {
                // Native has no bounded missing-owner return. An observation
                // must refuse cycles/deep paths instead of reproducing a hang.
                if (current == root || next.count == maximumOuterNodes)
                    return false;
                for (std::size_t prior = 0; prior < next.count; ++prior)
                    if (next.nodes[prior].address == current)
                        return false;
                auto& node = next.nodes[next.count++];
                node.address = current;
                if (!NodeFields(node) || !CaptureType(node.classAddress, next.cachedClass, node.type) ||
                    !NativeClassChainAccess(read).IsChildOf(node.classAddress, next.cachedClass, node.matches))
                    return false;
                Type after{};
                if (!CaptureType(node.classAddress, next.cachedClass, after) || !EqualType(node.type, after))
                    return false;
                if (node.matches)
                {
                    next.owner = current;
                    break;
                }
                current = node.outer;
            }
            // First typed owner wins, as in native GetTypedOuter. A failed
            // correlation or overridden method must not search for a later one.
            if (!next.owner || !OwnerFields(next.owner, next.backreference, next.vtable, next.worldTarget) || next.backreference != root ||
                next.worldTarget != target)
                return false;
            for (std::size_t index = 0; index < next.count; ++index)
            {
                const auto& before = next.nodes[index];
                Node        after{};
                after.address = before.address;
                if (!NodeFields(after) || after.outer != before.outer || after.classAddress != before.classAddress ||
                    !CaptureType(after.classAddress, next.cachedClass, after.type) || !EqualType(before.type, after.type))
                    return false;
            }
            std::uintptr_t backreferenceAfter = 0, vtableAfter = 0, worldTargetAfter = 0;
            std::uintptr_t cacheAfter = 0, outerAfter = 0;
            if (!OwnerFields(next.owner, backreferenceAfter, vtableAfter, worldTargetAfter) || backreferenceAfter != next.backreference ||
                vtableAfter != next.vtable || worldTargetAfter != next.worldTarget || !Copy(cache, &cacheAfter, sizeof(cacheAfter)) ||
                cacheAfter != next.cachedClass || !Field(root, Profile::objectOuterOffset, outerAfter) || outerAfter != next.rootOuter)
                return false;
            output = next;
            return true;
        }
        static bool Equal(const Path& a, const Path& b) noexcept
        {
            if (a.cachedClass != b.cachedClass || a.rootOuter != b.rootOuter || a.owner != b.owner || a.backreference != b.backreference ||
                a.vtable != b.vtable || a.worldTarget != b.worldTarget || a.count != b.count)
                return false;
            for (std::size_t index = 0; index < a.count; ++index)
            {
                const auto& x = a.nodes[index];
                const auto& y = b.nodes[index];
                if (x.address != y.address || x.outer != y.outer || x.classAddress != y.classAddress || x.matches != y.matches || !EqualType(x.type, y.type))
                    return false;
            }
            return true;
        }
        const std::uintptr_t image;
        const ReadMemory     read;
    };
}
