/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned loaded IPL/entity/UE graph, never an engine-code emulator
 ****************************************************************************/
#pragma once
#include "NativeWorldIplFixture.h"
#include "NativeModelObjectGroupFixture.h"
#include "../../Client/game_de/NativeWorldIplLoadedProfile.h"
#include "../../Client/game_de/NativeBuildingPoolDefaultProfile.h"
#include "../../Client/game_de/NativeModelTimeProfile.h"
#include <array>
#include <cstring>
namespace NativeWorldIplLoadedFixture
{
    namespace P = mta::game_de::NativeWorldIplLoadedProfile;
    namespace A = NativeModelObjectGroupFixture;
    namespace W = NativeWorldIplFixture;
    namespace R = mta::game_de::NativeModelReferenceMap;
    struct State
    {
        W::Pool buildings{P::BuildingCapacity, P::BuildingBytes}, dummies{P::DummyCapacity, P::DummyBytes}, objects{P::ObjectCapacity, P::ObjectBytes};
        alignas(std::uintptr_t) std::array<unsigned char, P::DynamicActorBytes> actor{};
        alignas(std::uintptr_t) std::array<unsigned char, P::ComponentBytes> component{}, child{};
        alignas(std::uintptr_t) std::array<std::array<unsigned char, 0x40>, 3> classes{};
        alignas(std::uintptr_t) std::array<unsigned char, 18 * 24> items{};
        std::array<std::uintptr_t, 1>                chunks{};
        std::array<std::array<std::uintptr_t, 2>, 3> chains{};
        alignas(std::uintptr_t) std::array<R::Row, 1> mapRows{};
        alignas(std::uintptr_t) std::array<std::uintptr_t, 2> reference{};
        std::array<std::uintptr_t, 1> children{};
        std::uintptr_t                image = 0, root = 0, provider = 0;
        W::Writer                     writer = nullptr;
        void*                         host = nullptr;
        W::State&                     ipl;
        State(std::uintptr_t selectedImage, std::uintptr_t selectedRoot, std::uintptr_t selectedProvider, W::Writer selectedWriter, void* selectedHost,
              W::State& selectedIpl)
            : image(selectedImage), root(selectedRoot), provider(selectedProvider), writer(selectedWriter), host(selectedHost), ipl(selectedIpl)
        {
        }
        bool Ready() const noexcept { return buildings.Ready() && dummies.Ready() && objects.Ready(); }
        template <class V>
        void Global(std::uintptr_t rva, const V& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        std::uintptr_t Actor() const noexcept { return reinterpret_cast<std::uintptr_t>(actor.data()); }
        std::uintptr_t Component() const noexcept { return reinterpret_cast<std::uintptr_t>(component.data()); }
        std::uintptr_t Child() const noexcept { return reinterpret_cast<std::uintptr_t>(child.data()); }
        std::uintptr_t Class(std::size_t i) const noexcept { return reinterpret_cast<std::uintptr_t>(classes[i].data()); }
        std::uintptr_t Items() const noexcept { return reinterpret_cast<std::uintptr_t>(items.data()); }
        void           Member(std::uintptr_t address, std::uint32_t index, std::uintptr_t type, std::uintptr_t outer, std::uintptr_t table)
        {
            W::Put(address, 0, image + table);
            W::Put(address, 8, std::uint32_t{0});
            W::Put(address, 0xc, static_cast<std::int32_t>(index));
            W::Put(address, 0x10, type);
            W::Put(address, 0x20, outer);
            W::Put(Items(), index * 24, address);
            W::Put(Items(), index * 24 + 0x10, index + 100);
        }
        void Entity(W::Pool& pool, std::size_t index, std::uintptr_t table, unsigned char type, unsigned char iplnum = 1)
        {
            const auto address = pool.Row(index);
            pool.flags[index] = 1;
            W::Put(address, 0, image + table);
            W::Put(address, 0x3a, std::int16_t{17});
            W::Put(address, 0x56, iplnum);
            W::Put(address, 0x6a, type);
            if (type == 4)
                W::Put(address, 0x1c6, std::int16_t{-1});
        }
        template <class Graph>
        void Reset(const Graph& graph, bool dynamic = false)
        {
            for (const auto& pin : P::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : mta::game_de::NativeBuildingPoolDefaultProfile::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : mta::game_de::NativeModelTimeProfile::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : mta::game_de::NativeModelReferenceProfile::Code)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& slot : P::Slots)
                Global(slot.rva, image + slot.target, PAGE_READONLY);
            A::Install(image, writer, host, 1, 17);
            Global(A::P::PoolRva + 0x22, std::uint16_t{1});
            buildings.Reset();
            dummies.Reset();
            objects.Reset();
            actor.fill(0);
            component.fill(0);
            child.fill(0);
            items.fill(0);
            for (auto& type : classes)
                type.fill(0);
            Global(P::BuildingRoot, buildings.Address());
            Global(P::DummyRoot, dummies.Address());
            Global(P::ObjectRoot, objects.Address());
            std::memcpy(items.data(), graph.items.data(), graph.items.size());
            const auto ownerTable = mta::game_de::NativeStartupWorldProfile::ownerVtableRva;
            Member(provider, 11, graph.Class(0), root, 0x4301010);
            for (std::size_t i = 0; i != 3; ++i)
            {
                Member(Class(i), static_cast<std::uint32_t>(12 + i), graph.Class(0), 0, ownerTable);
                chains[i][0] = Class(i) + 0x30;
                W::Put(Class(i), 0x30, reinterpret_cast<std::uintptr_t>(chains[i].data()));
                W::Put(Class(i), 0x38, std::int32_t{0});
            }
            chains[1] = {Class(0) + 0x30, Class(1) + 0x30};
            W::Put(Class(1), 0x38, std::int32_t{1});
            Member(Actor(), 15, Class(dynamic ? 1 : 0), graph.Level(), dynamic ? P::DynamicActorVtable : P::ActorVtable);
            Member(Component(), 16, Class(2), Actor(), P::ComponentVtable);
            Member(Child(), 17, Class(2), Actor(), P::ComponentVtable);
            W::Put(Actor(), 0x130, Component());
            W::Put(Actor(), 0x298, Component());
            W::Put(Component(), 0x14f, static_cast<unsigned char>(1));
            Global(P::ActorClass, Class(0));
            Global(P::DynamicActorClass, Class(1));
            Global(P::ComponentClass, Class(2));
            chunks[0] = Items();
            Global(mta::game_de::NativeStartupWorldProfile::objectChunksRva, reinterpret_cast<std::uintptr_t>(chunks.data()));
            Global(mta::game_de::NativeStartupWorldProfile::objectCountRva, std::int32_t{18});
            // Use the actual root-owned sparse model reference map. The
            // whole original remains a final explicit test double, so this
            // ledger is neither substituted for nor executed as UE cleanup.
            const auto map = root + mta::game_de::NativeModelReferenceProfile::MapOffset;
            std::memset(reinterpret_cast<void*>(map), 0, mta::game_de::NativeModelReferenceProfile::HeaderBytes);
            mapRows[0] = {17, 1, -1, 0};
            W::Put(map, 0, reinterpret_cast<std::uintptr_t>(mapRows.data()));
            W::Put(map, 8, std::int32_t{1});
            W::Put(map, 0xc, std::int32_t{1});
            W::Put(map, 0x10, std::uint32_t{1});
            W::Put(map, 0x28, std::int32_t{1});
            W::Put(map, 0x2c, std::int32_t{128});
            W::Put(map, 0x30, std::int32_t{-1});
            W::Put(map, 0x38, std::int32_t{0});
            W::Put(map, 0x48, std::int32_t{1});
            reference = {};
            children = {};
            Entity(buildings, 0, P::BuildingVtable, 1);
            W::Put(buildings.Row(0), 0x20, Actor());
            for (std::size_t index = 1; index != 3; ++index)
            {
                W::Put(ipl.ipl.Row(index), 0x22, std::int16_t{1});
                W::Put(ipl.ipl.Row(index), 0x24, std::int16_t{0});
                W::Put(ipl.ipl.Row(index), 0x26, std::int16_t{1});
                W::Put(ipl.ipl.Row(index), 0x28, std::int16_t{0});
            }
            W::Put(ipl.ipl.Row(1), 0x22, std::int16_t{0});
            W::Put(ipl.ipl.Row(1), 0x24, std::int16_t{0});
            W::Put(ipl.ipl.Row(1), 0x2d, static_cast<unsigned char>(1));
            ipl.Stream(1, 1);
        }
        void WithChild()
        {
            children[0] = Child();
            W::Put(Component(), 0xd0, reinterpret_cast<std::uintptr_t>(children.data()));
            W::Put(Component(), 0xd8, std::int32_t{1});
            W::Put(Component(), 0xdc, std::int32_t{1});
            W::Put(Child(), 0xc0, Component());
        }
        void WithRelatedDummy()
        {
            buildings.flags[0] = 0x81;
            Entity(objects, 0, P::ObjectVtable, 4);
            Entity(dummies, 0, P::DummyVtable, 5);
            W::Put(objects.Row(0), 0x20, Actor());
            W::Put(objects.Row(0), 0x200, dummies.Row(0));
            reference = {0, objects.Row(0) + 0x200};
            W::Put(dummies.Row(0), 0x40, reinterpret_cast<std::uintptr_t>(reference.data()));
            W::Put(ipl.ipl.Row(1), 0x22, std::int16_t{1});
            W::Put(ipl.ipl.Row(1), 0x24, std::int16_t{0});
            W::Put(ipl.ipl.Row(1), 0x26, std::int16_t{0});
            W::Put(ipl.ipl.Row(1), 0x28, std::int16_t{0});
            mapRows[0].count = 2;
        }
        void Acknowledge(std::uint32_t index, bool release = true)
        {
            W::Put(ipl.ipl.Row(index), W::P::DisabledOffset, std::uint16_t{1});
            W::Put(ipl.ipl.Row(index), 0x2d, static_cast<unsigned char>(0));
            ipl.Stream(index, 0);
            if (!release)
                return;
            for (auto* pool : {&buildings, &dummies, &objects})
                for (std::int32_t at = 0; at != pool->capacity; ++at)
                {
                    if (pool->flags[at] & 0x80)
                        continue;
                    unsigned char rowIpl = 0;
                    std::memcpy(&rowIpl, reinterpret_cast<void*>(pool->Row(at) + 0x56), 1);
                    if (rowIpl == index)
                        pool->flags[at] |= 0x80;
                }
        }
    };
}
