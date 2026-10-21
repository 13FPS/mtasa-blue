/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain the owned near leaf and context until caller-proved retirement
 *****************************************************************************/
#pragma once
#include "NativeWeatherRainBridge.h"

namespace mta::game_de
{
    // This allocates only private helper memory. It cannot qualify an image,
    // publish the native entry or authorize a callback/native serializer call.
    class NativeWeatherRainLeaf final
    {
    public:
        enum class Failure : unsigned
        {
            None,
            InvalidImage,
            AlreadyAllocated,
            Allocation,
            Build,
            Copy,
            Protection,
            Flush,
            Identity,
            Readback,
            Quiescence,
            ActiveCalls,
            Release
        };

        struct State final
        {
            std::uintptr_t                    allocationBase = 0;
            std::size_t                       allocationBytes = 0, pageBytes = 0;
            std::uintptr_t                    leafEntry = 0;
            NativeWeatherRainBridge::Context* context = nullptr;
            NativeWeatherRainBridge::Plan     plan{};
            bool                              prepared = false;
            Failure                           failure = Failure::None;
        };

        // The owner must hold its established peer/entry/stack quiescence for
        // the entire Release call. A zero activity count is only an additional
        // refusal check and never substitutes for this explicit owner proof.
        using ReleaseCheck = bool (*)(void*, const State&) noexcept;

        NativeWeatherRainLeaf() noexcept = default;
        ~NativeWeatherRainLeaf() = default;
        NativeWeatherRainLeaf(const NativeWeatherRainLeaf&) = delete;
        NativeWeatherRainLeaf& operator=(const NativeWeatherRainLeaf&) = delete;
        NativeWeatherRainLeaf(NativeWeatherRainLeaf&&) = delete;
        NativeWeatherRainLeaf& operator=(NativeWeatherRainLeaf&&) = delete;

        bool Prepare(std::uintptr_t image) noexcept;
        bool Verify() noexcept;
        // No allocation preserves the caller's output. Partial preparation
        // still publishes the exact retained identities and cleanup debt.
        bool Read(State&) const noexcept;
        bool Release(ReleaseCheck, void* opaque) noexcept;

    private:
        bool           AllocationIdentity() const noexcept;
        bool           ContextIdentity() const noexcept;
        State          state;
        std::uintptr_t image = 0;
        bool           complete = false;
    };
}
