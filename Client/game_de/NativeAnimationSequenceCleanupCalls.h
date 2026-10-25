/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Full-capacity metadata qualification for one actual cleanup owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceDependencyCalls.h"
namespace mta::game_de::NativeAnimationSequenceCleanupCalls
{
    // This shares the issued storage friend, exact current heap and one gate.
    // It reads row metadata only; an original frame record never grants any
    // payload format, native hierarchy retirement or publication permission.
    ClockResult WithArray(std::uintptr_t, NativeCustomAnimSequenceStorage&, std::uint64_t, NativeAnimationSequenceDependencyCalls::CheckOwner,
                          NativeAnimationSequenceDependencyCalls::ResolveFrame, void*, NativeAnimationSequenceDependencyCalls::Action, void*) noexcept;
}
