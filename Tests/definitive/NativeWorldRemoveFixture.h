/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned sector/LOD resources and explicit final-original unlink double
 ****************************************************************************/
#pragma once
#include "NativeWorldPoolMatrixFixture.h"
#include "NativeWorldBoundRectFixture.h"
#include "../../Client/game_de/NativeWorldRemoveMemory.h"
#include "../../Client/game_de/NativeWorldPoolMatrixProfile.h"
#include <array>
#include <cstring>
#include <vector>
namespace NativeWorldRemoveFixture
{
    namespace P = mta::game_de::NativeWorldAddProfile;
    namespace E = mta::game_de::NativeWorldIplLoadedProfile;
    namespace W = NativeWorldIplFixture;
    using Kind = mta::game_de::NativeWorldRemoveMemory::Kind;
    struct State
    {
        NativeWorldPoolMatrixFixture::State entities;
        W::Pool                             singles{static_cast<std::int32_t>(P::SingleCapacity), P::SingleBytes};
        W::Pool                             doubles{static_cast<std::int32_t>(P::DoubleCapacity), P::DoubleBytes};
        std::uintptr_t                      image = 0;
        W::Writer                           writer = nullptr;
        void*                               host = nullptr;
        Kind                                kind = Kind::Building;
        bool                                lod = false;
        State(std::uintptr_t selectedImage, W::Writer selectedWriter, void* selectedHost)
            : entities(selectedImage, selectedWriter, selectedHost), image(selectedImage), writer(selectedWriter), host(selectedHost)
        {
            Reset();
        }
        bool           Ready() const noexcept { return entities.Ready() && singles.Ready() && doubles.Ready(); }
        std::uintptr_t Entity() const noexcept { return kind == Kind::Building ? entities.buildings.Row(0) : entities.dummies.Row(0); }
        W::Pool&       Nodes() noexcept { return !lod && kind == Kind::Dummy ? doubles : singles; }
        std::uintptr_t Head(unsigned x) const noexcept
        {
            const auto cell = std::size_t{4} * (lod ? P::LodSide : P::SectorSide) + x;
            return image + (lod ? P::LodRva + cell * 8 : P::SectorsRva + cell * 16 + (kind == Kind::Dummy ? 8 : 0));
        }
        template <class T>
        static T Read(std::uintptr_t address, std::size_t offset = 0)
        {
            T value{};
            std::memcpy(&value, reinterpret_cast<void*>(address + offset), sizeof(value));
            return value;
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        void SetupBuilding() { Setup(Kind::Building, false); }
        void SetupDummy() { Setup(Kind::Dummy, false); }
        void SetupLod(Kind selected = Kind::Building) { Setup(selected, true); }
        void Reset() { SetupBuilding(); }
        void Setup(Kind selected, bool selectedLod)
        {
            if (!Ready())
                return;
            kind = selected;
            lod = selectedLod;
            entities.Reset();
            singles.Reset();
            doubles.Reset();
            for (const auto& pin : P::Code)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& scalar : P::Scalars)
                writer(host, scalar.rva, &scalar.bits, sizeof(scalar.bits), PAGE_READONLY);
            NativeWorldBoundRectFixture::Install(image, writer, host);
            writer(host, mta::game_de::NativeWorldPoolMatrixProfile::WorldRemoveRva, mta::game_de::NativeWorldPoolMatrixProfile::WorldRemoveBytes.data(),
                   mta::game_de::NativeWorldPoolMatrixProfile::WorldRemoveBytes.size(), PAGE_EXECUTE_READ);
            writer(host, 0x1132490, E::EntityWorldRemoveBytes.data(), E::EntityWorldRemoveBytes.size(), PAGE_EXECUTE_READ);
            for (const auto table : {E::BuildingVtable, E::SpecialBuildingVtable, E::DummyVtable})
                Global(table + 0x20, image + 0x1132490, PAGE_READONLY);
            Global(P::SingleRootRva, singles.Address());
            Global(P::DoubleRootRva, doubles.Address());
            std::vector<unsigned char> sectors(P::SectorSide * P::SectorSide * 16), lodHeads(P::LodSide * P::LodSide * 8);
            writer(host, P::SectorsRva, sectors.data(), sectors.size(), PAGE_READWRITE);
            writer(host, P::LodRva, lodHeads.data(), lodHeads.size(), PAGE_READWRITE);
            auto& typed = kind == Kind::Building ? entities.buildings : entities.dummies;
            typed.flags[1] = 3;
            std::memcpy(reinterpret_cast<void*>(typed.Row(1)), reinterpret_cast<void*>(typed.Row(0)), typed.stride);
            W::Put(Entity(), 0x30, lod ? std::uint32_t{P::LodFlag} : std::uint32_t{0});
            const std::array<unsigned char, 4> ranges{2, 3, 4, 4};
            std::memcpy(reinterpret_cast<void*>(Entity() + 0x3c), ranges.data(), ranges.size());
            auto& pool = Nodes();
            W::Put(pool.Address(), 0x14, std::int32_t{100});
            const std::array<unsigned char, 5> generations{127, 3, 6, 9, 11};
            for (std::size_t index = 0; index != 5; ++index)
            {
                pool.flags[index] = generations[index];
                W::Put(pool.Row(index), 0, (index == 1 || index == 3) ? Entity() : typed.Row(1));
                W::Put(pool.Row(index), 8, index == 2 || index == 4 ? std::uintptr_t{0} : pool.Row(index + 1));
                if (pool.stride == P::DoubleBytes)
                    W::Put(pool.Row(index), 0x10, index == 0 || index == 3 ? std::uintptr_t{0} : pool.Row(index - 1));
            }
            W::Put(Head(2), 0, pool.Row(0));
            W::Put(Head(3), 0, pool.Row(3));
        }
        bool Remove(std::uintptr_t entity)
        {
            // Only this labelled final-original double mutates fixture nodes.
            // It traverses actual lists independently of the production
            // preflight's expected bytes, mirroring the two native list cases.
            if (entity != Entity())
                return false;
            auto& pool = Nodes();
            for (unsigned x = 2; x <= 3; ++x)
            {
                const auto  head = Head(x);
                auto        node = Read<std::uintptr_t>(head), previous = std::uintptr_t{0};
                std::size_t visited = 0;
                while (node && Read<std::uintptr_t>(node) != entity)
                {
                    if (++visited > static_cast<std::size_t>(pool.capacity))
                        return false;
                    previous = node;
                    node = Read<std::uintptr_t>(node, 8);
                }
                if (!node)
                    continue;
                const auto next = Read<std::uintptr_t>(node, 8);
                if (Read<std::uintptr_t>(head) == node)
                    W::Put(head, 0, next);
                if (pool.stride == P::DoubleBytes)
                    previous = Read<std::uintptr_t>(node, 0x10);
                if (previous)
                    W::Put(previous, 8, next);
                if (pool.stride == P::DoubleBytes && next)
                    W::Put(next, 0x10, previous);
                if (node < pool.Row(0) || (node - pool.Row(0)) % pool.stride)
                    return false;
                const auto index = (node - pool.Row(0)) / pool.stride;
                if (index >= static_cast<std::size_t>(pool.capacity))
                    return false;
                pool.flags[index] |= 0x80;
                if (static_cast<std::int32_t>(index) < Read<std::int32_t>(pool.Address(), 0x14))
                    W::Put(pool.Address(), 0x14, static_cast<std::int32_t>(index));
            }
            return true;
        }
    };
}
