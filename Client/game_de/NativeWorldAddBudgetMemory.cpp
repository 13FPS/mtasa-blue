/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Seal actual native list-pool capacity and exact next allocation order
 ****************************************************************************/
#include "NativeWorldAddBudgetMemory.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace mta::game_de::NativeWorldAddBudgetMemory
{
    namespace AddP = NativeWorldAddProfile;
    namespace AddPlan = NativeWorldAddPlan;
    namespace AddC = NativeStreamingCompletionContext;
    namespace AddH = NativeModelReferenceHeap;
    namespace AddM = NativeWorldPoolMatrixMemory;
    namespace
    {
        template <class T, std::size_t N>
        T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            if (offset <= N && sizeof(value) <= N - offset)
                std::memcpy(&value, bytes.data() + offset, sizeof(value));
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
        std::uintptr_t Root(AddPlan::PoolKind kind) noexcept
        {
            return kind == AddPlan::PoolKind::Single ? AddP::SingleRootRva : AddP::DoubleRootRva;
        }
        std::size_t Stride(AddPlan::PoolKind kind) noexcept
        {
            return kind == AddPlan::PoolKind::Single ? AddP::SingleBytes : AddP::DoubleBytes;
        }
        bool Pool(std::uintptr_t image, AddPlan::PoolKind kind, NativeWorldIplMemory::Pool& output)
        {
            NativeWorldIplMemory::Pool pool;
            const auto                 capacity = kind == AddPlan::PoolKind::Single ? AddP::SingleCapacity : AddP::DoubleCapacity;
            const auto                 stride = Stride(kind);
            if (!NativeImageAccess::ReadImage(image, image + Root(kind), &pool.address, sizeof(pool.address), true) ||
                !Writable(pool.address, pool.header.size()) || !AddC::Copy(pool.address, pool.header.data(), pool.header.size()))
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
            // Header padding has no invented zero requirement. It is sealed
            // byte-for-byte along with genuine capacity/cursor/owner fields.
            pool.states.resize(capacity);
            if (!AddC::Copy(pool.flags, pool.states.data(), pool.states.size()))
                return false;
            for (const auto flag : pool.states)
                if (flag & 0x80)
                    ++pool.free;
            output = std::move(pool);
            return true;
        }
        bool Member(const NativeWorldIplMemory::Pool& pool, std::size_t stride, std::uintptr_t address) noexcept
        {
            return address >= pool.objects && (address - pool.objects) % stride == 0 && (address - pool.objects) / stride < pool.states.size() &&
                   !(pool.states[(address - pool.objects) / stride] & 0x80);
        }
        bool ReadNode(std::uintptr_t address, std::size_t stride, Node& node) noexcept
        {
            node.address = address;
            return AddC::Copy(address, node.bytes.data(), stride);
        }
        bool SealPool(std::uintptr_t image, AddPlan::PoolKind kind, const NativeWorldIplMemory::Pool& pool) noexcept
        {
            std::uintptr_t        current = 0;
            decltype(pool.header) header{};
            if (!NativeImageAccess::ReadImage(image, image + Root(kind), &current, sizeof(current), true) || current != pool.address ||
                !AddC::Copy(current, header.data(), header.size()) || header != pool.header)
                return false;
            std::array<unsigned char, 256> chunk{};
            for (std::size_t at = 0; at != pool.states.size();)
            {
                const auto count = (std::min)(chunk.size(), pool.states.size() - at);
                if (!AddC::Copy(pool.flags + at, chunk.data(), count) || std::memcmp(chunk.data(), pool.states.data() + at, count))
                    return false;
                at += count;
            }
            return true;
        }
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        if (!AddM::Profile(image))
            return false;
        std::array<unsigned char, AddP::MaximumPinBytes> bytes{};
        for (const auto& pin : AddP::Code)
            if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        for (const auto& scalar : AddP::Scalars)
        {
            std::uint32_t value = 0;
            if (!AddH::ReadOnlyImage(image, image + scalar.rva, sizeof(value)) ||
                !NativeImageAccess::ReadImage(image, image + scalar.rva, &value, sizeof(value)) || value != scalar.bits)
                return false;
        }
        for (const auto& slot : AddP::Slots)
        {
            std::uintptr_t target = 0;
            if (!AddH::ReadOnlyImage(image, image + slot.rva, sizeof(target)) ||
                !NativeImageAccess::ReadImage(image, image + slot.rva, &target, sizeof(target)) || target != image + slot.target)
                return false;
        }
        return true;
    }
    ClockResult CaptureForRect(std::uintptr_t image, AddPlan::Kind kind, std::uint32_t index, const AddPlan::Rect& supplied, Value& output)
    {
        if ((kind != AddPlan::Kind::Building && kind != AddPlan::Kind::Dummy) || !NativeWorldIplPlan::Valid(supplied))
            return ClockResult::InvalidArgument;
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        Value value;
        auto  result = AddC::Capture(image, value.world);
        if (result != ClockResult::Ok || (result = AddH::Capture(image, value.heap)) != ClockResult::Ok)
            return result;
        const auto entityKind = kind == AddPlan::Kind::Building ? AddM::Kind::Building : AddM::Kind::Dummy;
        if ((result = AddM::CaptureEntity(image, entityKind, index, value.entity)) != ClockResult::Ok)
            return result;
        if (Field<std::uintptr_t>(value.entity.bytes, 0x20))
            return ClockResult::UnsupportedBuild;
        const auto lod = (Field<std::uint32_t>(value.entity.bytes, 0x30) & AddP::LodFlag) != 0;
        const auto poolKind = AddPlan::Select(kind, lod);
        if (!Pool(image, poolKind, value.pool))
            return ClockResult::BindingChanged;
        const auto&                                                 pool = value.pool;
        const auto                                                  stride = Stride(poolKind);
        const std::array<std::pair<std::uintptr_t, std::size_t>, 3> storage{
            {{pool.address, pool.header.size()}, {pool.objects, static_cast<std::size_t>(pool.capacity) * stride}, {pool.flags, pool.states.size()}}};
        for (const auto& span : storage)
            if (!Disjoint(span.first, span.second, value.entity.pool, value.entity.header.size()) ||
                !Disjoint(span.first, span.second, value.entity.objects, static_cast<std::size_t>(value.entity.capacity) * value.entity.stride) ||
                !Disjoint(span.first, span.second, value.entity.flags, static_cast<std::size_t>(value.entity.capacity)))
                return ClockResult::BindingChanged;
        if ((result = AddPlan::Require(kind, lod, supplied, pool.cursor, pool.states, value.plan)) != ClockResult::Ok)
            return result;
        value.suppliedBounds = supplied;
        value.heads.reserve(value.plan.steps.size());
        value.allocatedPrefix.reserve(value.plan.steps.size());
        for (const auto& step : value.plan.steps)
        {
            Head head;
            head.rva = step.headRva;
            if (!NativeImageAccess::ReadImage(image, image + head.rva, &head.value, sizeof(head.value), true))
                return ClockResult::ReadFailure;
            if (head.value)
            {
                if (!Member(pool, stride, head.value) || !ReadNode(head.value, stride, head.node))
                    return ClockResult::BindingChanged;
                if (poolKind == AddPlan::PoolKind::Double && Field<std::uintptr_t>(head.node.bytes, 0x10))
                    return ClockResult::BindingChanged;
            }
            Node allocated;
            if (!ReadNode(pool.objects + step.slot * stride, stride, allocated))
                return ClockResult::ReadFailure;
            value.heads.push_back(head);
            value.allocatedPrefix.push_back(allocated);
        }
        if (!Seal(image, value))
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    bool Seal(std::uintptr_t image, const Value& value) noexcept
    {
        AddC::Value world;
        if (!Profile(image) || AddC::Capture(image, world) != ClockResult::Ok || !AddC::Same(world, value.world, true) ||
            AddH::Recheck(image, value.heap) != ClockResult::Ok || !AddM::SealEntity(image, value.entity) || !SealPool(image, value.plan.pool, value.pool) ||
            value.heads.size() != value.plan.steps.size() || value.allocatedPrefix.size() != value.plan.steps.size())
            return false;
        const auto stride = Stride(value.plan.pool);
        for (const auto& head : value.heads)
        {
            std::uintptr_t current = 0;
            if (!NativeImageAccess::ReadImage(image, image + head.rva, &current, sizeof(current), true) || current != head.value)
                return false;
            if (current)
            {
                Node node;
                if (!ReadNode(current, stride, node) || node.bytes != head.node.bytes)
                    return false;
            }
        }
        for (const auto& before : value.allocatedPrefix)
        {
            Node current;
            if (!ReadNode(before.address, stride, current) || current.bytes != before.bytes)
                return false;
        }
        return true;
    }
}
