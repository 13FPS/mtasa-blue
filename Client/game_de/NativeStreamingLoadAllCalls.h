/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine blocking-all loading within the retained native callback owner
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de::NativeStreamingLoadAllCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using NativeFunction = void (*)();
    using Invoke = ClockResult (*)(NativeFunction, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::uint32_t  exceptionCode = 0;
        unsigned char  modeBefore = 0, modeAfter = 0;
        bool           entered = false, returned = false;
    };
    // The global native entry has no priority argument; true refuses here.
    // The SDK routes selective work to NativeStreamingPriorityCalls instead.
    // Both genuine paths share one reentry gate, preserving outer ownership.
    // Receipt facts borrow no object/GC lease.
    ClockResult Load(std::uintptr_t image, bool onlyPriority, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
