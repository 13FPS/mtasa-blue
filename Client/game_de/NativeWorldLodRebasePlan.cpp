/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Plan typed LOD edits without truncated deltas or pointer writes
 ****************************************************************************/
#include "NativeWorldLodRebasePlan.h"
#include "NativeWorldIplLoadedProfile.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <unordered_map>
namespace mta::game_de::NativeWorldLodRebasePlan
{
    ClockResult Plan(const NativeWorldLodMemory::Snapshot& snapshot, std::uintptr_t newBase, std::size_t newCapacity, std::vector<Edit>& output) noexcept
    {
        namespace M = NativeWorldLodMemory;
        namespace P = NativeWorldIplLoadedProfile;
        if (snapshot.representation != M::Representation::ResolvedPointers)
            return ClockResult::UnsupportedBuild;
        constexpr auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
        if (!newBase || newBase % alignof(std::uintptr_t) || !newCapacity || newCapacity > (maximum - newBase) / P::BuildingBytes)
            return ClockResult::InvalidArgument;
        try
        {
            const std::array<std::size_t, 3> capacities{P::BuildingCapacity, P::DummyCapacity, P::ObjectCapacity};
            const std::array<std::size_t, 3> strides{P::BuildingBytes, P::DummyBytes, P::ObjectBytes};
            std::size_t                      occupied = 0;
            for (std::size_t index = 0; index != snapshot.pools.size(); ++index)
            {
                const auto& pool = snapshot.pools[index];
                if (!pool.objects || pool.objects % alignof(std::uintptr_t) || pool.capacity != static_cast<std::int32_t>(capacities[index]) ||
                    pool.stride != strides[index] || pool.generations.size() != capacities[index] || capacities[index] > (maximum - pool.objects) / pool.stride)
                    return ClockResult::BindingChanged;
                for (std::size_t prior = 0; prior != index; ++prior)
                {
                    const auto& other = snapshot.pools[prior];
                    if (!(pool.objects + capacities[index] * pool.stride <= other.objects || other.objects + capacities[prior] * other.stride <= pool.objects))
                        return ClockResult::BindingChanged;
                }
                occupied += static_cast<std::size_t>(
                    std::count_if(pool.generations.begin(), pool.generations.end(), [](unsigned char generation) { return !(generation & 0x80); }));
            }
            if (occupied != snapshot.rows.size())
                return ClockResult::BindingChanged;
            std::unordered_map<std::uintptr_t, const M::Row*> rows;
            for (const auto& row : snapshot.rows)
            {
                if (row.pool >= snapshot.pools.size())
                    return ClockResult::BindingChanged;
                const auto&    pool = snapshot.pools[row.pool];
                std::uintptr_t lod = 0;
                if (row.index >= capacities[row.pool] || row.bytes.size() != pool.stride)
                    return ClockResult::BindingChanged;
                std::memcpy(&lod, row.bytes.data() + 0x60, sizeof(lod));
                if (row.address != pool.objects + static_cast<std::size_t>(row.index) * pool.stride || (row.generation & 0x80) ||
                    row.generation != pool.generations[row.index] || row.lod != lod || row.lodChildren != row.bytes[0x68] ||
                    !rows.emplace(row.address, &row).second)
                    return ClockResult::BindingChanged;
            }
            std::vector<Edit> next;
            for (const auto& row : snapshot.rows)
            {
                if (!row.lod)
                    continue;
                const auto target = rows.find(row.lod);
                // A plausible offset into any pool is insufficient: the exact
                // destination row must have been typed and occupied in the same
                // observation. Cross-pool relationships remain unchanged.
                if (target == rows.end())
                    return ClockResult::BindingChanged;
                if (target->second->pool != 0)
                    continue;
                const auto index = target->second->index;
                if (index >= newCapacity)
                    return ClockResult::InvalidArgument;
                const auto after = newBase + static_cast<std::uintptr_t>(index) * P::BuildingBytes;
                if (after != row.lod)
                {
                    if (row.pool == 0 && row.index >= newCapacity)
                        return ClockResult::InvalidArgument;
                    // Building owners move with their pool. Preserve their
                    // old preimage cell while naming the future destination
                    // cell; an old-row write after movement would target freed
                    // storage. Other typed owner pools keep their current cell.
                    const auto sourceSlot = row.address + 0x60;
                    const auto slot = row.pool == 0 ? newBase + static_cast<std::uintptr_t>(row.index) * P::BuildingBytes + 0x60 : sourceSlot;
                    next.push_back({sourceSlot, slot, row.lod, after, row.pool, row.index, index});
                }
            }
            output.swap(next);
            return ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
}
