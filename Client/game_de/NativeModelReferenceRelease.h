/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit final model-count release with optional native removal
 ****************************************************************************/
#pragma once
#include "NativeModelReferenceMemory.h"
#include "NativeStreamingRemoveCalls.h"
namespace mta::game_de::NativeModelReferenceRelease
{
    enum class ZeroPolicy : std::uint32_t
    {
        KeepResident,
        RemoveAtZero
    };
    struct Receipt
    {
        NativeModelReferenceMemory::Receipt reference{};
        NativeStreamingRemoveCalls::Receipt removal{};
        bool                                removalPrepared = false;
    };
    // Only an occupied selected-key DWORD count of one is admitted. Reaching
    // zero preserves that keyed row and does not invent a sparse free. The
    // caller must supply its actual custom-clump/custom-col eligibility as
    // policy; neither native fields nor this primitive establish SDK policy.
    // CJ/model0 exclusion, pending cancellation and interface references stay
    // with the genuine shared MTA wrapper policy. An entered failure retires
    // the owner without replay or rolling back an accepted zero/removal.
    ClockResult ReleaseFinal(std::uintptr_t image, std::uint32_t modelIndex, ZeroPolicy, NativeModelReferenceMemory::CheckOwner, Receipt* = nullptr,
                             NativeModelReferenceMemory::Exchange = nullptr, NativeStreamingRemoveCalls::Invoke = nullptr,
                             const NativeModelReferenceMemory::RequiredIdentity* = nullptr) noexcept;
}
