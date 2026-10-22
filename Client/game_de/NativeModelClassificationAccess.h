/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA model predicate intents with the retained native callback owner
 ****************************************************************************/
#pragma once
#include "NativeModelClassificationCalls.h"
namespace mta::game_de
{
    class NativeModelClassificationAccess
    {
    public:
        virtual ~NativeModelClassificationAccess() = default;
        // An old/read-only owner cannot manufacture a negative vehicle class
        // result. Refusal preserves the caller's previous observation.
        virtual ClockResult ReadModelArchiveAllocated(std::uint32_t, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadModelVehiclePredicate(std::uint32_t, NativeModelVehiclePredicate, bool&) noexcept { return ClockResult::NotInitialized; }
    };
}
