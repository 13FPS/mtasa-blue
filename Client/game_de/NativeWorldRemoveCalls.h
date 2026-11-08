/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve real World Remove entry/return edges for its owning journal
 ****************************************************************************/
#pragma once
#include "NativeWorldRemoveMemory.h"
namespace mta::game_de::NativeWorldRemoveCalls
{
    using Entry = void (*)(void*);
    using Invoker = ClockResult (*)(Entry, void*, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        bool          entered = false, returned = false;
        std::uint32_t exception = 0;
        ClockResult   result = ClockResult::NotInitialized;
    };
    // The Runtime must publish retained intent BEFORE this private primitive.
    // A returned original still needs owner and Removed acknowledgement; any
    // entered uncertainty remains debt and may never replay automatically.
    ClockResult Invoke(std::uintptr_t image, const NativeWorldRemoveMemory::Value&, Receipt&, Invoker = nullptr) noexcept;
}
