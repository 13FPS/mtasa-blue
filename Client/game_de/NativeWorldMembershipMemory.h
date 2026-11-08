/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read complete world-head membership without admitting a mutation
 ****************************************************************************/
#pragma once
#include "NativeWorldAddBudgetMemory.h"
#include "NativeWorldMembershipProfile.h"
#include <array>
#include <vector>
namespace mta::game_de::NativeWorldMembershipMemory
{
    using Kind = NativeWorldPoolMatrixMemory::Kind;
    using CheckOwner = NativeStreamingCompletionContext::CheckOwner;
    enum class Group : unsigned char
    {
        NormalBuilding,
        NormalDummy,
        RepeatVehicle,
        RepeatPed,
        RepeatObject,
        Lod,
        Moving
    };
    struct Pool
    {
        NativeWorldIplMemory::Pool native;
        std::vector<unsigned char> bytes;
        std::size_t                reachable = 0;
    };
    struct Occurrence
    {
        std::uintptr_t headRva = 0, node = 0;
        Group          group = Group::NormalBuilding;
        unsigned char  generation = 0;
    };
    struct Value
    {
        NativeStreamingCompletionContext::Value world;
        NativeModelReferenceHeap::Value         heap;
        NativeWorldPoolMatrixMemory::Entity     entity;
        // Supplied by the same current owner whose callbacks fence the borrow;
        // this plain identity is not an independently issued generation lease.
        std::uint64_t generation = 0;
        // The same fixed native allocations also serve IPL/Cover. Unreachable
        // occupied nodes remain opaque; this is no external-reference census.
        std::array<Pool, 2>         pools;
        std::vector<std::uintptr_t> normal, repeat, lod;
        std::uintptr_t              moving = 0;
        std::array<std::size_t, 7>  counts{};
        std::vector<Occurrence>     occurrences;
    };
    bool Profile(std::uintptr_t image) noexcept;
    // Only an Ok result with no occurrences proves absence from these complete
    // world head graphs in this owner callback. It grants no write/free/release,
    // pool replacement, GC lease, or whole-world backup readiness.
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, CheckOwner, CheckOwner afterApplication, Value&);
    // Callback-local equality fence after the last owner check; no native entry
    // or caller callback runs here, and no observation may outlive its owner.
    bool Seal(std::uintptr_t image, const Value&) noexcept;
}
