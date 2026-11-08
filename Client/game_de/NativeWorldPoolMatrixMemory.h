/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current typed pool receiver and native static-matrix observations
 ****************************************************************************/
#pragma once
#include "NativeStreamingCompletionContext.h"
#include "NativeModelReferenceHeap.h"
#include "NativeWorldPoolMatrixProfile.h"
#include <array>
#include <vector>
namespace mta::game_de::NativeWorldPoolMatrixMemory
{
    enum class Kind : unsigned char
    {
        Building,
        Dummy
    };
    enum class List : unsigned char
    {
        Dynamic,
        Static,
        Free,
        Unbound
    };
    using Transform = std::array<unsigned char, NativeWorldPoolMatrixProfile::TransformBytes>;
    struct Entity
    {
        Kind                            kind = Kind::Building;
        std::uint32_t                   index = 0;
        unsigned char                   generation = 0;
        std::uintptr_t                  root = 0, pool = 0, objects = 0, flags = 0, address = 0, matrix = 0, vtable = 0;
        std::int32_t                    capacity = 0;
        std::size_t                     stride = 0;
        std::array<unsigned char, 32>   header{};
        std::array<unsigned char, 0x78> bytes{};
    };
    struct NodeView
    {
        std::uintptr_t address = 0, owner = 0, previous = 0, next = 0, attached = 0;
        // Raw initialized bytes48..4f have no proved flag-width consumer in
        // these admitted APIs. Preserve/seal them without a zero predicate.
        std::uint64_t                                                      opaque48 = 0;
        List                                                               list = List::Unbound;
        std::array<unsigned char, NativeWorldPoolMatrixProfile::NodeBytes> bytes{};
    };
    struct Value
    {
        NativeStreamingCompletionContext::Value                                world;
        NativeModelReferenceHeap::Value                                        heap;
        std::uintptr_t                                                         bank = 0;
        std::uint64_t                                                          cookie = 0;
        std::array<unsigned char, 6 * NativeWorldPoolMatrixProfile::NodeBytes> sentinels{};
        std::vector<NodeView>                                                  nodes;
        std::array<std::vector<std::size_t>, 3>                                lists;
    };
    // These private primitives borrow the caller's already-owned current
    // GameThread/no-GC callback. Observations are values, not lifetime leases.
    // A matrix owner pointer alone is never an admitted native entity.
    bool        Profile(std::uintptr_t image) noexcept;
    ClockResult CaptureEntity(std::uintptr_t image, Kind, std::uint32_t index, Entity&);
    bool        SealEntity(std::uintptr_t image, const Entity&) noexcept;
    ClockResult Capture(std::uintptr_t image, Value&);
    bool        Seal(std::uintptr_t image, const Value&) noexcept;
    bool        SameBank(const Value&, const Value&) noexcept;
    ClockResult FindOwner(const Value&, const Entity&, NodeView&) noexcept;
    ClockResult RequireFree(const Value&, std::size_t count) noexcept;
    bool        ValidTransform(const Transform&) noexcept;
    // Caller records an accepted store before entering this final raw fence.
    // It copies only CMatrix's first64 transform bytes; attached RW metadata,
    // owner and intrusive list links belong to the new native allocation.
    ClockResult StoreTransform(std::uintptr_t image, const Value&, const Entity&, const Transform&) noexcept;
}
