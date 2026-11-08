/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Constructed CPU arithmetic cases, separate from actual memory reads
 ****************************************************************************/
#include "../../Client/game_de/NativeWorldLodRebasePlan.h"
#include "../../Client/game_de/NativeWorldIplLoadedProfile.h"
#include <array>
#include <cstring>
#include <limits>
#include <utility>
namespace NativeWorldLodRebasePlanFixture
{
    inline mta::game_de::NativeWorldLodMemory::Snapshot Constructed()
    {
        namespace M = mta::game_de::NativeWorldLodMemory;
        namespace P = mta::game_de::NativeWorldIplLoadedProfile;
        M::Snapshot                      value;
        const std::array<std::size_t, 3> capacities{P::BuildingCapacity, P::DummyCapacity, P::ObjectCapacity};
        const std::array<std::size_t, 3> strides{P::BuildingBytes, P::DummyBytes, P::ObjectBytes};
        for (unsigned pool = 0; pool != 3; ++pool)
        {
            auto& target = value.pools[pool];
            target.objects = 0x110000000ull + static_cast<std::uintptr_t>(pool) * 0x100000000ull;
            target.capacity = static_cast<std::int32_t>(capacities[pool]);
            target.stride = strides[pool];
            target.generations.assign(capacities[pool], 0x80);
        }
        const auto add = [&](unsigned pool, std::uint32_t index, std::uintptr_t lod)
        {
            M::Row row;
            row.pool = pool;
            row.index = index;
            row.generation = 7;
            row.lodChildren = 3;
            row.address = value.pools[pool].objects + index * value.pools[pool].stride;
            row.lod = lod;
            row.bytes.resize(value.pools[pool].stride);
            std::memcpy(row.bytes.data() + 0x60, &lod, sizeof(lod));
            row.bytes[0x68] = row.lodChildren;
            value.pools[pool].generations[index] = row.generation;
            value.rows.push_back(std::move(row));
        };
        add(0, 2, value.pools[0].objects + 7 * P::BuildingBytes);
        add(0, 7, 0);
        add(1, 3, value.pools[0].objects + 7 * P::BuildingBytes);
        add(2, 4, value.pools[1].objects + 3 * P::DummyBytes);
        add(2, 5, value.pools[2].objects + 4 * P::ObjectBytes);
        return value;
    }

    template <class Check>
    void Run(Check&& check)
    {
        using namespace mta::game_de;
        namespace P = NativeWorldLodRebasePlan;
        namespace L = NativeWorldIplLoadedProfile;
        auto                 source = Constructed();
        const auto           destination = std::uintptr_t{0x510000000ull};
        std::vector<P::Edit> edits;
        check(P::Plan(source, destination, L::BuildingCapacity, edits) == ClockResult::Ok && edits.size() == 2,
              "constructed CPU snapshot plans only two pointers into moved Buildings");
        check(edits[0].sourceSlot == source.rows[0].address + 0x60 && edits[0].slot == destination + 2 * L::BuildingBytes + 0x60 &&
                  edits[0].before == source.rows[0].lod && edits[0].after == destination + 7 * L::BuildingBytes,
              "Building owner preimage stays distinct from its prospective moved cell above4GiB");
        check(edits[1].slot == source.rows[2].address + 0x60 && edits[1].sourceSlot == edits[1].slot && edits[1].ownerPool == 1,
              "Dummy owner cell remains current while its Building target moves");
        check(P::Plan(source, 0x10000, L::BuildingCapacity, edits) == ClockResult::Ok && edits[0].after == 0x10000 + 7 * L::BuildingBytes,
              "a downward move uses base plus typed index without unsigned delta wrap");
        check(P::Plan(source, source.pools[0].objects, L::BuildingCapacity, edits) == ClockResult::Ok && edits.empty(),
              "unchanged base needs no pointer stores");
        const auto refuse = [&](auto snapshot, std::uintptr_t base, std::size_t capacity, const char* why)
        {
            std::vector<P::Edit> prior(1);
            prior[0].sourceSlot = 0x1234;
            prior[0].slot = 0x5678;
            prior[0].after = 0x9abc;
            check(P::Plan(snapshot, base, capacity, prior) != ClockResult::Ok && prior.size() == 1 && prior[0].sourceSlot == 0x1234 &&
                      prior[0].slot == 0x5678 && prior[0].after == 0x9abc,
                  why);
        };
        refuse(source, destination, 7, "truncated target refuses whole CPU plan without changing output");
        refuse(source, (std::numeric_limits<std::uintptr_t>::max)() - 7, 2, "destination extent overflow refuses without wrap");
        refuse(source, destination + 1, L::BuildingCapacity, "unaligned hypothetical base is refused");
        auto invalid = source;
        invalid.representation = NativeWorldLodMemory::Representation::PlacementIndices;
        refuse(invalid, destination, L::BuildingCapacity, "unresolved placement indices cannot become pointers");
        invalid = source;
        invalid.rows.pop_back();
        refuse(invalid, destination, L::BuildingCapacity, "omitted occupied row cannot claim an exhaustive typed snapshot");
        invalid = source;
        invalid.rows[0].lod += 1;
        std::memcpy(invalid.rows[0].bytes.data() + 0x60, &invalid.rows[0].lod, sizeof(std::uintptr_t));
        refuse(invalid, destination, L::BuildingCapacity, "interior pointer cannot truncate to an occupied row index");
        invalid = source;
        invalid.pools[0].generations[7] |= 0x80;
        refuse(invalid, destination, L::BuildingCapacity, "freed target generation cannot be rebased");
        invalid = source;
        invalid.rows.push_back(invalid.rows.front());
        refuse(invalid, destination, L::BuildingCapacity, "duplicate typed row identity is refused");
        invalid = source;
        invalid.rows[0].bytes[0x60] ^= 1;
        refuse(invalid, destination, L::BuildingCapacity, "raw64 preimage must match captured LOD pointer");
        invalid = source;
        for (const auto row : {std::size_t{0}, std::size_t{2}})
        {
            invalid.rows[row].lod = 0;
            std::memcpy(invalid.rows[row].bytes.data() + 0x60, &invalid.rows[row].lod, sizeof(std::uintptr_t));
        }
        invalid.rows[1].lod = invalid.rows[0].address;
        std::memcpy(invalid.rows[1].bytes.data() + 0x60, &invalid.rows[1].lod, sizeof(std::uintptr_t));
        refuse(invalid, destination, 3, "truncated moved Building owner cannot emit a store into freed old storage");
    }
}
