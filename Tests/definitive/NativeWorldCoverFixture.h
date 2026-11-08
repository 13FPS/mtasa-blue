/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned cover state and explicitly simulated original native effects
 ****************************************************************************/
#pragma once
#include "NativeWorldPoolMatrixFixture.h"
#include "../../Client/game_de/NativeWorldCoverProfile.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace NativeWorldCoverFixture
{
    namespace CoverP = mta::game_de::NativeWorldCoverProfile;
    namespace CoverEntityP = mta::game_de::NativeWorldIplLoadedProfile;
    namespace CoverW = NativeWorldIplFixture;
    struct State
    {
        // Reuse the existing native typed Building/Dummy pool fixture. Every
        // selected entity has matrix18 null: cover cleanup does not borrow or
        // require a matrix bank even though the shared builder owns one.
        NativeWorldPoolMatrixFixture::State entities;
        CoverW::Pool                        nodes{static_cast<std::int32_t>(CoverP::DoublePoolCapacity), CoverP::DoublePoolStride};
        alignas(std::uintptr_t) std::array<unsigned char, 8> unrelatedSlot{};
        std::uintptr_t image = 0;
        CoverW::Writer writer = nullptr;
        void*          host = nullptr;

        State(std::uintptr_t selectedImage, CoverW::Writer selectedWriter, void* selectedHost)
            : entities(selectedImage, selectedWriter, selectedHost), image(selectedImage), writer(selectedWriter), host(selectedHost)
        {
            Setup();
        }
        State(const State&) = delete;
        State&         operator=(const State&) = delete;
        bool           Ready() const noexcept { return entities.Ready() && nodes.Ready(); }
        std::uintptr_t Point(std::size_t index) const noexcept { return image + CoverP::TableRva + index * CoverP::PointBytes; }
        std::uintptr_t Reference(std::size_t index) const noexcept { return image + CoverP::RefBankRva + index * CoverP::RefBytes; }
        std::uintptr_t Node(std::size_t index) const noexcept { return nodes.Row(index); }
        template <class T>
        static T Read(std::uintptr_t address, std::size_t offset = 0)
        {
            T value{};
            std::memcpy(&value, reinterpret_cast<const void*>(address + offset), sizeof(value));
            return value;
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        bool RefIndex(std::uintptr_t address, std::size_t& index) const noexcept
        {
            if (address < Reference(0) || (address - Reference(0)) % CoverP::RefBytes)
                return false;
            index = (address - Reference(0)) / CoverP::RefBytes;
            return index < CoverP::RefCapacity;
        }
        void PointOwner(std::size_t index, unsigned char type, std::uintptr_t owner)
        {
            CoverW::Put(Point(index), CoverP::PointTypeOffset, type);
            CoverW::Put(Point(index), CoverP::PointOwnerOffset, owner);
        }
        void Register(std::size_t index, std::uintptr_t entity, std::uintptr_t slot)
        {
            CoverW::Put(Reference(index), CoverP::RefNextOffset, Read<std::uintptr_t>(entity, CoverP::EntityRefsOffset));
            CoverW::Put(Reference(index), CoverP::RefSlotOffset, slot);
            CoverW::Put(entity, CoverP::EntityRefsOffset, Reference(index));
            CoverW::Put(slot, 0, entity);
        }
        void Setup()
        {
            if (!Ready())
                return;
            entities.Reset();
            nodes.Reset();
            for (const auto& pin : CoverP::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            Global(CoverP::DoublePoolRootRva, nodes.Address());
            std::array<unsigned char, CoverP::PointCapacity * CoverP::PointBytes> table{};
            for (std::size_t index = 0; index != CoverP::PointCapacity; ++index)
            {
                // Native removal touches only type and owner. Nonzero row
                // payloads make accidental row clearing visible to real ACKs.
                for (std::size_t byte = 1; byte != CoverP::PointBytes; ++byte)
                    table[index * CoverP::PointBytes + byte] = static_cast<unsigned char>(0xa1 + (index + byte) % 31);
            }
            writer(host, CoverP::TableRva, table.data(), table.size(), PAGE_READWRITE);
            std::array<unsigned char, CoverP::RefCapacity * CoverP::RefBytes> refs{};
            writer(host, CoverP::RefBankRva, refs.data(), refs.size(), PAGE_READWRITE);
            Global(CoverP::FreeRefRootRva, Reference(0));
            const std::array<std::size_t, 5> occupiedRefs{5, 6, 7, 9, 11};
            std::uintptr_t                   previous = 0;
            for (std::size_t index = 0; index != CoverP::RefCapacity; ++index)
            {
                if (std::find(occupiedRefs.begin(), occupiedRefs.end(), index) != occupiedRefs.end())
                    continue;
                if (previous)
                    CoverW::Put(previous, CoverP::RefNextOffset, Reference(index));
                previous = Reference(index);
            }
            CoverW::Put(previous, CoverP::RefNextOffset, std::uintptr_t{0});
            const auto building = entities.buildings.Row(0), dummy = entities.dummies.Row(0);
            CoverW::Put(building, 0x56, static_cast<unsigned char>(1));
            for (std::size_t index = 1; index != 3; ++index)
            {
                entities.buildings.flags[index] = static_cast<unsigned char>(index + 1);
                CoverW::Put(entities.buildings.Row(index), 0, image + CoverEntityP::BuildingVtable);
                CoverW::Put(entities.buildings.Row(index), 0x3a, std::int16_t{17});
                CoverW::Put(entities.buildings.Row(index), 0x6a, static_cast<unsigned char>(1));
            }
            const auto staticOwner = entities.buildings.Row(1), unlistedOwner = entities.buildings.Row(2);
            CoverW::Put(unlistedOwner, 0x56, static_cast<unsigned char>(1));
            PointOwner(0, 1, building);
            PointOwner(1, 3, dummy);
            PointOwner(2, 1, building);
            PointOwner(3, 1, staticOwner);
            PointOwner(4, 3, unlistedOwner);
            PointOwner(5, 0, dummy);
            Register(5, building, Point(0) + CoverP::PointOwnerOffset);
            Register(6, building, reinterpret_cast<std::uintptr_t>(unrelatedSlot.data()));
            Register(9, building, Point(2) + CoverP::PointOwnerOffset);
            Register(7, dummy, Point(1) + CoverP::PointOwnerOffset);
            Register(11, unlistedOwner, Point(4) + CoverP::PointOwnerOffset);
            Global(CoverP::CountRva, std::int32_t{5});
            const std::array<std::size_t, 3>    order{7, 2, 15};
            const std::array<std::uintptr_t, 3> owners{building, dummy, staticOwner};
            for (std::size_t at = 0; at != order.size(); ++at)
            {
                nodes.flags[order[at]] = static_cast<unsigned char>(at + 3);
                CoverW::Put(Node(order[at]), CoverP::NodeItemOffset, owners[at]);
                CoverW::Put(Node(order[at]), CoverP::NodeNextOffset, at + 1 == order.size() ? std::uintptr_t{0} : Node(order[at + 1]));
                CoverW::Put(Node(order[at]), CoverP::NodePreviousOffset, at ? Node(order[at - 1]) : std::uintptr_t{0});
            }
            // This unrelated occupied slot and the high initial cursor expose
            // overbroad pool clearing and skipped native cursor retirement.
            nodes.flags[20] = 11;
            CoverW::Put(Node(20), CoverP::NodeItemOffset, unlistedOwner);
            CoverW::Put(nodes.Address(), CoverP::PoolCursorOffset, std::int32_t{41});
            Global(CoverP::ListRootRva, Node(order[0]));
        }
        void Reset() { Setup(); }
        bool Remove(std::uintptr_t entity)
        {
            // Explicit final-original double: production admission, source
            // checks, receiver ownership and post-ACK remain the real code.
            // No function in the installed GTA executable is invoked here.
            auto free = Read<std::uintptr_t>(image + CoverP::FreeRefRootRva);
            auto count = Read<std::int32_t>(image + CoverP::CountRva);
            for (std::size_t point = 0; point != CoverP::PointCapacity; ++point)
            {
                if (!Read<unsigned char>(Point(point)) || Read<std::uintptr_t>(Point(point), CoverP::PointOwnerOffset) != entity)
                    continue;
                if (entity)
                {
                    auto        previousSlot = entity + CoverP::EntityRefsOffset;
                    auto        reference = Read<std::uintptr_t>(previousSlot);
                    std::size_t traversed = 0;
                    while (reference)
                    {
                        std::size_t index = 0;
                        if (++traversed > CoverP::RefCapacity || !RefIndex(reference, index))
                            return false;
                        const auto next = Read<std::uintptr_t>(reference, CoverP::RefNextOffset);
                        if (Read<std::uintptr_t>(reference, CoverP::RefSlotOffset) == Point(point) + CoverP::PointOwnerOffset)
                        {
                            CoverW::Put(previousSlot, 0, next);
                            CoverW::Put(reference, CoverP::RefNextOffset, free);
                            free = reference;
                            CoverW::Put(reference, CoverP::RefSlotOffset, std::uintptr_t{0});
                            CoverW::Put(image + CoverP::FreeRefRootRva, 0, free);
                            break;
                        }
                        previousSlot = reference;
                        reference = next;
                    }
                }
                --count;
                CoverW::Put(Point(point), CoverP::PointTypeOffset, static_cast<unsigned char>(0));
                CoverW::Put(image + CoverP::CountRva, 0, count);
                CoverW::Put(Point(point), CoverP::PointOwnerOffset, std::uintptr_t{0});
            }
            return true;
        }
        bool Clear()
        {
            // Simulate the whole native DoubleListClear, including neighbour
            // writes and lower-generation flag preservation, rather than
            // zeroing the table/list or substituting the manager initializer.
            auto        current = Read<std::uintptr_t>(image + CoverP::ListRootRva);
            std::size_t traversed = 0;
            while (current)
            {
                if (++traversed > CoverP::DoublePoolCapacity || current < Node(0) || (current - Node(0)) % CoverP::DoublePoolStride)
                    return false;
                const auto index = (current - Node(0)) / CoverP::DoublePoolStride;
                if (index >= CoverP::DoublePoolCapacity)
                    return false;
                const auto next = Read<std::uintptr_t>(current, CoverP::NodeNextOffset);
                const auto previous = Read<std::uintptr_t>(current, CoverP::NodePreviousOffset);
                if (Read<std::uintptr_t>(image + CoverP::ListRootRva) == current)
                    CoverW::Put(image + CoverP::ListRootRva, 0, next);
                if (previous)
                    CoverW::Put(previous, CoverP::NodeNextOffset, next);
                if (next)
                    CoverW::Put(next, CoverP::NodePreviousOffset, previous);
                nodes.flags[index] |= 0x80;
                if (static_cast<std::int32_t>(index) < Read<std::int32_t>(nodes.Address(), CoverP::PoolCursorOffset))
                    CoverW::Put(nodes.Address(), CoverP::PoolCursorOffset, static_cast<std::int32_t>(index));
                current = next;
            }
            return true;
        }
    };
}
