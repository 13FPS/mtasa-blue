/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve full-width typed LOD relationships when planning a pool move
 ****************************************************************************/
#pragma once
#include "NativeWorldLodMemory.h"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace mta::game_de::NativeWorldLodRebasePlan
{
    struct Edit
    {
        // Preserve the old cell's preimage separately from the prospective
        // destination cell when the owner itself is a moved Building.
        std::uintptr_t sourceSlot = 0, slot = 0, before = 0, after = 0;
        unsigned       ownerPool = 0;
        std::uint32_t  ownerIndex = 0, targetIndex = 0;
    };
    // A destination is hypothetical CPU input, never allocation/write authority.
    // Consume the actual typed snapshot; unresolved placement indices cannot be
    // rebased as pointers. Failure leaves the caller's prior output unchanged.
    ClockResult Plan(const NativeWorldLodMemory::Snapshot&, std::uintptr_t newBase, std::size_t newCapacity, std::vector<Edit>&) noexcept;
}
