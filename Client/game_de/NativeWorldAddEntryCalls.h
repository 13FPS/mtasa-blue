/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Whole native WorldAdd entry under genuine bounds and fixed-pool borrows
 ****************************************************************************/
#pragma once
#include "NativeWorldAddEntryMemory.h"
namespace mta::game_de::NativeWorldAddEntryCalls
{
    using Kind = NativeWorldAddEntryMemory::Kind;
    using CheckOwner = NativeWorldAddEntryMemory::CheckOwner;
    using Entry = void (*)(void*);
    using Invoker = ClockResult (*)(Entry, std::uintptr_t, std::uint32_t&) noexcept;
    struct Receipt
    {
        NativeWorldBoundRectCalls::Receipt rectangle;
        std::uintptr_t                     entity = 0;
        std::size_t                        nodes = 0;
        std::uint32_t                      exception = 0;
        ClockResult                        result = ClockResult::NotInitialized;
        bool                               entered = false, returned = false, acknowledged = false, uncertain = false;
    };
    // The consuming owner must journal accepted intent before calling this
    // mutating primitive. Only returned AND exact post-ACK can retire it;
    // entered exception/source/owner drift cannot be rolled back or replayed.
    ClockResult Add(std::uintptr_t, std::uint64_t, Kind, std::uint32_t, CheckOwner, CheckOwner, Receipt* = nullptr, Invoker = nullptr,
                    NativeWorldBoundRectCalls::Invoker = nullptr) noexcept;
}
