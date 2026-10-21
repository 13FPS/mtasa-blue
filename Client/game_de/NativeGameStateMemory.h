/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared native system-state binding and checked scalar access
 *****************************************************************************/
#pragma once

#include <enums/SystemState.h>
#include <cstdint>

namespace mta::game_de::NativeGameStateMemory
{
    // These helpers qualify storage and the four established state meanings.
    // A caller must separately own the engine callback before requesting a
    // write; neither image qualification nor a matching read grants that lease.
    // The owner also checks executable identity before binding this live image.
    bool Qualify(std::uintptr_t image) noexcept;
    bool Decode(std::uint32_t native, SystemState::Enum& output) noexcept;
    bool Encode(SystemState::Enum state, std::uint32_t& output) noexcept;
    bool Read(std::uintptr_t image, std::uint32_t& output) noexcept;

    enum class StoreResult
    {
        Unsupported,
        ValueChanged,
        Applied
    };

    // Compare at the actual DWORD rather than overwriting a native transition
    // which happened after Read. Applied means that one scalar was replaced;
    // it does not assert the engine remains in that state or has finished startup.
    // A successful request is persistent intent, not a temporary override:
    // cleanup must not restore an earlier state over native forward progress.
    StoreResult CompareExchange(std::uintptr_t image, std::uint32_t expected, std::uint32_t target) noexcept;
}
