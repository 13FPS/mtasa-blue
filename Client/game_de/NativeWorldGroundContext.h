/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe current native ground-query roots without acquiring entities
 ****************************************************************************/
#pragma once
#include "NativeStreamingCompletionContext.h"
#include <array>
namespace mta::game_de::NativeWorldGroundContext
{
    struct Observation
    {
        std::uintptr_t address = 0, value = 0;
    };
    struct Value
    {
        NativeStreamingCompletionContext::Value graph;
        std::array<Observation, 32>             roots{};
        std::size_t                             count = 0;
        std::uint16_t                           scan = 0;
    };
    bool        Profile(std::uintptr_t image) noexcept;
    ClockResult Capture(std::uintptr_t image, Value&) noexcept;
    bool        SameBefore(const Value&, const Value&) noexcept;
    // After native execution list topology/scan state may legitimately differ.
    // Only independently recaptured readiness and the same world are required.
    bool SameWorld(const Value&, const Value&) noexcept;
}
