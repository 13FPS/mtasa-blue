/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the actual fixed-pool World Add budget without admitting entry
 ****************************************************************************/
#pragma once
#include "NativeWorldAddPlan.h"
#include "NativeWorldIplMemory.h"
#include "NativeWorldPoolMatrixMemory.h"
#include <array>
#include <vector>

namespace mta::game_de::NativeWorldAddBudgetMemory
{
    struct Node
    {
        std::uintptr_t                                                       address = 0;
        std::array<unsigned char, NativeWorldCoverProfile::DoublePoolStride> bytes{};
    };
    struct Head
    {
        std::uintptr_t rva = 0, value = 0;
        Node           node;
    };
    struct Value
    {
        NativeStreamingCompletionContext::Value world;
        NativeModelReferenceHeap::Value         heap;
        NativeWorldPoolMatrixMemory::Entity     entity;
        // Reuse the existing qualified native32-byte pool/header/flags schema;
        // do not invent a second pool layout for the same native allocations.
        NativeWorldIplMemory::Pool pool;
        NativeWorldAddPlan::Value  plan;
        NativeWorldAddPlan::Rect   suppliedBounds{};
        std::vector<Head>          heads;
        std::vector<Node>          allocatedPrefix;
    };
    bool Profile(std::uintptr_t image) noexcept;
    // This closes fixed-pool exhaustion/cursor/node lifetime for the supplied
    // rectangle only. The upstream genuine GetBoundRect collision/transform
    // borrow is deliberately NOT admitted: success is not World::Add permission.
    // The actor-null typed subset skips UE writes, but no original is called.
    ClockResult CaptureForRect(std::uintptr_t image, NativeWorldAddPlan::Kind, std::uint32_t index, const NativeWorldAddPlan::Rect&, Value&);
    bool        Seal(std::uintptr_t image, const Value&) noexcept;
}
