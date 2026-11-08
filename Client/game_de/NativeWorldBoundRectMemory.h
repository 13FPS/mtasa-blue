/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One current native BoundRect input borrow shared by World Add/Remove
 ****************************************************************************/
#pragma once
#include "NativeWorldPoolMatrixMemory.h"
#include "NativeModelObjectGroupMemory.h"
#include "NativeModelObjectGroupProfile.h"
#include "NativeWorldIplPlan.h"
#include <array>

namespace mta::game_de::NativeWorldBoundRectMemory
{
    using Kind = NativeWorldPoolMatrixMemory::Kind;
    using Rect = NativeWorldIplPlan::Rect;
    using CheckOwner = NativeModelBankReader::CheckOwner;
    struct Value
    {
        NativeStreamingCompletionContext::Value                              world;
        NativeModelReferenceHeap::Value                                      heap;
        NativeWorldPoolMatrixMemory::Entity                                  entity;
        NativeModelObjectGroupMemory::Value                                  model;
        NativeWorldPoolMatrixMemory::Value                                   matrices;
        NativeWorldPoolMatrixMemory::NodeView                                matrix;
        std::array<unsigned char, NativeModelObjectGroupProfile::PoolStride> modelBytes{};
        std::array<unsigned char, 24>                                        defaultBox{};
        bool                                                                 hasMatrix = false;
    };
    bool Profile(std::uintptr_t image) noexcept;
    // Initial genuine native subset: actor-null typed Building/Dummy, actual
    // allocated normalAtomic metadata with null collision30, and the genuine
    // static fallback box. Admit a current owned matrix, or exact inline +0
    // heading bits only. General heap collision and CRT math routes refuse.
    // All owner callbacks finish before the final callback-free raw seal.
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, CheckOwner, CheckOwner afterApplication, Value&);
    bool        Seal(std::uintptr_t image, const Value&) noexcept;
}
