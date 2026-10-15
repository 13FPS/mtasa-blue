/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded observation of an explicitly selected native DE viewport callback
 *****************************************************************************/

#pragma once

#include <cstdint>
#include "../sdk/game/CGameRenderObservation.h"
#include "../sdk/game/CGameRenderEndpointDiagnostic.h"
#include "../sdk/game/CGameRenderSetupDiagnostic.h"

namespace mta::game_de
{
    enum class NativeRenderObservationFailure : std::uint32_t
    {
        None = 0,
        UnsupportedBuild = 1,
        FingerprintMismatch = 2,
        HookInstall = 3,
        HookRemove = 4,
        WrongProducerThread = 5,
        NestedCallback = 6,
        InvalidViewport = 7,
        TimeBudget = 8,
        UnsupportedRoute = 9,
        GpuOwnershipCleanup = 10
    };

    struct NativeRenderObservationState
    {
        bool                             bound = false, active = false, modulePinned = false, pendingCleanup = false;
        std::uint64_t                    generation = 0, nativePresentCallbacks = 0, acceptedSnapshots = 0, rejectedSnapshots = 0;
        std::uint64_t                    nestedCallbacks = 0, foreignThreadCallbacks = 0, publicationMisses = 0;
        std::uint32_t                    callbackThreadId = 0, width = 0, height = 0, syncInterval = 0, failureReason = 0;
        bool                             windowPresent = false, swapchainPresent = false, devicePresent = false, contextPresent = false;
        bool                             lastOriginalReturned = false, lastOriginalResult = false;
        std::uint32_t                    requestedRoute = 0, observedRoute = 0, presenceKnownMask = 0, queuePresent = 0;
        GpuOwnershipProbeState           gpu;
        CustomPresentClassificationState custom;
        FrameBoundaryObservationState    boundary;
        FrameBoundaryObservationState    nativeDispatch;
    };

    // This is an observation of a naturally invoked native viewport callback,
    // not proof that its optional COM Present branch ran or pixels were drawn.
    // Bound records the successful build/function admission and stays true
    // after End; active/pendingCleanup describe observation and hook cleanup.
    // API1/2 make no COM calls. Explicit API3/4 queries and releases only inside
    // the callback; API4 also reads pinned custom-present policy without vcalls.
    // None issue GPU work or export native object pointers.
    // Begin/ReadState/End belong to one producer thread. End
    // disarms immediately but may require a later retry; retain the owner and
    // module until it succeeds. Static pinned hook state never retains this.
    class NativeRenderObservation final
    {
    public:
        NativeRenderObservation() = default;
        ~NativeRenderObservation();
        NativeRenderObservation(const NativeRenderObservation&) = delete;
        NativeRenderObservation& operator=(const NativeRenderObservation&) = delete;
        // A zero-callback run cannot choose a backend. The route is explicit;
        // observedRoute remains unknown until a validated original returns.
        bool Begin(RenderObservationRoute route = RenderObservationRoute::D3D11, bool queryGpuOwnership = false, bool classifyCustomPresent = false,
                   bool queryFrameBoundary = false, bool nativeDispatch = false) noexcept;
        bool End() noexcept;
        void ReadState(NativeRenderObservationState& state) const noexcept;
        // This separate cache does not add fields or claims to the V5 ABI.
        bool ReadEndpointDiagnostic(RenderEndpointDiagnosticV1& state) const noexcept;
        // Worker-only setup evidence survives End without changing any render
        // wire fields or granting observation/admission after a refusal.
        bool ReadSetupDiagnostic(RenderSetupDiagnosticV1& state) const noexcept;

    private:
        std::uint64_t                        generation = 0;
        mutable NativeRenderObservationState finalState;
    };
}
