/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual native-shaped occupied LOD source/target rows for read-only tests
 ****************************************************************************/
#pragma once
#include "NativeWorldPoolMatrixFixture.h"
#include "../../Client/game_de/NativeWorldLodMemory.h"
#include "../../Client/game_de/NativeWorldLodProfile.h"
namespace NativeWorldLodFixture
{
    namespace LodF = NativeWorldIplFixture;
    namespace LodEP = mta::game_de::NativeWorldIplLoadedProfile;
    struct State
    {
        NativeWorldPoolMatrixFixture::State entities;
        LodF::Pool                          objects{static_cast<std::int32_t>(LodEP::ObjectCapacity), LodEP::ObjectBytes};
        std::uintptr_t                      image;
        LodF::Writer                        writer;
        void*                               host;
        State(std::uintptr_t value, LodF::Writer apply, void* context) : entities(value, apply, context), image(value), writer(apply), host(context)
        {
            Reset();
        }
        bool Ready() const noexcept { return entities.Ready() && objects.Ready(); }
        void Reset()
        {
            if (!Ready())
                return;
            entities.Reset();
            objects.Reset();
            for (const auto& pin : LodEP::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : mta::game_de::NativeWorldLodProfile::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& slot : LodEP::Slots)
            {
                const auto target = image + slot.target;
                writer(host, slot.rva, &target, sizeof(target), PAGE_READONLY);
            }
            const auto objectPool = objects.Address();
            writer(host, LodEP::ObjectRoot, &objectPool, sizeof(objectPool), PAGE_READWRITE);
            entities.buildings.flags[1] = 2;
            entities.dummies.flags[1] = 2;
            objects.flags[0] = 1;
            LodF::Put(entities.buildings.Row(1), 0, image + LodEP::BuildingVtable);
            LodF::Put(entities.buildings.Row(1), 0x6a, static_cast<unsigned char>(1));
            LodF::Put(entities.dummies.Row(1), 0, image + LodEP::DummyVtable);
            LodF::Put(entities.dummies.Row(1), 0x6a, static_cast<unsigned char>(5));
            LodF::Put(objects.Row(0), 0, image + LodEP::ObjectVtable);
            LodF::Put(objects.Row(0), 0x6a, static_cast<unsigned char>(4));
            // B0->B1; D0->D1->B0; O0->B1. This exercises same-pool,
            // cross-pool and null pointers without inventing a LOD cycle.
            LodF::Put(entities.buildings.Row(0), 0x60, entities.buildings.Row(1));
            LodF::Put(entities.dummies.Row(0), 0x60, entities.dummies.Row(1));
            LodF::Put(entities.dummies.Row(1), 0x60, entities.buildings.Row(0));
            LodF::Put(objects.Row(0), 0x60, entities.buildings.Row(1));
            LodF::Put(entities.buildings.Row(0), 0x68, static_cast<unsigned char>(1));
            LodF::Put(entities.buildings.Row(1), 0x68, static_cast<unsigned char>(2));
            LodF::Put(entities.dummies.Row(1), 0x68, static_cast<unsigned char>(1));
        }
    };
}
