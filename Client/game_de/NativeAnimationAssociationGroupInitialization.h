/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact reverse-row source itinerary and original-call prefix facts
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationGroupAccess.h"
#include "NativeStaticAssociationInitializeCalls.h"
#include <limits>
#include <vector>
namespace mta::game_de::NativeAnimationAssociationGroupInitialization
{
    struct Step
    {
        std::uint32_t                                   index = 0;
        std::uintptr_t                                  row = 0, beforeNodes = 0, hierarchy = 0;
        NativeStaticAssociationInitializeCalls::Receipt native;
        ClockResult                                     result = ClockResult::NotInitialized;
        bool                                            skipped = false, initialized = false;
    };
    struct Record
    {
        NativeAnimationAssociationGroupReferenceToken group;
        std::uintptr_t                                clump = 0;
        std::uint32_t                                 count = 0, visited = 0, skipped = 0, initialized = 0;
        std::uint32_t                                 failedIndex = (std::numeric_limits<std::uint32_t>::max)();
        ClockResult                                   result = ClockResult::NotInitialized;
        bool                                          completed = false, partialAccepted = false, enteredUnknown = false;
        // These are CPU acknowledgement facts only. Accepted nodes remain
        // owned/released by their original engine group/bank, never this log.
        std::vector<Step> steps;
    };
}
