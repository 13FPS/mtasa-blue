/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse genuine native four-corner BoundRect without CPU trig replacement
 ****************************************************************************/
#pragma once
#include "NativeWorldBoundRectMemory.h"

namespace mta::game_de::NativeWorldBoundRectCalls
{
    using Rect = NativeWorldBoundRectMemory::Rect;
    using Kind = NativeWorldBoundRectMemory::Kind;
    using CheckOwner = NativeWorldBoundRectMemory::CheckOwner;
    using Entry = Rect* (*)(void*, Rect*);
    using Invoker = ClockResult (*)(Entry, std::uintptr_t entity, Rect&, Rect*& returned, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t entity = 0, metadata = 0;
        std::uint32_t  exception = 0;
        ClockResult    result = ClockResult::NotInitialized;
        bool           entered = false, returned = false, uncertain = false;
    };
    // These private primitives borrow an already admitted current native
    // callback, never a persistent lease. Only a full source/owner/post-seal
    // acknowledgement publishes output. This is not World::Add/Remove entry.
    ClockResult Read(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, Rect& output, CheckOwner, CheckOwner afterApplication,
                     Receipt* = nullptr, Invoker = nullptr);
}
