/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Seal current typed sector/LOD resources before genuine World Remove
 ****************************************************************************/
#pragma once
#include "NativeWorldBoundRectMemory.h"
#include "NativeWorldAddPlan.h"
#include "NativeWorldIplMemory.h"
#include <array>
#include <vector>
namespace mta::game_de::NativeWorldRemoveMemory
{
    using Kind = NativeWorldBoundRectMemory::Kind;
    using CheckOwner = NativeWorldBoundRectMemory::CheckOwner;
    struct Head
    {
        std::uintptr_t rva = 0, before = 0, after = 0;
    };
    struct Node
    {
        std::uintptr_t                                                address = 0;
        std::array<unsigned char, NativeWorldAddProfile::DoubleBytes> before{}, after{};
    };
    struct Value
    {
        NativeWorldBoundRectMemory::Value bound;
        NativeWorldIplMemory::Pool        pool;
        NativeWorldAddPlan::PoolKind      poolKind = NativeWorldAddPlan::PoolKind::Single;
        std::vector<Head>                 heads;
        std::vector<Node>                 nodes;
        std::vector<unsigned char>        flagsAfter;
        std::int32_t                      cursorAfter = -1;
        std::size_t                       removedCount = 0;
        // Only these plain identities may be copied into retained intent.
        // bound's current world/model/matrix observations are callback-local.
        std::vector<std::uintptr_t> identity;
    };
    bool        Profile(std::uintptr_t image) noexcept;
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, CheckOwner, CheckOwner afterApplication, Value&);
    // Both checks are callback-free. Removed compares the native expected
    // deltas and unchanged typed/world/heap inputs; it never invokes an entry.
    bool Seal(std::uintptr_t image, const Value&) noexcept;
    bool Removed(std::uintptr_t image, const Value&) noexcept;
}
