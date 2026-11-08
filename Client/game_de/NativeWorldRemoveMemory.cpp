/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Match native sector/LOD unlink and fixed-slot release effects
 ****************************************************************************/
#include "NativeWorldRemoveMemory.h"
#include "NativeWorldIplLoadedProfile.h"
#include "NativeWorldPoolMatrixProfile.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
namespace mta::game_de::NativeWorldRemoveMemory
{
    namespace P = NativeWorldAddProfile;
    namespace B = NativeWorldBoundRectMemory;
    namespace C = NativeStreamingCompletionContext;
    namespace I = NativeImageAccess;
    namespace
    {
        template <class T, std::size_t N>
        T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T result{};
            if (offset <= N && sizeof(result) <= N - offset)
                std::memcpy(&result, bytes.data() + offset, sizeof(result));
            return result;
        }
        template <class T, std::size_t N>
        void Put(std::array<unsigned char, N>& bytes, std::size_t offset, const T& value) noexcept
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }
        bool Writable(std::uintptr_t address, std::size_t bytes, std::size_t alignment = 8) noexcept
        {
            if (!address || address % alignment || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + bytes;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect != PAGE_READWRITE && page.Protect != PAGE_WRITECOPY))
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (base > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + page.RegionSize <= address)
                    return false;
                address = (std::min)(end, base + page.RegionSize);
            }
            return true;
        }
        bool Disjoint(std::uintptr_t a, std::size_t aBytes, std::uintptr_t b, std::size_t bBytes) noexcept
        {
            return a + aBytes <= b || b + bBytes <= a;
        }
        std::uintptr_t Root(NativeWorldAddPlan::PoolKind kind) noexcept
        {
            return kind == NativeWorldAddPlan::PoolKind::Single ? P::SingleRootRva : P::DoubleRootRva;
        }
        std::size_t Stride(NativeWorldAddPlan::PoolKind kind) noexcept
        {
            return kind == NativeWorldAddPlan::PoolKind::Single ? P::SingleBytes : P::DoubleBytes;
        }
        bool ReadPool(std::uintptr_t image, NativeWorldAddPlan::PoolKind kind, NativeWorldIplMemory::Pool& pool)
        {
            const auto capacity = kind == NativeWorldAddPlan::PoolKind::Single ? P::SingleCapacity : P::DoubleCapacity;
            const auto stride = Stride(kind);
            if (!I::ReadImage(image, image + Root(kind), &pool.address, sizeof(pool.address), true) || !Writable(pool.address, pool.header.size()) ||
                !C::Copy(pool.address, pool.header.data(), pool.header.size()))
                return false;
            pool.objects = Field<std::uintptr_t>(pool.header, 0);
            pool.flags = Field<std::uintptr_t>(pool.header, 8);
            pool.capacity = Field<std::int32_t>(pool.header, 0x10);
            pool.cursor = Field<std::int32_t>(pool.header, 0x14);
            if (pool.capacity != static_cast<std::int32_t>(capacity) || pool.cursor < -1 || pool.cursor >= pool.capacity || pool.header[0x18] != 1 ||
                !Writable(pool.objects, capacity * stride) || !Writable(pool.flags, capacity, 1) ||
                !Disjoint(pool.address, pool.header.size(), pool.objects, capacity * stride) ||
                !Disjoint(pool.address, pool.header.size(), pool.flags, capacity) || !Disjoint(pool.objects, capacity * stride, pool.flags, capacity))
                return false;
            pool.states.resize(capacity);
            if (!C::Copy(pool.flags, pool.states.data(), pool.states.size()))
                return false;
            for (const auto flag : pool.states)
                if (flag & 0x80)
                    ++pool.free;
            return true;
        }
        bool Member(const Value& value, std::uintptr_t address) noexcept
        {
            const auto& pool = value.pool;
            const auto  stride = Stride(value.poolKind);
            return address >= pool.objects && (address - pool.objects) % stride == 0 && (address - pool.objects) / stride < pool.states.size() &&
                   !(pool.states[(address - pool.objects) / stride] & 0x80);
        }
        bool ReadNode(std::uintptr_t address, std::size_t stride, std::array<unsigned char, P::DoubleBytes>& bytes) noexcept
        {
            bytes = {};
            return C::Copy(address, bytes.data(), stride);
        }
        bool Equal(std::uintptr_t image, const Value& value, bool removed) noexcept
        {
            if (!Profile(image) || !B::Seal(image, value.bound))
                return false;
            std::uintptr_t current = 0;
            auto           header = value.pool.header;
            if (removed)
                Put(header, 0x14, value.cursorAfter);
            decltype(header) actual{};
            if (!I::ReadImage(image, image + Root(value.poolKind), &current, sizeof(current), true) || current != value.pool.address ||
                !C::Copy(current, actual.data(), actual.size()) || actual != header)
                return false;
            const auto&                    expected = removed ? value.flagsAfter : value.pool.states;
            std::array<unsigned char, 256> chunk{};
            for (std::size_t offset = 0; offset != expected.size();)
            {
                const auto size = (std::min)(chunk.size(), expected.size() - offset);
                if (!C::Copy(value.pool.flags + offset, chunk.data(), size) || std::memcmp(chunk.data(), expected.data() + offset, size))
                    return false;
                offset += size;
            }
            for (const auto& head : value.heads)
                if (!I::ReadImage(image, image + head.rva, &current, sizeof(current), true) || current != (removed ? head.after : head.before))
                    return false;
            for (const auto& node : value.nodes)
            {
                decltype(node.before) bytes{};
                if (!ReadNode(node.address, Stride(value.poolKind), bytes) || bytes != (removed ? node.after : node.before))
                    return false;
            }
            return true;
        }
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        if (!B::Profile(image))
            return false;
        const std::array<NativeWorldIplLoadedProfile::Pin, 4> pins{
            {{NativeWorldPoolMatrixProfile::WorldRemoveRva, NativeWorldPoolMatrixProfile::WorldRemoveBytes.data(),
              NativeWorldPoolMatrixProfile::WorldRemoveBytes.size(), true},
             {0x1132490, NativeWorldIplLoadedProfile::EntityWorldRemoveBytes.data(), NativeWorldIplLoadedProfile::EntityWorldRemoveBytes.size(), true},
             {0x1171130, NativeWorldIplPoolProfile::SingleInitialize.data(), NativeWorldIplPoolProfile::SingleInitialize.size(), true},
             {0x1171202, NativeWorldCoverProfile::DoubleLinkPoolPublishingIntervalBytes.data(),
              NativeWorldCoverProfile::DoubleLinkPoolPublishingIntervalBytes.size(), true}}};
        std::array<unsigned char, 820> bytes{};
        for (const auto& pin : pins)
            if (pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        for (const auto table :
             {NativeWorldIplLoadedProfile::BuildingVtable, NativeWorldIplLoadedProfile::SpecialBuildingVtable, NativeWorldIplLoadedProfile::DummyVtable})
        {
            std::uintptr_t method = 0;
            if (!NativeModelReferenceHeap::ReadOnlyImage(image, image + table + 0x20, sizeof(method)) ||
                !I::ReadImage(image, image + table + 0x20, &method, sizeof(method)) || method != image + 0x1132490)
                return false;
        }
        return true;
    }
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner, CheckOwner afterApplication,
                        Value& output)
    {
        if (!owner || !afterApplication)
            return ClockResult::NotInitialized;
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        try
        {
            Value value;
            auto  result = B::Capture(image, generation, kind, index, owner, afterApplication, value.bound);
            if (result != ClockResult::Ok)
                return result;
            const auto& entity = value.bound.entity;
            const auto  lod = (Field<std::uint32_t>(entity.bytes, 0x30) & P::LodFlag) != 0;
            const auto  side = lod ? P::LodSide : P::SectorSide;
            const auto  left = entity.bytes[0x3c], right = entity.bytes[0x3d], bottom = entity.bytes[0x3e], top = entity.bytes[0x3f];
            // Native normal sectors clamp these bytes; admit the ordinary
            // in-range representation so cells cannot alias through clamping.
            if (left > right || bottom > top || static_cast<std::size_t>(right) >= side || static_cast<std::size_t>(top) >= side)
                return ClockResult::BindingChanged;
            value.poolKind = NativeWorldAddPlan::Select(kind == Kind::Building ? NativeWorldAddPlan::Kind::Building : NativeWorldAddPlan::Kind::Dummy, lod);
            if (!ReadPool(image, value.poolKind, value.pool))
                return ClockResult::BindingChanged;
            const auto                                                  stride = Stride(value.poolKind);
            const std::array<std::pair<std::uintptr_t, std::size_t>, 3> storage{{{value.pool.address, value.pool.header.size()},
                                                                                 {value.pool.objects, value.pool.states.size() * stride},
                                                                                 {value.pool.flags, value.pool.states.size()}}};
            for (const auto& span : storage)
                if (!Disjoint(span.first, span.second, entity.pool, entity.header.size()) ||
                    !Disjoint(span.first, span.second, entity.objects, static_cast<std::size_t>(entity.capacity) * entity.stride) ||
                    !Disjoint(span.first, span.second, entity.flags, static_cast<std::size_t>(entity.capacity)))
                    return ClockResult::BindingChanged;
            value.flagsAfter = value.pool.states;
            value.cursorAfter = value.pool.cursor;
            std::unordered_set<std::uintptr_t>              visited;
            std::unordered_map<std::uintptr_t, std::size_t> nodeIndex;
            for (unsigned y = bottom; y <= top; ++y)
                for (unsigned x = left; x <= right; ++x)
                {
                    const auto cell = static_cast<std::size_t>(y) * side + x;
                    Head       head;
                    head.rva = lod ? P::LodRva + cell * 8 : P::SectorsRva + cell * 16 + (value.poolKind == NativeWorldAddPlan::PoolKind::Double ? 8 : 0);
                    if (!I::ReadImage(image, image + head.rva, &head.before, sizeof(head.before), true))
                        return ClockResult::ReadFailure;
                    head.after = head.before;
                    std::uintptr_t previous = 0, selected = 0;
                    for (auto at = head.before; at;)
                    {
                        if (visited.size() >= value.pool.states.size() || !visited.insert(at).second || !Member(value, at))
                            return ClockResult::BindingChanged;
                        Node node;
                        node.address = at;
                        if (!ReadNode(at, stride, node.before))
                            return ClockResult::ReadFailure;
                        node.after = node.before;
                        if (value.poolKind == NativeWorldAddPlan::PoolKind::Double && Field<std::uintptr_t>(node.before, 0x10) != previous)
                            return ClockResult::BindingChanged;
                        if (Field<std::uintptr_t>(node.before, 0) == entity.address)
                        {
                            if (selected)
                                return ClockResult::BindingChanged;
                            selected = at;
                        }
                        nodeIndex.emplace(at, value.nodes.size());
                        value.nodes.push_back(node);
                        previous = at;
                        at = Field<std::uintptr_t>(node.before, 8);
                    }
                    if (selected)
                    {
                        const auto     selectedIndex = nodeIndex.at(selected);
                        const auto     next = Field<std::uintptr_t>(value.nodes[selectedIndex].before, 8);
                        std::uintptr_t prior = 0;
                        if (value.poolKind == NativeWorldAddPlan::PoolKind::Double)
                            prior = Field<std::uintptr_t>(value.nodes[selectedIndex].before, 0x10);
                        else
                            for (auto at = head.before; at != selected; at = Field<std::uintptr_t>(value.nodes[nodeIndex.at(at)].before, 8))
                                prior = at;
                        if (head.before == selected)
                            head.after = next;
                        if (prior)
                            Put(value.nodes[nodeIndex.at(prior)].after, 8, next);
                        if (value.poolKind == NativeWorldAddPlan::PoolKind::Double && next)
                            Put(value.nodes[nodeIndex.at(next)].after, 0x10, prior);
                        const auto slot = static_cast<std::size_t>((selected - value.pool.objects) / stride);
                        value.flagsAfter[slot] |= 0x80;
                        if (static_cast<std::int32_t>(slot) < value.cursorAfter)
                            value.cursorAfter = static_cast<std::int32_t>(slot);
                        ++value.removedCount;
                    }
                    value.heads.push_back(head);
                }
            value.identity = {image,
                              static_cast<std::uintptr_t>(generation),
                              static_cast<std::uintptr_t>(kind),
                              index,
                              value.bound.world.root,
                              value.bound.world.world,
                              value.bound.world.gameMode,
                              value.bound.world.level,
                              entity.pool,
                              entity.address,
                              entity.flags,
                              entity.generation,
                              value.pool.address,
                              value.pool.objects,
                              value.pool.flags,
                              static_cast<std::uintptr_t>(value.pool.capacity),
                              stride};
            if ((result = C::Owner(image, owner)) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
                return result;
            if (!Seal(image, value))
                return ClockResult::BindingChanged;
            output = std::move(value);
            return ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    bool Seal(std::uintptr_t image, const Value& value) noexcept
    {
        return Equal(image, value, false);
    }
    bool Removed(std::uintptr_t image, const Value& value) noexcept
    {
        return Equal(image, value, true);
    }
}
