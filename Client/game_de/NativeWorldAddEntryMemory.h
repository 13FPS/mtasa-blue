/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bind genuine BoundRect observations to exact native WorldAdd effects
 ****************************************************************************/
#pragma once
#include "NativeWorldBoundRectCalls.h"
#include "NativeWorldAddBudgetMemory.h"
namespace mta::game_de::NativeWorldAddEntryMemory
{
    using Kind = NativeWorldBoundRectMemory::Kind;
    using CheckOwner = NativeWorldBoundRectMemory::CheckOwner;
    struct Value
    {
        NativeWorldBoundRectMemory::Value  bounds;
        NativeWorldAddBudgetMemory::Value  budget;
        NativeWorldBoundRectCalls::Receipt rectRead;
        std::vector<unsigned char>         bank, sectors, lod;
        std::uint32_t                      fpControls = 0;
    };
    // Values borrow the same current native callback. They are never persistent
    // leases or a claim that this entity is absent from every world list.
    ClockResult Capture(std::uintptr_t, std::uint64_t, Kind, std::uint32_t, CheckOwner, CheckOwner, Value&, NativeWorldBoundRectCalls::Invoker = nullptr,
                        NativeWorldBoundRectCalls::Receipt* rectangleReceipt = nullptr);
    bool        Seal(std::uintptr_t, const Value&) noexcept;
    // A pure byte-effect expectation, reusing the already qualified Budget
    // plan. It does not replace geometry, allocate nodes or publish pool roots.
    bool Expected(const Value&, Value&);
}
