/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve genuine cover cleanup entry and acknowledgement boundaries
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>

namespace mta::game_de::NativeWorldCoverCalls
{
    enum class Operation : unsigned char
    {
        RemoveForEntity,
        ClearList
    };
    struct Receipt
    {
        bool          entered = false, returned = false;
        std::uint32_t exception = 0;
        ClockResult   result = ClockResult::NotInitialized;
    };
    using Invoker = ClockResult (*)(Operation, std::uintptr_t image, std::uintptr_t entity, Receipt&) noexcept;
    // The consuming controller must qualify every typed owner/list/reference
    // borrow, seal the last current owner observation and retain accepted intent
    // before entry. A source/entry thunk alone cannot grant manager ownership;
    // entered exceptions or lost post-return acknowledgements must not replay.
    ClockResult Invoke(Operation, std::uintptr_t image, std::uintptr_t entity, Receipt&, Invoker = nullptr) noexcept;
}
