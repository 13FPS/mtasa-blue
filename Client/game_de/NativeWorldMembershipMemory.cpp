/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Seal complete native world heads and their fixed-pool node graphs
 ****************************************************************************/
#include "NativeWorldMembershipMemory.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>
namespace mta::game_de::NativeWorldMembershipMemory
{
    namespace P = NativeWorldMembershipProfile;
    namespace A = NativeWorldAddProfile;
    namespace C = NativeStreamingCompletionContext;
    namespace H = NativeModelReferenceHeap;
    namespace E = NativeWorldPoolMatrixMemory;
    namespace I = NativeImageAccess;
    namespace
    {
        template <class T>
        T Field(const unsigned char* bytes, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, bytes + offset, sizeof(value));
            return value;
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
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                    return false;
                address = (std::min)(end, begin + page.RegionSize);
            }
            return true;
        }
        bool Disjoint(std::uintptr_t a, std::size_t aBytes, std::uintptr_t b, std::size_t bBytes) noexcept
        {
            return a + aBytes <= b || b + bBytes <= a;
        }
        std::uintptr_t Root(unsigned pool) noexcept
        {
            return pool ? A::DoubleRootRva : A::SingleRootRva;
        }
        std::size_t Capacity(unsigned pool) noexcept
        {
            return pool ? A::DoubleCapacity : A::SingleCapacity;
        }
        std::size_t Stride(unsigned pool) noexcept
        {
            return pool ? A::DoubleBytes : A::SingleBytes;
        }
        bool Geometry(unsigned index, const Pool& observed) noexcept
        {
            const auto& pool = observed.native;
            const auto  capacity = Capacity(index), stride = Stride(index);
            return pool.capacity == static_cast<std::int32_t>(capacity) && pool.cursor >= -1 && pool.cursor < pool.capacity && pool.header[0x18] == 1 &&
                   pool.states.size() == capacity && observed.bytes.size() == capacity * stride && Writable(pool.address, pool.header.size()) &&
                   Writable(pool.objects, capacity * stride) && Writable(pool.flags, capacity, 1) &&
                   Field<std::uintptr_t>(pool.header.data(), 0) == pool.objects && Field<std::uintptr_t>(pool.header.data(), 8) == pool.flags &&
                   Field<std::int32_t>(pool.header.data(), 0x10) == pool.capacity && Field<std::int32_t>(pool.header.data(), 0x14) == pool.cursor &&
                   Disjoint(pool.address, pool.header.size(), pool.objects, capacity * stride) &&
                   Disjoint(pool.address, pool.header.size(), pool.flags, capacity) && Disjoint(pool.objects, capacity * stride, pool.flags, capacity);
        }
        bool ReadPool(std::uintptr_t image, unsigned index, Pool& output)
        {
            Pool  observed;
            auto& pool = observed.native;
            if (!I::ReadImage(image, image + Root(index), &pool.address, sizeof(pool.address), true) || !Writable(pool.address, pool.header.size()) ||
                !C::Copy(pool.address, pool.header.data(), pool.header.size()))
                return false;
            pool.objects = Field<std::uintptr_t>(pool.header.data(), 0);
            pool.flags = Field<std::uintptr_t>(pool.header.data(), 8);
            pool.capacity = Field<std::int32_t>(pool.header.data(), 0x10);
            pool.cursor = Field<std::int32_t>(pool.header.data(), 0x14);
            pool.states.resize(Capacity(index));
            observed.bytes.resize(Capacity(index) * Stride(index));
            if (!Geometry(index, observed) || !C::Copy(pool.flags, pool.states.data(), pool.states.size()) ||
                !C::Copy(pool.objects, observed.bytes.data(), observed.bytes.size()))
                return false;
            for (const auto flag : pool.states)
                if (flag & 0x80)
                    ++pool.free;
            output = std::move(observed);
            return true;
        }
        bool Separate(const Value& value) noexcept
        {
            using Span = std::pair<std::uintptr_t, std::size_t>;
            std::array<Span, 9> spans{};
            for (unsigned index = 0; index != 2; ++index)
            {
                const auto& pool = value.pools[index];
                spans[index * 3] = {pool.native.address, pool.native.header.size()};
                spans[index * 3 + 1] = {pool.native.objects, pool.bytes.size()};
                spans[index * 3 + 2] = {pool.native.flags, pool.native.states.size()};
            }
            spans[6] = {value.entity.pool, value.entity.header.size()};
            spans[7] = {value.entity.objects, static_cast<std::size_t>(value.entity.capacity) * value.entity.stride};
            spans[8] = {value.entity.flags, static_cast<std::size_t>(value.entity.capacity)};
            for (std::size_t a = 0; a != spans.size(); ++a)
                for (std::size_t b = a + 1; b != spans.size(); ++b)
                    if (!Disjoint(spans[a].first, spans[a].second, spans[b].first, spans[b].second))
                        return false;
            return true;
        }
        bool ReadHeads(std::uintptr_t image, std::uintptr_t rva, std::size_t count, std::vector<std::uintptr_t>& heads)
        {
            heads.resize(count);
            return I::ReadImage(image, image + rva, heads.data(), count * sizeof(std::uintptr_t), true);
        }
        bool Topology(Value& value)
        {
            std::array<std::vector<unsigned char>, 2> visited;
            for (unsigned index = 0; index != 2; ++index)
                visited[index].assign(Capacity(index), 0);
            const auto walk = [&](std::uintptr_t rva, std::uintptr_t head, unsigned poolIndex, Group group)
            {
                auto&          observed = value.pools[poolIndex];
                const auto&    pool = observed.native;
                const auto     stride = Stride(poolIndex);
                std::uintptr_t previous = 0;
                for (auto address = head; address;)
                {
                    if (address < pool.objects || (address - pool.objects) % stride)
                        return false;
                    const auto slot = (address - pool.objects) / stride;
                    if (slot >= pool.states.size() || (pool.states[slot] & 0x80) || visited[poolIndex][slot])
                        return false;
                    visited[poolIndex][slot] = 1;
                    ++observed.reachable;
                    const auto* node = observed.bytes.data() + slot * stride;
                    if (poolIndex && Field<std::uintptr_t>(node, 0x10) != previous)
                        return false;
                    if (Field<std::uintptr_t>(node, 0) == value.entity.address)
                    {
                        ++value.counts[static_cast<unsigned>(group)];
                        value.occurrences.push_back({rva, address, group, pool.states[slot]});
                    }
                    previous = address;
                    address = Field<std::uintptr_t>(node, 8);
                }
                return true;
            };
            for (std::size_t index = 0; index != value.normal.size(); ++index)
                if (!walk(P::NormalRva + index * 8, value.normal[index], static_cast<unsigned>(index % 2),
                          index % 2 ? Group::NormalDummy : Group::NormalBuilding))
                    return false;
            for (std::size_t index = 0; index != value.repeat.size(); ++index)
                if (!walk(P::RepeatRva + index * 8, value.repeat[index], 1, static_cast<Group>(static_cast<unsigned>(Group::RepeatVehicle) + index % 3)))
                    return false;
            for (std::size_t index = 0; index != value.lod.size(); ++index)
                if (!walk(P::LodRva + index * 8, value.lod[index], 0, Group::Lod))
                    return false;
            return walk(P::MovingRva, value.moving, 1, Group::Moving);
        }
        bool EqualBytes(std::uintptr_t address, const unsigned char* before, std::size_t size) noexcept
        {
            std::array<unsigned char, 4096> bytes{};
            for (std::size_t offset = 0; offset != size;)
            {
                const auto count = (std::min)(bytes.size(), size - offset);
                if (!C::Copy(address + offset, bytes.data(), count) || std::memcmp(bytes.data(), before + offset, count))
                    return false;
                offset += count;
            }
            return true;
        }
        bool EqualHeads(std::uintptr_t image, std::uintptr_t rva, const std::vector<std::uintptr_t>& before) noexcept
        {
            // Source bank storage is module-owned and its complete extent was
            // derived from native indexing, not from adjacent readable pages.
            std::array<std::uintptr_t, 256> bytes{};
            for (std::size_t offset = 0; offset != before.size();)
            {
                const auto count = (std::min)(bytes.size(), before.size() - offset);
                if (!I::ReadImage(image, image + rva + offset * 8, bytes.data(), count * 8, true) ||
                    std::memcmp(bytes.data(), before.data() + offset, count * 8))
                    return false;
                offset += count;
            }
            return true;
        }
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        if (!NativeWorldAddBudgetMemory::Profile(image))
            return false;
        std::array<unsigned char, NativePedOrientationProfile::MaximumPinBytes> bytes{};
        for (const auto& pin : P::MovingCode)
            if (pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        return true;
    }
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner, CheckOwner afterApplication,
                        Value& output)
    {
        if (!owner || !afterApplication)
            return ClockResult::NotInitialized;
        if (!generation || (kind != Kind::Building && kind != Kind::Dummy))
            return ClockResult::InvalidArgument;
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        try
        {
            Value value;
            value.generation = generation;
            auto result = C::Owner(image, owner);
            if (result != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok || (result = C::Capture(image, value.world)) != ClockResult::Ok ||
                (result = H::Capture(image, value.heap)) != ClockResult::Ok || (result = E::CaptureEntity(image, kind, index, value.entity)) != ClockResult::Ok)
                return result;
            if (!ReadPool(image, 0, value.pools[0]) || !ReadPool(image, 1, value.pools[1]) || !Separate(value))
                return ClockResult::BindingChanged;
            if (!ReadHeads(image, P::NormalRva, P::NormalHeads, value.normal) || !ReadHeads(image, P::RepeatRva, P::RepeatHeads, value.repeat) ||
                !ReadHeads(image, P::LodRva, P::LodHeads, value.lod) || !I::ReadImage(image, image + P::MovingRva, &value.moving, sizeof(value.moving), true))
                return ClockResult::ReadFailure;
            if (!Topology(value))
                return ClockResult::BindingChanged;
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
        C::Value world;
        if (!value.generation || !Profile(image) || value.normal.size() != P::NormalHeads || value.repeat.size() != P::RepeatHeads ||
            value.lod.size() != P::LodHeads || C::Capture(image, world) != ClockResult::Ok || !C::Same(world, value.world, true) ||
            H::Recheck(image, value.heap) != ClockResult::Ok || !E::SealEntity(image, value.entity))
            return false;
        for (unsigned index = 0; index != 2; ++index)
        {
            const auto&    observed = value.pools[index];
            const auto&    pool = observed.native;
            std::uintptr_t root = 0;
            if (!Geometry(index, observed) || !I::ReadImage(image, image + Root(index), &root, sizeof(root), true) || root != pool.address ||
                !EqualBytes(pool.address, pool.header.data(), pool.header.size()) || !EqualBytes(pool.flags, pool.states.data(), pool.states.size()) ||
                !EqualBytes(pool.objects, observed.bytes.data(), observed.bytes.size()))
                return false;
        }
        std::uintptr_t moving = 0;
        return Separate(value) && EqualHeads(image, P::NormalRva, value.normal) && EqualHeads(image, P::RepeatRva, value.repeat) &&
               EqualHeads(image, P::LodRva, value.lod) && I::ReadImage(image, image + P::MovingRva, &moving, sizeof(moving), true) && moving == value.moving &&
               C::Fence(image, value.world) && H::Recheck(image, value.heap) == ClockResult::Ok && E::SealEntity(image, value.entity);
    }
}
