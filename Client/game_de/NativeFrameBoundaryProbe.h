/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read-only scoped observations at an admitted standard Present entry
 *****************************************************************************/
#pragma once
#include "../sdk/game/CGameRenderObservation.h"
#include "../sdk/game/CGameRenderEndpointDiagnostic.h"
#include "../sdk/game/CGameRenderRootsDiagnostic.h"
#include "../sdk/game/CGameRenderDeviceLease.h"
#include <cstddef>

namespace mta::game_de
{
    // This separate TLS record comes only from the real manager's natural
    // wrapper callback. It grants no GPU/COM diagnostic status or native lease.
    struct NativePresentPolicyScope
    {
        std::uint64_t             generation = 0;
        std::uintptr_t            image = 0, viewport = 0;
        std::uint32_t             callbackThread = 0;
        unsigned                  depth = 0;
        NativePresentPolicyScope* previous = nullptr;
        bool                      entered = false;
    };
    using NativePolicySelector = std::uint32_t (*)(const NativePresentPolicyScope&, std::uintptr_t chain, std::uintptr_t target, std::uint32_t incoming,
                                                   std::uint32_t flags, const void* caller, unsigned dispatchDepth) noexcept;

    // One worker serializes configuration/retirement under the actual manager's
    // transition ownership. Configure requires the existing retained DXGI ref.
    // Retire follows final detach/drain and M unregister; immediate producer
    // retirement only disarms the manager's intent while this storage remains.
    bool ConfigureNativePresentPolicy(std::uint64_t generation, NativePolicySelector selector) noexcept;
    bool RetireNativePresentPolicy(std::uint64_t generation) noexcept;
    void EnterNativePresentPolicyScope(NativePresentPolicyScope& scope, std::uint64_t generation, std::uintptr_t image, void* viewport,
                                       std::uint32_t callbackThread, unsigned depth) noexcept;
    void LeaveNativePresentPolicyScope(NativePresentPolicyScope& scope) noexcept;
    // The real manager holds only its M short read around qualification and its
    // atomic decision, then explicitly releases it before returning an interval.
    // No original, COM, hash, loader or SRW operation occurs in qualification.
    bool QualifyNativePresentPolicyEdge(const NativePresentPolicyScope& scope, std::uintptr_t chain, std::uintptr_t target, const void* caller,
                                        unsigned dispatchDepth) noexcept;
    bool NativePresentPolicyPoisoned(std::uint64_t generation) noexcept;
    bool NativePolicyHooksCurrent(std::uint64_t generation) noexcept;
    // Shared begin resets only diagnostic state. The caller owns Render and has
    // already closed/drained both its consumer and this diagnostic scope count.
    bool     BeginSharedFrameBoundaryDiagnostic(std::uint64_t generation, bool boundaryEnabled) noexcept;
    unsigned FrameBoundaryDiagnosticInFlight() noexcept;

    struct DeviceWindowCandidate
    {
        void*         window = nullptr;
        std::uint32_t windowed = 0;
    };
    // The query executor lets owned fixtures exercise faults and cleanup without
    // loading the game, installing its hooks, or submitting GPU work.
    class FrameBoundaryQueries
    {
    public:
        virtual std::int32_t Execute(FrameBoundaryQueryStage stage, FrameBoundaryObservationState& state) = 0;
        virtual bool         Close() noexcept = 0;
        // An explicit device request may move an already acquired reference;
        // ordinary observations never call this or retain a COM object.
        virtual ID3D12Device* DetachDeviceCandidate() noexcept { return nullptr; }
        virtual bool          CaptureDeviceWindowCandidate(DeviceWindowCandidate&) noexcept { return false; }

    protected:
        ~FrameBoundaryQueries() = default;
    };
    bool RunFrameBoundaryQueries(FrameBoundaryQueries& queries, FrameBoundaryObservationState& state, RenderRootsDiagnosticV1* trace = nullptr,
                                 ID3D12Device** deviceCandidate = nullptr, DeviceWindowCandidate* windowCandidate = nullptr) noexcept;
    bool ReleaseRenderDeviceReference(ID3D12Device*& device) noexcept;

    struct FrameBoundaryScope
    {
        FrameBoundaryObservationState state;
        FrameBoundaryObservationState nativeDispatch;
        RenderEndpointDiagnosticV1    diagnostic;
        RenderRootsDiagnosticV1       rootsDiagnostic;
        std::uintptr_t                image = 0, viewport = 0, swapchain = 0, nativeSwapchain = 0;
        std::uint32_t                 callbackThread = 0;
        bool                          knownPresenter = false, cleanupFailed = false, entered = false, dispatchEntered = false;
        bool                          diagnosticCounted = false;
        std::uint64_t                 policyEpoch = 0, policyGeneration = 0;
        std::uint64_t                 epoch = 0;
        void (*cleanupFailure)() noexcept = nullptr;
        FrameBoundaryScope*   previous = nullptr;
        ID3D12Device*         deviceCandidate = nullptr;
        bool                  deviceReserved = false;
        bool                  captureDeviceWindow = false;
        DeviceWindowCandidate deviceWindowCandidate;
        bool (*reserveDeviceCandidate)() noexcept = nullptr;
    };
    // Prepare is worker-only and performs no COM calls. Change appends to the
    // observer's existing all-thread Detours transaction; it never commits one.
    bool     PrepareFrameBoundaryHooks(std::uintptr_t image, std::uint64_t generation = 0, bool nativeDispatch = false, void* nativeWrapperHook = nullptr,
                                       bool policyOnly = false) noexcept;
    bool     ChangeFrameBoundaryHooks(bool install) noexcept;
    bool     FrameBoundaryHooksCommitted(bool install) noexcept;
    bool     FrameBoundaryHooksOwned() noexcept;
    unsigned FrameBoundaryInFlight() noexcept;
    void     StopFrameBoundaryAdmission() noexcept;
    void     ResetFrameBoundaryObservation() noexcept;
    void     EnterFrameBoundaryScope(FrameBoundaryScope& scope, std::uintptr_t image, void* viewport, std::uint32_t callbackThread,
                                     const CustomPresentClassificationState& custom, void (*cleanupFailure)() noexcept = nullptr,
                                     bool (*reserveDeviceCandidate)() noexcept = nullptr, bool captureDeviceWindow = false) noexcept;
    void     LeaveFrameBoundaryScope(FrameBoundaryScope& scope) noexcept;
    // API5 alone may normalize our verified installed pre-release patch back to
    // its fully pinned original bytes. Historical API4 uses its original reader.
    bool ReadFrameBoundaryOwnedOriginal(std::uintptr_t address, void* output, std::size_t bytes) noexcept;
    void PublishFrameBoundaryEndpointDiagnostic(const FrameBoundaryScope& scope, std::uint64_t acceptedSnapshots) noexcept;
    bool ReadFrameBoundaryEndpointDiagnostic(std::uint64_t generation, RenderEndpointDiagnosticV1& state) noexcept;
    void ReleaseFrameBoundaryDiagnosticModules() noexcept;
    // The worker holds its own suspended-thread references across the Detours
    // transaction, so custom CALL rollback finishes before any thread resumes.
    bool NativeDispatchInstructionSafe(std::uintptr_t instruction, std::uintptr_t nativeWrapperTrampoline = 0) noexcept;
    bool CaptureNativeDispatchProtectionBaseline() noexcept;
    bool ChangeNativeDispatchCallsite(bool install, bool nativeEntryInstalled) noexcept;
    bool NativeDispatchPatchPresent() noexcept;
    bool NativeDispatchDetoursPresent() noexcept;
    bool NativeDispatchSafeToResume(bool nativeEntryInstalled) noexcept;
    bool ReleaseNativeDispatchRelay() noexcept;
}
