/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit actual cover owners and acknowledge only exact native effects
 ****************************************************************************/
#include "NativeWorldCoverMemory.h"
#include "NativeWorldIplLoadedProfile.h"
#include "NativeImageAccess.h"
#include <algorithm>
#include <cstring>
namespace mta::game_de::NativeWorldCoverMemory
{
    namespace CoverP = NativeWorldCoverProfile;
    namespace CoverD = NativeStreamingCompletionContext;
    namespace CoverI = NativeImageAccess;
    namespace CoverE = NativeWorldPoolMatrixMemory;
    template <class T, class B>
    T CoverField(const B& bytes, std::size_t offset) noexcept
    {
        T value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
    template <class T, class B>
    void CoverPut(B& bytes, std::size_t offset, T value) noexcept
    {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }
    template <class T>
    bool CoverRead(std::uintptr_t address, T& value) noexcept
    {
        return CoverD::Copy(address, &value, sizeof(value));
    }
    bool CoverIndex(std::uintptr_t base, std::uintptr_t address, std::size_t stride, std::size_t count, std::size_t& index) noexcept
    {
        if (address < base || (address - base) % stride)
            return false;
        index = (address - base) / stride;
        return index < count;
    }
    bool CoverSlotWritable(std::uintptr_t image, std::uintptr_t slot) noexcept
    {
        return slot && slot % alignof(std::uintptr_t) == 0 &&
               (CoverI::ImagePages(image, slot, sizeof(std::uintptr_t), true) || CoverD::Writable(slot, sizeof(std::uintptr_t)));
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        if (!CoverE::Profile(image))
            return false;
        std::array<unsigned char, CoverP::MaximumPinBytes> bytes{};
        for (const auto& pin : CoverP::Pins)
            if (!pin.size || pin.size > bytes.size() || !CoverI::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        return true;
    }
    bool CoverResolve(std::uintptr_t image, std::uintptr_t address, CoverE::Entity& result)
    {
        namespace EP = NativeWorldIplLoadedProfile;
        for (unsigned candidate = 0; candidate != 2; ++candidate)
        {
            const auto     root = candidate ? EP::DummyRoot : EP::BuildingRoot;
            const auto     count = candidate ? EP::DummyCapacity : EP::BuildingCapacity;
            const auto     stride = candidate ? EP::DummyBytes : EP::BuildingBytes;
            std::uintptr_t pool = 0, objects = 0;
            std::size_t    index = 0;
            if (CoverI::ReadImage(image, image + root, &pool, sizeof(pool), true) && CoverRead(pool, objects) &&
                CoverIndex(objects, address, stride, count, index))
                return CoverE::CaptureEntity(image, candidate ? CoverE::Kind::Dummy : CoverE::Kind::Building, static_cast<std::uint32_t>(index), result) ==
                       ClockResult::Ok;
        }
        return false;
    }
    void CoverIdentity(Value& value)
    {
        const auto& w = value.world;
        value.identity = {w.root, w.gameMode, w.level, w.world, w.gameClass, w.levelClass, w.mode, w.count};
        for (std::size_t at = 0; at < w.count; ++at)
        {
            const auto& m = w.members[at];
            value.identity.insert(value.identity.end(), {m.objectAddress, m.vtableAddress, m.classAddress, m.outerAddress, m.itemAddress,
                                                         static_cast<std::uintptr_t>(m.index), m.serial});
        }
    }
    ClockResult Capture(std::uintptr_t image, Value& output, const Value* retained)
    {
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        Value value;
        auto  result = CoverD::Capture(image, value.world);
        if (result != ClockResult::Ok || (result = NativeModelReferenceHeap::Capture(image, value.heap)) != ClockResult::Ok)
            return result;
        CoverIdentity(value);
        if (!CoverI::ReadImage(image, image + CoverP::TableRva, value.points.data(), value.points.size(), true) ||
            !CoverI::ReadImage(image, image + CoverP::RefBankRva, value.references.data(), value.references.size(), true) ||
            !CoverI::ReadImage(image, image + CoverP::CountRva, &value.count, sizeof(value.count), true) ||
            !CoverI::ReadImage(image, image + CoverP::ListRootRva, &value.head, sizeof(value.head), true) ||
            !CoverI::ReadImage(image, image + CoverP::FreeRefRootRva, &value.free, sizeof(value.free), true) ||
            !CoverI::ReadImage(image, image + CoverP::DoublePoolRootRva, &value.pool, sizeof(value.pool), true) ||
            !CoverD::Writable(value.pool, value.header.size()) || !CoverRead(value.pool, value.header))
            return ClockResult::ReadFailure;
        value.objects = CoverField<std::uintptr_t>(value.header, CoverP::PoolObjectsOffset);
        value.flags = CoverField<std::uintptr_t>(value.header, CoverP::PoolFlagsOffset);
        const auto cursor = CoverField<std::int32_t>(value.header, CoverP::PoolCursorOffset);
        if (CoverField<std::int32_t>(value.header, CoverP::PoolCapacityOffset) != static_cast<std::int32_t>(CoverP::DoublePoolCapacity) || cursor < -1 ||
            cursor >= static_cast<std::int32_t>(CoverP::DoublePoolCapacity) || value.header[CoverP::PoolOwnsOffset] != 1 ||
            value.header[CoverP::PoolOwnsOffset + 1])
            return ClockResult::BindingChanged;
        value.nodes.resize(CoverP::DoublePoolCapacity * CoverP::DoublePoolStride);
        value.generations.resize(CoverP::DoublePoolCapacity);
        if (!CoverD::Writable(value.objects, value.nodes.size()) || !CoverD::Writable(value.flags, value.generations.size()) ||
            !NativeModelReferenceMap::Disjoint(value.pool, value.header.size(), value.objects, value.nodes.size()) ||
            !NativeModelReferenceMap::Disjoint(value.pool, value.header.size(), value.flags, value.generations.size()) ||
            !NativeModelReferenceMap::Disjoint(value.objects, value.nodes.size(), value.flags, value.generations.size()) ||
            !CoverD::Copy(value.objects, value.nodes.data(), value.nodes.size()) ||
            !CoverD::Copy(value.flags, value.generations.data(), value.generations.size()))
            return ClockResult::BindingChanged;
        const auto addOwner = [&](std::uintptr_t address) -> bool
        {
            if (std::any_of(value.owners.begin(), value.owners.end(), [&](const auto& owner) { return owner.address == address; }))
                return true;
            CoverE::Entity entity;
            if (!CoverResolve(image, address, entity))
                return false;
            value.owners.push_back(entity);
            return true;
        };
        std::vector<bool> listed(CoverP::DoublePoolCapacity);
        auto              node = value.head;
        std::uintptr_t    previous = 0;
        while (node)
        {
            std::size_t index = 0;
            if (!CoverIndex(value.objects, node, CoverP::DoublePoolStride, CoverP::DoublePoolCapacity, index) || listed[index] ||
                (value.generations[index] & 0x80))
                return ClockResult::BindingChanged;
            listed[index] = true;
            value.order.push_back(index);
            const auto at = index * CoverP::DoublePoolStride;
            if (CoverField<std::uintptr_t>(value.nodes, at + CoverP::NodePreviousOffset) != previous ||
                !addOwner(CoverField<std::uintptr_t>(value.nodes, at + CoverP::NodeItemOffset)))
                return ClockResult::UnsupportedBuild;
            previous = node;
            node = CoverField<std::uintptr_t>(value.nodes, at + CoverP::NodeNextOffset);
        }
        if (retained)
            for (const auto& owner : retained->owners)
                if (!addOwner(owner.address))
                    return ClockResult::BindingChanged;
        std::vector<bool> refs(CoverP::RefCapacity);
        const auto        refBase = image + CoverP::RefBankRva;
        // A registered reference slot may be unrelated to covers. Preserve it
        // and require its current owner rather than discarding the whole list.
        for (const auto& owner : value.owners)
        {
            auto ref = CoverField<std::uintptr_t>(owner.bytes, CoverP::EntityRefsOffset);
            while (ref)
            {
                std::size_t index = 0;
                if (!CoverIndex(refBase, ref, CoverP::RefBytes, CoverP::RefCapacity, index) || refs[index])
                    return ClockResult::BindingChanged;
                refs[index] = true;
                Slot slot{CoverField<std::uintptr_t>(value.references, index * CoverP::RefBytes + CoverP::RefSlotOffset), 0};
                if (!CoverSlotWritable(image, slot.address) || !CoverRead(slot.address, slot.value) || slot.value != owner.address ||
                    std::any_of(value.slots.begin(), value.slots.end(), [&](const auto& old) { return old.address == slot.address; }))
                    return ClockResult::BindingChanged;
                value.slots.push_back(slot);
                ref = CoverField<std::uintptr_t>(value.references, index * CoverP::RefBytes + CoverP::RefNextOffset);
            }
        }
        node = value.free;
        while (node)
        {
            std::size_t index = 0;
            if (!CoverIndex(refBase, node, CoverP::RefBytes, CoverP::RefCapacity, index) || refs[index] ||
                CoverField<std::uintptr_t>(value.references, index * CoverP::RefBytes + CoverP::RefSlotOffset))
                return ClockResult::BindingChanged;
            refs[index] = true;
            node = CoverField<std::uintptr_t>(value.references, index * CoverP::RefBytes + CoverP::RefNextOffset);
        }
        // Unselected owners need no receiver calls, but their reference nodes
        // must not alias a selected/free chain that cleanup will relink. The
        // real static bank gives a finite partition without guessing heap rows.
        for (std::size_t index = 0; index < CoverP::RefCapacity; ++index)
        {
            if (refs[index])
                continue;
            const auto  offset = index * CoverP::RefBytes;
            const auto  next = CoverField<std::uintptr_t>(value.references, offset + CoverP::RefNextOffset);
            std::size_t nextIndex = 0;
            if (next && (!CoverIndex(refBase, next, CoverP::RefBytes, CoverP::RefCapacity, nextIndex) || refs[nextIndex]))
                return ClockResult::BindingChanged;
            Slot slot{CoverField<std::uintptr_t>(value.references, offset + CoverP::RefSlotOffset), 0};
            if (!CoverSlotWritable(image, slot.address) || !CoverRead(slot.address, slot.value) || !slot.value ||
                std::any_of(value.owners.begin(), value.owners.end(), [&](const auto& owner) { return owner.address == slot.value; }) ||
                std::any_of(value.slots.begin(), value.slots.end(), [&](const auto& old) { return old.address == slot.address; }))
                return ClockResult::BindingChanged;
            value.slots.push_back(slot);
        }
        std::uint32_t active = 0;
        for (std::size_t index = 0; index < CoverP::PointCapacity; ++index)
        {
            const auto at = index * CoverP::PointBytes;
            if (!value.points[at + CoverP::PointTypeOffset])
                continue;
            ++active;
            const auto ownerAddress = CoverField<std::uintptr_t>(value.points, at + CoverP::PointOwnerOffset);
            const auto owner = std::find_if(value.owners.begin(), value.owners.end(), [&](const auto& row) { return row.address == ownerAddress; });
            if (owner != value.owners.end() && !(owner->kind == CoverE::Kind::Building && owner->bytes[0x56] == 0) &&
                std::none_of(value.slots.begin(), value.slots.end(),
                             [&](const auto& slot) { return slot.address == image + CoverP::TableRva + at + CoverP::PointOwnerOffset; }))
                return ClockResult::BindingChanged;
        }
        if (active != value.count || !Seal(image, value))
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    bool Seal(std::uintptr_t image, const Value& value) noexcept
    {
        if (!Profile(image) || !CoverD::Fence(image, value.world) || NativeModelReferenceHeap::Recheck(image, value.heap) != ClockResult::Ok)
            return false;
        const auto equal = [&](std::uintptr_t address, const auto& bytes, bool owned) -> bool
        {
            std::vector<unsigned char> raw(bytes.size());
            return (owned ? CoverI::ReadImage(image, address, raw.data(), raw.size(), true) : CoverD::Copy(address, raw.data(), raw.size())) &&
                   std::equal(raw.begin(), raw.end(), bytes.begin());
        };
        // Allocation failures are handled by the caller's retained journal.
        try
        {
            std::uintptr_t pool = 0, head = 0, free = 0;
            std::uint32_t  count = 0;
            if (!CoverI::ReadImage(image, image + CoverP::DoublePoolRootRva, &pool, sizeof(pool), true) || pool != value.pool ||
                !CoverI::ReadImage(image, image + CoverP::ListRootRva, &head, sizeof(head), true) || head != value.head ||
                !CoverI::ReadImage(image, image + CoverP::FreeRefRootRva, &free, sizeof(free), true) || free != value.free ||
                !CoverI::ReadImage(image, image + CoverP::CountRva, &count, sizeof(count), true) || count != value.count ||
                !equal(image + CoverP::TableRva, value.points, true) || !equal(image + CoverP::RefBankRva, value.references, true) ||
                !equal(value.pool, value.header, false) || !equal(value.objects, value.nodes, false) || !equal(value.flags, value.generations, false))
                return false;
            for (const auto& owner : value.owners)
                if (!CoverE::SealEntity(image, owner))
                    return false;
            for (const auto& slot : value.slots)
            {
                std::uintptr_t current = 0;
                if (!CoverSlotWritable(image, slot.address) || !CoverRead(slot.address, current) || current != slot.value)
                    return false;
            }
            NativeStreamingCompletionContext::Value current;
            return CoverD::Capture(image, current) == ClockResult::Ok && CoverD::Same(value.world, current, true) && CoverD::Fence(image, current) &&
                   NativeModelReferenceHeap::Recheck(image, value.heap) == ClockResult::Ok;
        }
        catch (...)
        {
            return false;
        }
    }
    bool Same(const Value& a, const Value& b) noexcept
    {
        if (a.identity != b.identity || !NativeModelReferenceHeap::Same(a.heap, b.heap) || a.points != b.points || a.references != b.references ||
            a.header != b.header || a.pool != b.pool || a.objects != b.objects || a.flags != b.flags || a.head != b.head || a.free != b.free ||
            a.count != b.count || a.nodes != b.nodes || a.generations != b.generations || a.owners.size() != b.owners.size())
            return false;
        for (std::size_t i = 0; i < a.owners.size(); ++i)
        {
            const auto& x = a.owners[i];
            const auto& y = b.owners[i];
            if (x.address != y.address || x.kind != y.kind || x.index != y.index || x.generation != y.generation || x.pool != y.pool || x.root != y.root ||
                x.objects != y.objects || x.flags != y.flags || x.stride != y.stride || x.capacity != y.capacity || x.header != y.header || x.bytes != y.bytes)
                return false;
        }
        // Slot order changes when a native reference is removed. The point/ref
        // byte preimages already prove removed slots; compare remaining slots.
        if (a.slots.size() != b.slots.size())
            return false;
        for (const auto& slot : a.slots)
            if (std::none_of(b.slots.begin(), b.slots.end(), [&](const auto& other) { return slot.address == other.address && slot.value == other.value; }))
                return false;
        return true;
    }
    bool ExpectRemove(std::uintptr_t image, Value& value, std::uintptr_t address) noexcept
    {
        auto owner = std::find_if(value.owners.begin(), value.owners.end(), [&](const auto& row) { return row.address == address; });
        if (owner == value.owners.end())
            return false;
        for (std::size_t point = 0; point < CoverP::PointCapacity; ++point)
        {
            const auto at = point * CoverP::PointBytes;
            if (!value.points[at] || CoverField<std::uintptr_t>(value.points, at + CoverP::PointOwnerOffset) != address)
                continue;
            const auto  slot = image + CoverP::TableRva + at + CoverP::PointOwnerOffset;
            auto        ref = CoverField<std::uintptr_t>(owner->bytes, CoverP::EntityRefsOffset);
            std::size_t previous = CoverP::RefCapacity;
            while (ref)
            {
                std::size_t index = 0;
                if (!CoverIndex(image + CoverP::RefBankRva, ref, CoverP::RefBytes, CoverP::RefCapacity, index))
                    return false;
                const auto offset = index * CoverP::RefBytes;
                const auto next = CoverField<std::uintptr_t>(value.references, offset + CoverP::RefNextOffset);
                if (CoverField<std::uintptr_t>(value.references, offset + CoverP::RefSlotOffset) == slot)
                {
                    if (previous == CoverP::RefCapacity)
                        CoverPut(owner->bytes, CoverP::EntityRefsOffset, next);
                    else
                        CoverPut(value.references, previous * CoverP::RefBytes + CoverP::RefNextOffset, next);
                    CoverPut(value.references, offset + CoverP::RefNextOffset, value.free);
                    value.free = ref;
                    CoverPut(value.references, offset + CoverP::RefSlotOffset, std::uintptr_t{0});
                    value.slots.erase(std::remove_if(value.slots.begin(), value.slots.end(), [&](const auto& old) { return old.address == slot; }),
                                      value.slots.end());
                    break;
                }
                previous = index;
                ref = next;
            }
            if (!value.count)
                return false;
            --value.count;
            value.points[at] = 0;
            CoverPut(value.points, at + CoverP::PointOwnerOffset, std::uintptr_t{0});
        }
        return true;
    }
    bool ExpectClear(Value& value) noexcept
    {
        for (const auto index : value.order)
        {
            const auto at = index * CoverP::DoublePoolStride;
            const auto next = CoverField<std::uintptr_t>(value.nodes, at + CoverP::NodeNextOffset);
            const auto previous = CoverField<std::uintptr_t>(value.nodes, at + CoverP::NodePreviousOffset);
            if (value.head == value.objects + at)
                value.head = next;
            std::size_t other = 0;
            if (previous)
            {
                if (!CoverIndex(value.objects, previous, CoverP::DoublePoolStride, CoverP::DoublePoolCapacity, other))
                    return false;
                CoverPut(value.nodes, other * CoverP::DoublePoolStride + CoverP::NodeNextOffset, next);
            }
            if (next)
            {
                if (!CoverIndex(value.objects, next, CoverP::DoublePoolStride, CoverP::DoublePoolCapacity, other))
                    return false;
                CoverPut(value.nodes, other * CoverP::DoublePoolStride + CoverP::NodePreviousOffset, previous);
            }
            value.generations[index] |= 0x80;
            const auto cursor = CoverField<std::int32_t>(value.header, CoverP::PoolCursorOffset);
            if (static_cast<std::int32_t>(index) < cursor)
                CoverPut(value.header, CoverP::PoolCursorOffset, static_cast<std::int32_t>(index));
        }
        value.order.clear();
        return value.head == 0;
    }
}
