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

    // API4 identifies a pinned custom-present implementation by guarded reads.
    // Its static callback policy never changes API3 support or proves that a
    // native Present branch executed, a GPU frame completed, or drawing is safe.
    inline constexpr std::uint32_t RenderObservationApiVersionV4 = 4;
    enum class CustomPresentClassificationStatus : std::uint32_t
    {
        NotAttempted = 0,
        Absent,
        KnownNativePresenter,
        UnknownImplementation,
        SignatureMismatch,
        ReadFault,
        DeviceMismatch,
        RootsChanged
    };
    enum class CustomPresentImplementation : std::uint32_t
    {
        None = 0,
        GamefaceD3D12NativePresenter = 1
    };
    struct CustomPresentClassificationState
    {
        // Only KnownNativePresenter publishes implementation1, all seven slot
        // bits, and policy15. SignatureMismatch/ReadFault/DeviceMismatch may
        // retain verified slot bits; every other status clears that mask.
        // Policy bits: native Present requested1, no fallback advance2,
        // post-Present no-op4, resize notification8. A known implementation is
        // independent of COM query success and still cannot admit GPU writes.
        std::uint32_t status = 0, implementationId = 0, slotSignatureMask = 0, staticPolicyFlags = 0;
    };

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

    // API3 is an explicit, read-only COM ownership query inside a naturally
    // invoked D3D12 callback. Older observers never acquire COM references.
    inline constexpr std::uint32_t RenderObservationApiVersionV3 = 3;
    enum class GpuOwnershipQueryStage : std::uint32_t
    {
        NotAttempted = 0,
        NativeRoots,
        CustomPresent,
        SingleNode,
        SwapchainQuery,
        QueueQuery,
        QueueDescription,
        QueueDevice,
        SwapchainDevice,
        CanonicalQueueDevice,
        CanonicalSwapchainDevice,
        SwapchainDescription,
        CurrentBackbufferIndex,
        Backbuffer,
        BackbufferDescription,
        BackbufferDevice,
        CanonicalBackbufferDevice,
        RootReadback,
        ReleaseReferences,
        Complete
    };
    enum class GpuOwnershipQueryStatus : std::uint32_t
    {
        NotAttempted = 0,
        Accepted,
        Unsupported,
        Failed,
        Exception,
        RootsChanged,
        CleanupFailed
    };
    struct GpuOwnershipProbeState
    {
        // Counts describe coherent published probes, one per accepted native
        // snapshot. A nonblocking publication miss retains the previous values.
        std::uint64_t attempts = 0, accepted = 0, rejected = 0;
        // metadataValid includes a fully queried unsupported result; it never
        // implies a safe GPU frame boundary, resource state, or GUI admission.
        std::uint32_t queryStage = 0, hresult = 0, metadataValid = 0, supportPredicate = 0;
        std::uint32_t queueType = 0, queuePriority = 0, queueFlags = 0, queueNodeMask = 0;
        std::uint32_t deviceNodeCount = 0, adapterLuidLow = 0, adapterLuidHigh = 0;
        // Canonical IUnknown identity: queue device1, SC device2, buffer device4.
        std::uint32_t canonicalIdentityFlags = 0;
        std::uint32_t currentBackbufferIndex = 0, swapchainBufferCount = 0, swapchainFormat = 0, swapchainFlags = 0;
        std::uint32_t swapchainWidth = 0, swapchainHeight = 0;
        std::uint64_t backbufferWidth = 0;
        std::uint32_t backbufferHeight = 0, backbufferFormat = 0, backbufferSampleCount = 0, backbufferSampleQuality = 0;
        std::uint32_t customPresentPresent = 0, queryStatus = 0, nativeExplicitGPUCount = 0, nativeAFRCount = 0;
    };
    struct RenderObservationStateV3
    {
        RenderObservationStateV2 base;
        GpuOwnershipProbeState   gpu;
    };
    class IGameRenderObservationV3
    {
    public:
        virtual Status Begin() noexcept = 0;
        virtual Status ReadState(RenderObservationStateV3& state) noexcept = 0;
        virtual Status End() noexcept = 0;
        virtual void   Release() noexcept = 0;

    protected:
        virtual ~IGameRenderObservationV3() = default;
    };
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport)
    IGameRenderObservationV3* CreateGameRenderObservationV3(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept;
#else
    extern "C" IGameRenderObservationV3* CreateGameRenderObservationV3(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept;
#endif

    struct RenderObservationStateV4
    {
        RenderObservationStateV3         base;
        CustomPresentClassificationState custom;
    };
    class IGameRenderObservationV4
    {
    public:
        virtual Status Begin() noexcept = 0;
        virtual Status ReadState(RenderObservationStateV4& state) noexcept = 0;
        virtual Status End() noexcept = 0;
        virtual void   Release() noexcept = 0;

    protected:
        virtual ~IGameRenderObservationV4() = default;
    };
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport)
    IGameRenderObservationV4* CreateGameRenderObservationV4(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept;
#else
    extern "C" IGameRenderObservationV4* CreateGameRenderObservationV4(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept;
#endif

    // API5 observes associations at the pinned standard Present entry. Even a
    // coherent result describes CPU bookkeeping and object identity only: it
    // cannot verify GPU execution, resource state, or permission to draw.
    inline constexpr std::uint32_t RenderObservationApiVersionV5 = 5;
    enum class FrameBoundaryObservationStatus : std::uint32_t
    {
        NotAttempted = 0,
        EndpointUnadmitted,
        ObservedCoherent,
        UnsupportedAssociation,
        WrongThreadOrNested,
        QueryFailed,
        RootsChanged,
        ResizeOverlap,
        EndpointChanged,
        PresentFailed,
        NoScopedPresent
    };
    enum class FrameBoundaryQueryStage : std::uint32_t
    {
        NotAttempted = 0,
        NativeRoots,
        SwapchainInterface,
        Backbuffer,
        NativeResourceComparison,
        QueueInterface,
        QueueIdentity,
        NativeTrackedState,
        RootReadback,
        Complete
    };
    struct FrameBoundaryObservationState
    {
        // endpointProfileId1 is this machine's fully pinned System32 DXGI.
        // Facts: canonical SC1, exact held buffer/native resource pointer2,
        // canonical native/creation queue4, direct context association8,
        // current index16, uniform CPU tracker32, tracked decoded zero64,
        // coherent roots/resize epoch128. None is a measured GPU fact.
        std::uint32_t status = 0, endpointProfileId = 0, factsMask = 0, presentHresult = 0;
        std::uint32_t nativeBufferIndex = 0, dxgiBufferIndex = 0, nativeTrackedState = 0, nativeTrackedMode = 0;
        // Lifecycle counters are copied with the completed native callback,
        // never attached later to an older published association snapshot.
        std::uint64_t presentEntries = 0, rejectedEntries = 0, resizeEpoch = 0, resizeEntries = 0, preReleaseEntries = 0, foreignLifecycleEntries = 0;
        std::uint32_t presentThreadId = 0, resizeThreadId = 0, preReleaseThreadId = 0, lifecycleFlags = 0, queryStage = 0, queryHresult = 0;
    };
    struct RenderObservationStateV5
    {
        RenderObservationStateV4      base;
        FrameBoundaryObservationState boundary;
    };
    class IGameRenderObservationV5
    {
    public:
        virtual Status Begin() noexcept = 0;
        virtual Status ReadState(RenderObservationStateV5& state) noexcept = 0;
        virtual Status End() noexcept = 0;
        virtual void   Release() noexcept = 0;

    protected:
        virtual ~IGameRenderObservationV5() = default;
    };
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport)
    IGameRenderObservationV5* CreateGameRenderObservationV5(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept;
#else
    extern "C" IGameRenderObservationV5* CreateGameRenderObservationV5(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept;
#endif
}
