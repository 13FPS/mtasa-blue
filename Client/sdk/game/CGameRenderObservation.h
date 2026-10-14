/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional value-only native render observations owned by MTA Core
 *****************************************************************************/

#pragma once

#include "CGameAdapter.h"

namespace mta::game_de
{
    // This optional diagnostics ABI does not extend the gameplay adapter's
    // vtable or capabilities. Observing a native wrapper is not a renderer.
    inline constexpr std::uint32_t RenderObservationApiVersion = 1;

    struct RenderObservationState
    {
        std::uint32_t bound = 0;
        std::uint32_t active = 0;
        std::uint32_t modulePinned = 0;
        std::uint32_t pendingCleanup = 0;
        std::uint64_t generation = 0;
        std::uint64_t nativePresentCallbacks = 0;
        std::uint64_t acceptedSnapshots = 0;
        std::uint64_t rejectedSnapshots = 0;
        std::uint64_t nestedCallbacks = 0;
        std::uint64_t foreignThreadCallbacks = 0;
        std::uint64_t publicationMisses = 0;
        std::uint32_t callbackThreadId = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t syncInterval = 0;
        std::uint32_t failureReason = 0;
        std::uint32_t windowPresent = 0;
        std::uint32_t swapchainPresent = 0;
        std::uint32_t devicePresent = 0;
        std::uint32_t contextPresent = 0;
        // A returned false native bool is distinct from an original which has
        // not returned; neither value proves a COM Present branch executed.
        std::uint32_t lastOriginalReturned = 0;
        std::uint32_t lastOriginalResult = 0;
    };

    class IGameRenderObservation
    {
    public:
        virtual Status Begin() noexcept = 0;
        virtual Status ReadState(RenderObservationState& state) noexcept = 0;
        virtual Status End() noexcept = 0;
        // Finish End successfully before releasing this factory-owned lease.
        // Failed cleanup retains the object and its module for an owner retry.
        virtual void Release() noexcept = 0;

    protected:
        virtual ~IGameRenderObservation() = default;
    };

#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport) IGameRenderObservation* CreateGameRenderObservation(std::uint32_t apiVersion, CCoreInterface* owner) noexcept;
#else
    extern "C" IGameRenderObservation* CreateGameRenderObservation(std::uint32_t apiVersion, CCoreInterface* owner) noexcept;
#endif

    // API1 remains the historical D3D11-only contract. A separate factory and
    // vtable keep route selection from changing any existing caller's layout.
    enum class RenderObservationRoute : std::uint32_t
    {
        Unknown = 0,
        D3D11 = 1,
        D3D12 = 2,
    };
    inline constexpr std::uint32_t RenderObservationApiVersionV2 = 2;

    struct RenderObservationStateV2
    {
        RenderObservationState base;
        std::uint32_t          requestedRoute = 0;
        // A request is not evidence of the active RHI. This stays unknown until
        // the selected route accepts and publishes a naturally invoked callback.
        std::uint32_t observedRoute = 0;
        // Known presence bits: window1, swapchain2, device4, immediateContext8,
        // commandQueue16. Unknown members stay zero, not implicitly absent.
        std::uint32_t presenceKnownMask = 0;
        std::uint32_t queuePresent = 0;
    };

    class IGameRenderObservationV2
    {
    public:
        virtual Status Begin() noexcept = 0;
        virtual Status ReadState(RenderObservationStateV2& state) noexcept = 0;
        virtual Status End() noexcept = 0;
        virtual void   Release() noexcept = 0;

    protected:
        virtual ~IGameRenderObservationV2() = default;
    };

#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport)
    IGameRenderObservationV2* CreateGameRenderObservationV2(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept;
#else
    extern "C" IGameRenderObservationV2* CreateGameRenderObservationV2(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept;
#endif
}
