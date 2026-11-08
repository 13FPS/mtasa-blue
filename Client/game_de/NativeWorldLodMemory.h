/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current resolved64-bit entity LOD observations without resize effects
 ****************************************************************************/
#pragma once
#include "NativeStreamingCompletionContext.h"
#include "NativeModelReferenceHeap.h"
#include <array>
#include <vector>
namespace mta::game_de::NativeWorldLodMemory
{
    using CheckOwner = NativeStreamingCompletionContext::CheckOwner;
    // Exact source families: Building0, Dummy1, Object2. These are not the
    // native entity type bit values (1,5,4 respectively).
    enum class Representation : unsigned char
    {
        ResolvedPointers,
        PlacementIndices
    };
    struct Pool
    {
        std::uintptr_t                root = 0, address = 0, objects = 0, flags = 0;
        std::int32_t                  capacity = 0;
        std::size_t                   stride = 0;
        std::array<unsigned char, 32> header{};
        std::vector<unsigned char>    generations;
    };
    struct Row
    {
        unsigned                   pool = 0;
        std::uint32_t              index = 0;
        std::uintptr_t             address = 0, lod = 0;
        unsigned char              generation = 0, lodChildren = 0;
        std::vector<unsigned char> bytes;
    };
    struct Snapshot
    {
        Representation                  representation = Representation::ResolvedPointers;
        std::array<Pool, 3>             pools;
        std::vector<Row>                rows;
        std::vector<std::uintptr_t>     identity;
        NativeModelReferenceHeap::Value heap;
    };
    // These values are CPU observations, not engine leases. No UObject token,
    // pointer mutation, pool allocation or header publication is exported.
    ClockResult           Capture(std::uintptr_t image, CheckOwner, CheckOwner afterApplication, Snapshot&) noexcept;
    ClockResult           Recheck(std::uintptr_t image, CheckOwner, CheckOwner afterApplication, const Snapshot&) noexcept;
    bool                  Same(const Snapshot&, const Snapshot&) noexcept;
    inline constexpr bool CompleteIplEntityArrayEnumeration = false;
}
