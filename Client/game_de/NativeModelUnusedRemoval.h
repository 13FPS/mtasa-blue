/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Remove only a freshly observed occupied zero-count model without decrement
 ****************************************************************************/
#pragma once
#include "NativeModelReferenceMemory.h"
#include "NativeStreamingRemoveCalls.h"

namespace mta::game_de::NativeModelUnusedRemoval
{
    struct Receipt
    {
        NativeModelReferenceOrigin          origin{};
        NativeStreamingRemoveCalls::Receipt removal{};
        std::uint32_t                       modelIndex = 0, count = 0;
        bool                                countObserved = false, verified = false;
    };
    // The caller supplies its positively owned custom-resource absence domain.
    // An absent/unloaded key is refusal, never an invented zero. Positive count
    // is a successful false observation; zero admits only guarded native Remove.
    ClockResult RemoveIfZero(std::uintptr_t image, std::uint32_t model, NativeModelReferenceMemory::CheckOwner, bool& removed, Receipt* = nullptr,
                             NativeStreamingRemoveCalls::Invoke = nullptr, const NativeModelReferenceOrigin* = nullptr) noexcept;
}
