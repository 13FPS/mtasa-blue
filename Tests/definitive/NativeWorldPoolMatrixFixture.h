/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned native matrix bank and exact typed Building/Dummy pool rows
 ****************************************************************************/
#pragma once
#include "NativeWorldIplFixture.h"
#include "../../Client/game_de/NativeWorldPoolMatrixMemory.h"
#include "../../Client/game_de/NativeWorldIplLoadedProfile.h"
#include "../../Client/game_de/NativeBuildingPoolDefaultProfile.h"
#include <array>
#include <cmath>
#include <cstring>
namespace NativeWorldPoolMatrixFixture
{
    namespace MatrixP = mta::game_de::NativeWorldPoolMatrixProfile;
    namespace MatrixMem = mta::game_de::NativeWorldPoolMatrixMemory;
    namespace EntityP = mta::game_de::NativeWorldIplLoadedProfile;
    namespace W = NativeWorldIplFixture;
    struct State
    {
        W::Pool        buildings{EntityP::BuildingCapacity, EntityP::BuildingBytes}, dummies{EntityP::DummyCapacity, EntityP::DummyBytes};
        unsigned char* allocation = nullptr;
        std::uintptr_t image = 0;
        W::Writer      writer = nullptr;
        void*          host = nullptr;
        State(std::uintptr_t selectedImage, W::Writer selectedWriter, void* selectedHost) : image(selectedImage), writer(selectedWriter), host(selectedHost)
        {
            allocation =
                static_cast<unsigned char*>(VirtualAlloc(nullptr, MatrixP::BankBytes + MatrixP::CookieBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Reset();
        }
        ~State()
        {
            if (allocation)
                VirtualFree(allocation, 0, MEM_RELEASE);
        }
        State(const State&) = delete;
        State&         operator=(const State&) = delete;
        bool           Ready() const noexcept { return allocation && buildings.Ready() && dummies.Ready(); }
        std::uintptr_t Bank() const noexcept { return reinterpret_cast<std::uintptr_t>(allocation) + MatrixP::CookieBytes; }
        std::uintptr_t Node(std::size_t index) const noexcept { return Bank() + index * MatrixP::NodeBytes; }
        template <class V>
        void Global(std::uintptr_t rva, const V& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        static std::uintptr_t Pointer(std::uintptr_t address, std::size_t offset)
        {
            std::uintptr_t value = 0;
            std::memcpy(&value, reinterpret_cast<void*>(address + offset), sizeof(value));
            return value;
        }
        std::uintptr_t Head(MatrixMem::List list) const noexcept
        {
            return image + (list == MatrixMem::List::Dynamic  ? MatrixP::DynamicStart
                            : list == MatrixMem::List::Static ? MatrixP::StaticStart
                                                              : MatrixP::FreeStart);
        }
        std::uintptr_t Tail(MatrixMem::List list) const noexcept { return Head(list) + MatrixP::NodeBytes; }
        void           Reset()
        {
            if (!Ready())
                return;
            for (const auto& pin : MatrixP::MatrixPins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : mta::game_de::NativeBuildingPoolDefaultProfile::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            writer(host, 0x11716ef, EntityP::DummyPoolBytes.data(), EntityP::DummyPoolBytes.size(), PAGE_EXECUTE_READ);
            writer(host, 0x112e820, EntityP::DummyAllocateBytes.data(), EntityP::DummyAllocateBytes.size(), PAGE_EXECUTE_READ);
            writer(host, 0x1141700, EntityP::IplConstructBytes.data(), EntityP::IplConstructBytes.size(), PAGE_EXECUTE_READ);
            std::uintptr_t tableEntry = image + 0x10367b0;
            Global(EntityP::BuildingVtable, tableEntry, PAGE_READONLY);
            Global(EntityP::SpecialBuildingVtable, tableEntry, PAGE_READONLY);
            tableEntry = image + 0x1212120;
            Global(EntityP::DummyVtable, tableEntry, PAGE_READONLY);
            buildings.Reset();
            dummies.Reset();
            Global(EntityP::BuildingRoot, buildings.Address());
            Global(EntityP::DummyRoot, dummies.Address());
            for (auto* pool : {&buildings, &dummies})
            {
                pool->flags[0] = 1;
                W::Put(pool->Row(0), 0, image + (pool == &buildings ? EntityP::BuildingVtable : EntityP::DummyVtable));
                W::Put(pool->Row(0), 0x3a, std::int16_t{17});
                W::Put(pool->Row(0), 0x6a, static_cast<unsigned char>(pool == &buildings ? 1 : 5));
            }
            std::memset(allocation, 0, MatrixP::CookieBytes + MatrixP::BankBytes);
            W::Put(reinterpret_cast<std::uintptr_t>(allocation), 0, std::uint64_t{MatrixP::Capacity});
            Global(MatrixP::BankRva, Bank());
            std::array<unsigned char, 6 * MatrixP::NodeBytes> sentinels{};
            writer(host, MatrixP::DynamicStart, sentinels.data(), sentinels.size(), PAGE_READWRITE);
            for (auto list : {MatrixMem::List::Dynamic, MatrixMem::List::Static, MatrixMem::List::Free})
            {
                W::Put(Head(list), MatrixP::NextOffset, Tail(list));
                W::Put(Tail(list), MatrixP::PrevOffset, Head(list));
            }
            for (std::size_t index = 0; index != MatrixP::Capacity; ++index)
            {
                W::Put(Node(index), MatrixP::PrevOffset, index ? Node(index - 1) : Head(MatrixMem::List::Free));
                W::Put(Node(index), MatrixP::NextOffset, index + 1 < MatrixP::Capacity ? Node(index + 1) : Tail(MatrixMem::List::Free));
                Identity(index);
            }
            W::Put(Head(MatrixMem::List::Free), MatrixP::NextOffset, Node(0));
            W::Put(Tail(MatrixMem::List::Free), MatrixP::PrevOffset, Node(MatrixP::Capacity - 1));
        }
        void Identity(std::size_t index)
        {
            // Same twelve scalar identity fields as native static allocation;
            // its existing padding/attached/owner/list fields are not copied.
            for (std::size_t row = 0; row != 4; ++row)
                for (std::size_t column = 0; column != 3; ++column)
                    W::Put(Node(index), row * 16 + column * 4, float{row == column ? 1.0f : 0.0f});
        }
        void Move(std::size_t index, MatrixMem::List destination, std::uintptr_t owner = 0)
        {
            const auto address = Node(index), previous = Pointer(address, MatrixP::PrevOffset), next = Pointer(address, MatrixP::NextOffset);
            W::Put(previous, MatrixP::NextOffset, next);
            W::Put(next, MatrixP::PrevOffset, previous);
            const auto first = Pointer(Head(destination), MatrixP::NextOffset);
            W::Put(address, MatrixP::PrevOffset, Head(destination));
            W::Put(address, MatrixP::NextOffset, first);
            W::Put(first, MatrixP::PrevOffset, address);
            W::Put(Head(destination), MatrixP::NextOffset, address);
            W::Put(address, MatrixP::OwnerOffset, owner);
            if (owner)
                W::Put(owner, 0x18, address);
        }
        bool Remove(std::uintptr_t entity)
        {
            const auto matrix = Pointer(entity, 0x18);
            if (matrix < Bank() || (matrix - Bank()) % MatrixP::NodeBytes || (matrix - Bank()) / MatrixP::NodeBytes >= MatrixP::Capacity)
                return false;
            const auto index = (matrix - Bank()) / MatrixP::NodeBytes;
            float      x = 0, y = 0;
            std::memcpy(&x, reinterpret_cast<void*>(matrix + 0x10), 4);
            std::memcpy(&y, reinterpret_cast<void*>(matrix + 0x14), 4);
            const float heading = std::atan2(-x, y);
            std::memcpy(reinterpret_cast<void*>(entity + 8), reinterpret_cast<void*>(matrix + 0x30), 12);
            W::Put(entity, 0x14, heading);
            W::Put(entity, 0x18, std::uintptr_t{0});
            Move(index, MatrixMem::List::Free);
            return true;
        }
        bool Allocate(std::uintptr_t entity)
        {
            const auto first = Pointer(Head(MatrixMem::List::Free), MatrixP::NextOffset);
            if (first == Tail(MatrixMem::List::Free) || first < Bank())
                return false;
            const auto index = (first - Bank()) / MatrixP::NodeBytes;
            if (index >= MatrixP::Capacity)
                return false;
            Move(index, MatrixMem::List::Static, entity);
            Identity(index);
            return true;
        }
    };
}
