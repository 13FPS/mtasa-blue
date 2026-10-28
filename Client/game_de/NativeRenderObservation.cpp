/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded diagnostic consumer of the real native present-hook owner
 *****************************************************************************/

#include "NativeRenderObservation.h"
#include "NativePresentOwner.h"
#include "NativeD3D12OwnershipProbe.h"
#include "NativeCustomPresentProbe.h"
#include "NativeFrameBoundaryProbe.h"
#include <array>
#include <atomic>
#include <windows.h>

namespace mta::game_de
{
    namespace
    {
        using Failure = NativeRenderObservationFailure;
        namespace Owner = NativePresentOwner;
        using PresentFunction = Owner::PresentFunction;
        using RouteDescriptor = Owner::RouteDescriptor;
        constexpr ULONGLONG maximumObservationMs = 30000;
        enum class DeviceCapturePhase : unsigned
        {
            Off,
            Armed,
            Candidate,
            Ready,
            Taken,
            Stopped
        };

        struct Runtime
        {
            std::atomic<bool>            active{false};
            std::atomic<std::uint64_t>   callbacks{0}, accepted{0}, rejected{0}, nested{0}, foreign{0}, missed{0};
            std::atomic<std::uint64_t>   registeredGeneration{0};
            std::atomic<std::uint32_t>   failure{0};
            std::atomic<DWORD>           callbackThread{0};
            SRWLOCK                      snapshotLock = SRWLOCK_INIT;
            NativeRenderObservationState snapshot;
            DWORD                        producerThread = 0;
            std::uintptr_t               image = 0;
            ULONGLONG                    started = 0;
            const RouteDescriptor*       route = nullptr;
            std::uint32_t                requestedRoute = 0;
            bool queryGpuOwnership = false, classifyCustomPresent = false, queryFrameBoundary = false, nativeDispatch = false, captureDeviceWindow = false;
            std::atomic<DeviceCapturePhase> devicePhase{DeviceCapturePhase::Off};
            std::atomic<bool>               devicePending{false}, deviceReleaseFault{false};
            RenderDeviceLeaseV1             deviceLease;
            DeviceWindowCandidate           deviceWindow;
        };
        // This bounded diagnostic storage is process lifetime. The shared hook
        // manager calls static code and never keeps a consumer-object pointer.
        Runtime runtime;
        using ApiOperation = NativePresentOwner::Operation;
        void Fail(Failure failure) noexcept
        {
            runtime.active.store(false);
            StopFrameBoundaryAdmission();
            std::uint32_t expected = 0;
            runtime.failure.compare_exchange_strong(expected, static_cast<std::uint32_t>(failure));
        }
        void OwnerFailure(std::uint32_t failure) noexcept
        {
            Fail(static_cast<Failure>(failure));
        }
        bool Read(std::uintptr_t address, void* destination, std::size_t bytes, std::uintptr_t image = 0, bool executable = false) noexcept
        {
            return Owner::ReadBytes(address, destination, bytes, image, executable);
        }
        template <typename T>
        bool Read(std::uintptr_t address, T& value) noexcept
        {
            return Read(address, &value, sizeof(value));
        }
        bool Capture(void* viewport, std::uint32_t syncInterval, NativeRenderObservationState& output) noexcept
        {
            const auto object = reinterpret_cast<std::uintptr_t>(viewport);
            if (!runtime.route)
                return false;
            const auto&                  route = *runtime.route;
            std::uintptr_t               table = 0, owner = 0, window = 0, swapchain = 0;
            NativeRenderObservationState next;
            if (!Read(object, table) || table != runtime.image + route.viewportTable || !Read(object + 0x18, owner) || !owner || !Read(object + 0x40, window) ||
                !window || !Read(object + 0x4c, next.width) || !next.width || next.width > 32768 || !Read(object + 0x50, next.height) || !next.height ||
                next.height > 32768 || !Read(object + route.swapchainOffset, swapchain) || !swapchain)
                return false;
            if (route.route == RenderObservationRoute::D3D11)
            {
                std::uintptr_t primary = 0, secondary = 0, global = 0, device = 0, context = 0;
                if (!Read(owner, primary) || primary != runtime.image + 0x44fb2b8 || !Read(owner + 0x18, secondary) || secondary != runtime.image + 0x44fafa8 ||
                    !Read(runtime.image + 0x576fb60, global) || global != owner || !Read(owner + 0x170, device) || !device || !Read(owner + 0x158, context) ||
                    !context)
                    return false;
                std::uintptr_t globalAgain = 0;
                if (!Read(runtime.image + 0x576fb60, globalAgain) || globalAgain != global)
                    return false;
                next.devicePresent = next.contextPresent = true;
                next.presenceKnownMask = 15;
            }
            else
            {
                // The pinned D3D12 creator explicitly passes pCommandQueue:
                // owner+8A8 -> device object+38 -> manager+28. Neither the
                // device object nor this queue is an immediate D3D11 context.
                // Only non-null queue presence is borrowed within this call.
                std::uintptr_t deviceObject = 0, manager = 0, queue = 0, deviceAgain = 0, managerAgain = 0, queueAgain = 0;
                if (!Read(owner + 0x8a8, deviceObject) || !deviceObject || !Read(deviceObject + 0x38, manager) || !manager || !Read(manager + 0x28, queue) ||
                    !queue || !Read(owner + 0x8a8, deviceAgain) || deviceAgain != deviceObject || !Read(deviceObject + 0x38, managerAgain) ||
                    managerAgain != manager || !Read(manager + 0x28, queueAgain) || queueAgain != queue)
                    return false;
                next.presenceKnownMask = 19;
                next.queuePresent = 1;
            }
            std::uintptr_t tableAgain = 0, ownerAgain = 0, swapchainAgain = 0;
            if (!Read(object, tableAgain) || tableAgain != table || !Read(object + 0x18, ownerAgain) || ownerAgain != owner ||
                !Read(object + route.swapchainOffset, swapchainAgain) || swapchainAgain != swapchain)
                return false;
            // The engine owns these pointers only for its natural call. Copy
            // presence/scalars and transfer no raw pointer to Core. API3's
            // separate callback-local probe releases its COM refs before return.
            next.callbackThreadId = GetCurrentThreadId();
            next.syncInterval = syncInterval;
            next.windowPresent = next.swapchainPresent = true;
            next.observedRoute = static_cast<std::uint32_t>(route.route);
            output = next;
            return true;
        }

        struct ProbeRoots
        {
            std::uintptr_t owner = 0, device = 0, manager = 0, queue = 0, swapchain = 0, customPresent = 0;
            std::uint32_t  explicitGpuCount = 0, afrCount = 0;
        };

        bool ReadProbeRoots(void* viewport, ProbeRoots& roots) noexcept
        {
            const auto object = reinterpret_cast<std::uintptr_t>(viewport);
            return Read(object + 0x18, roots.owner) && roots.owner && Read(roots.owner + 0x8a8, roots.device) && roots.device &&
                   Read(roots.device + 0x38, roots.manager) && roots.manager && Read(roots.manager + 0x28, roots.queue) && roots.queue &&
                   Read(object + 0x60, roots.swapchain) && roots.swapchain && Read(object + 0x1d0, roots.customPresent) &&
                   Read(runtime.image + 0x503e8e0, roots.explicitGpuCount) && Read(runtime.image + 0x503f45c, roots.afrCount);
        }

        bool SameProbeRoots(const ProbeRoots& a, const ProbeRoots& b) noexcept
        {
            return a.owner == b.owner && a.device == b.device && a.manager == b.manager && a.queue == b.queue && a.swapchain == b.swapchain &&
                   a.customPresent == b.customPresent && a.explicitGpuCount == b.explicitGpuCount && a.afrCount == b.afrCount;
        }

        bool BorrowedComRoot(std::uintptr_t object) noexcept
        {
            std::uintptr_t table = 0, queryInterface = 0;
            return Read(object, table) && Read(table, queryInterface) && Owner::ValidatePages(queryInterface, 1, 0, true);
        }

        class ImageCustomPresentReads final : public CustomPresentReads
        {
        public:
            bool Object(std::uintptr_t address, void* output, std::size_t bytes) override { return Read(address, output, bytes); }
            bool Image(std::uintptr_t address, void* output, std::size_t bytes, bool executable) override
            {
                // API5 owns one detour inside a known policy function. Recover
                // its pinned bytes only after verifying the entire exact owned
                // patch; historical API4 still checks the ordinary live image.
                if (runtime.queryFrameBoundary && executable && address == runtime.image + 0xc20780 && bytes == 44)
                    return ReadFrameBoundaryOwnedOriginal(address, output, bytes);
                return Read(address, output, bytes, runtime.image, executable);
            }
        };

        void CaptureOwnership(void* viewport, GpuOwnershipProbeState& state, CustomPresentClassificationState* custom = nullptr) noexcept
        {
            using Stage = GpuOwnershipQueryStage;
            using QueryStatus = GpuOwnershipQueryStatus;
            if (custom)
                *custom = {};
            state.queryStage = static_cast<std::uint32_t>(Stage::NativeRoots);
            state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Failed);
            ProbeRoots roots;
            if (!ReadProbeRoots(viewport, roots))
            {
                state.hresult = static_cast<std::uint32_t>(E_POINTER);
                if (custom)
                    custom->status = static_cast<std::uint32_t>(CustomPresentClassificationStatus::ReadFault);
                return;
            }
            state.customPresentPresent = roots.customPresent ? 1u : 0u;
            state.nativeExplicitGPUCount = roots.explicitGpuCount;
            state.nativeAFRCount = roots.afrCount;
            ImageCustomPresentReads customReads;
            CustomPresentProbeRoots customRoots;
            if (custom)
                ClassifyCustomPresent(customReads, runtime.image, roots.customPresent, roots.device, *custom, customRoots);
            bool       queried = false;
            const auto query = [&]() noexcept
            {
                // Indexed multi-GPU routes may choose another native queue. Keep
                // their historical refusal stage rather than query device zero.
                const bool singleNativeGpu = roots.explicitGpuCount == 1 && roots.afrCount == 1;
                state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Unsupported);
                state.queryStage = static_cast<std::uint32_t>(Stage::CustomPresent);
                if (roots.customPresent && !singleNativeGpu)
                    return;
                state.queryStage = static_cast<std::uint32_t>(Stage::SingleNode);
                if (!singleNativeGpu)
                    return;
                if (!BorrowedComRoot(roots.swapchain) || !BorrowedComRoot(roots.queue))
                {
                    state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Failed);
                    state.hresult = static_cast<std::uint32_t>(E_POINTER);
                    return;
                }
                // These are the existing native swapchain and creation queue even
                // when a custom presenter exists; their metadata does not identify
                // its chosen frame sink. The query's support predicate still
                // excludes that route. Release every local ref before the original
                // can present or rotate buffers; issue no GPU work or waits.
                queried = true;
                ProbeD3D12Ownership(reinterpret_cast<void*>(roots.swapchain), reinterpret_cast<void*>(roots.queue), state);
            };
            query();
            // API4's class policy belongs to the same borrowed native roots as
            // its COM metadata, including refused/failed COM attempts. API3
            // retains its historical readback only after an actual COM query.
            if (!queried && !custom)
                return;
            ProbeRoots after;
            const bool changed = !ReadProbeRoots(viewport, after) || !SameProbeRoots(roots, after);
            if (changed && state.queryStatus != static_cast<std::uint32_t>(QueryStatus::CleanupFailed))
            {
                state.queryStage = static_cast<std::uint32_t>(Stage::RootReadback);
                state.queryStatus = static_cast<std::uint32_t>(QueryStatus::RootsChanged);
                state.metadataValid = state.supportPredicate = 0;
                state.hresult = 0;
            }
            if (custom && (changed || !CustomPresentRootsUnchanged(customReads, customRoots)))
            {
                *custom = {};
                custom->status = static_cast<std::uint32_t>(CustomPresentClassificationStatus::RootsChanged);
            }
        }

        bool ReserveDeviceCandidate() noexcept
        {
            auto expected = DeviceCapturePhase::Armed;
            return runtime.active.load() && !runtime.failure.load() && runtime.devicePhase.compare_exchange_strong(expected, DeviceCapturePhase::Candidate);
        }
        void DeviceReleaseFailed() noexcept
        {
            // Release may have changed the reference count before raising. Do
            // not retry a possibly freed pointer or claim complete cleanup.
            runtime.deviceReleaseFault.store(true);
            runtime.devicePhase.store(DeviceCapturePhase::Stopped);
            Fail(Failure::GpuOwnershipCleanup);
        }
        void DiscardDeviceCandidate(FrameBoundaryScope& scope) noexcept
        {
            const bool reserved = scope.deviceReserved;
            scope.deviceReserved = false;
            if (!ReleaseRenderDeviceReference(scope.deviceCandidate))
                DeviceReleaseFailed();
            // Ignored/foreign wrappers have an empty unrelated scope. They
            // cannot release another callback's reservation, even when that
            // callback has reserved but its query has not acquired a ref yet.
            if (reserved)
            {
                auto expected = DeviceCapturePhase::Candidate;
                runtime.devicePhase.compare_exchange_strong(
                    expected, runtime.active.load() && !runtime.failure.load() ? DeviceCapturePhase::Armed : DeviceCapturePhase::Stopped);
            }
        }
        void CommitDeviceCandidate(FrameBoundaryScope& scope, const NativeRenderObservationState& capture, bool result, std::uint64_t acceptedNext) noexcept
        {
            const auto& state = scope.nativeDispatch;
            const auto& trace = scope.rootsDiagnostic;
            auto        phase = DeviceCapturePhase::Candidate;
            // This stricter, explicit handoff does not change which historical
            // scalar snapshots are accepted. A cached device is first-wins and
            // never confers permission to submit on the game's queue or target.
            if (scope.deviceReserved && scope.deviceCandidate && result && runtime.active.load() && !runtime.failure.load() && !scope.cleanupFailed &&
                state.presentEntries == 1 && !state.rejectedEntries &&
                state.status == static_cast<unsigned>(FrameBoundaryObservationStatus::ObservedCoherent) && state.endpointProfileId == 2 &&
                state.factsMask == 255 && state.queryStage == static_cast<unsigned>(FrameBoundaryQueryStage::Complete) && !state.queryHresult &&
                !state.presentHresult && !state.lifecycleFlags && trace.queryPhase == static_cast<unsigned>(RenderRootsDiagnosticPhase::AfterCloseReadback) &&
                trace.checkpoint == static_cast<unsigned>(RenderRootsDiagnosticCheckpoint::ReadComplete) && !trace.refusalReason &&
                (!runtime.captureDeviceWindow || (scope.deviceWindowCandidate.window && scope.deviceWindowCandidate.windowed == 1)) &&
                !runtime.devicePending.load() && runtime.devicePhase.compare_exchange_strong(phase, DeviceCapturePhase::Ready))
            {
                runtime.deviceLease = {};
                runtime.deviceLease.device = scope.deviceCandidate;
                scope.deviceCandidate = nullptr;
                runtime.deviceLease.generation = Owner::CurrentGeneration();
                runtime.deviceLease.acceptedSnapshots = acceptedNext;
                runtime.deviceLease.dispatchEntries = capture.nativeDispatch.presentEntries;
                runtime.deviceLease.sourceProfileId = 2;
                runtime.deviceWindow = runtime.captureDeviceWindow ? scope.deviceWindowCandidate : DeviceWindowCandidate{};
                runtime.devicePending.store(true);
            }
        }
        bool InvokeOriginal(PresentFunction original, void* viewport, std::uint32_t syncInterval, FrameBoundaryScope& scope, bool observe)
        {
            bool returned = false;
            __try
            {
                const bool result = original(viewport, syncInterval);
                returned = true;
                return result;
            }
            __finally
            {
                if (observe)
                    LeaveFrameBoundaryScope(scope);
                if (!returned)
                    DiscardDeviceCandidate(scope);
            }
        }

        void BoundaryCleanupFailure() noexcept
        {
            // The original Present may raise before publication. Preserve the
            // fatal release fault now rather than relying on its return path.
            Fail(Failure::GpuOwnershipCleanup);
        }

        bool PresentBody(const Owner::CallbackContext& context, void* viewport, std::uint32_t syncInterval)
        {
            // Registration belongs to this static diagnostic generation, not
            // to the lifetime of a C++ consumer object or a cached snapshot.
            if (!context.generation || context.generation != runtime.registeredGeneration.load() || context.generation != Owner::CurrentGeneration())
                return context.original(viewport, syncInterval);
            runtime.callbacks.fetch_add(1);
            if (runtime.active.load())
            {
                if (context.depth != 1)
                {
                    runtime.nested.fetch_add(1);
                    Fail(Failure::NestedCallback);
                }
                const auto now = GetTickCount64();
                if (now < runtime.started || now - runtime.started >= maximumObservationMs)
                    Fail(Failure::TimeBudget);
            }
            NativeRenderObservationState capture;
            bool                         accepted = false;
            if (runtime.active.load())
            {
                if (!Capture(viewport, syncInterval, capture))
                {
                    runtime.rejected.fetch_add(1);
                    Fail(Failure::InvalidViewport);
                }
                else
                {
                    DWORD expected = 0;
                    runtime.callbackThread.compare_exchange_strong(expected, GetCurrentThreadId());
                    if (runtime.callbackThread.load() != GetCurrentThreadId())
                        runtime.foreign.fetch_add(1);
                    else
                        accepted = true;
                }
            }
            bool fatalCleanup = false;
            if (accepted && runtime.queryGpuOwnership)
            {
                CaptureOwnership(viewport, capture.gpu, runtime.classifyCustomPresent ? &capture.custom : nullptr);
                fatalCleanup = capture.gpu.queryStatus == static_cast<std::uint32_t>(GpuOwnershipQueryStatus::CleanupFailed);
                // Disarm before either the original or a contended publication
                // can throw/drop this metadata. Unknown Release effects must
                // never be repeated or hidden by a missed snapshot.
                if (fatalCleanup)
                    Fail(Failure::GpuOwnershipCleanup);
            }
            FrameBoundaryScope boundary;
            const bool         observeBoundary = accepted && runtime.queryFrameBoundary && !fatalCleanup && runtime.active.load();
            if (observeBoundary)
                EnterFrameBoundaryScope(boundary, runtime.image, viewport, runtime.callbackThread.load(), capture.custom, &BoundaryCleanupFailure,
                                        runtime.devicePhase.load() == DeviceCapturePhase::Armed ? &ReserveDeviceCandidate : nullptr,
                                        runtime.captureDeviceWindow);
            const bool result = InvokeOriginal(context.original, viewport, syncInterval, boundary, observeBoundary);
            if (observeBoundary)
            {
                capture.boundary = boundary.state;
                capture.nativeDispatch = boundary.nativeDispatch;
                if (boundary.cleanupFailed)
                {
                    fatalCleanup = true;
                    Fail(Failure::GpuOwnershipCleanup);
                }
            }
            if (accepted && (runtime.active.load() || fatalCleanup))
            {
                if (TryAcquireSRWLockExclusive(&runtime.snapshotLock))
                {
                    capture.lastOriginalReturned = true;
                    capture.lastOriginalResult = result;
                    if (runtime.queryGpuOwnership)
                    {
                        // These count published probes. A missed publication
                        // cannot attach new counters to older COM metadata.
                        const auto previous = runtime.snapshot.gpu;
                        capture.gpu.attempts = previous.attempts + 1;
                        capture.gpu.accepted = previous.accepted + (capture.gpu.queryStatus == static_cast<std::uint32_t>(GpuOwnershipQueryStatus::Accepted));
                        capture.gpu.rejected = previous.rejected + (capture.gpu.queryStatus != static_cast<std::uint32_t>(GpuOwnershipQueryStatus::Accepted));
                    }
                    if (runtime.queryFrameBoundary)
                    {
                        if (!observeBoundary && fatalCleanup)
                            capture.boundary = runtime.snapshot.boundary;
                        else
                        {
                            capture.boundary.presentEntries += runtime.snapshot.boundary.presentEntries;
                            capture.boundary.rejectedEntries += runtime.snapshot.boundary.rejectedEntries;
                        }
                    }
                    if (runtime.nativeDispatch)
                    {
                        if (!observeBoundary && fatalCleanup)
                        {
                            capture.nativeDispatch = runtime.snapshot.nativeDispatch;
                            capture.rootsDiagnostic = runtime.snapshot.rootsDiagnostic;
                        }
                        else
                        {
                            capture.nativeDispatch.presentEntries += runtime.snapshot.nativeDispatch.presentEntries;
                            capture.nativeDispatch.rejectedEntries += runtime.snapshot.nativeDispatch.rejectedEntries;
                            // Publish only after the original returned, under the
                            // same lock/count commit as the associated API6 facts.
                            // This remains historical when a later publication misses.
                            capture.rootsDiagnostic = boundary.rootsDiagnostic;
                            capture.rootsDiagnostic.version = RenderRootsDiagnosticVersionV1;
                            capture.rootsDiagnostic.generationLow = static_cast<std::uint32_t>(Owner::CurrentGeneration());
                            capture.rootsDiagnostic.generationHigh = static_cast<std::uint32_t>(Owner::CurrentGeneration() >> 32);
                            const auto acceptedNext = runtime.accepted.load() + 1;
                            capture.rootsDiagnostic.acceptedSnapshotsLow = static_cast<std::uint32_t>(acceptedNext);
                            capture.rootsDiagnostic.acceptedSnapshotsHigh = static_cast<std::uint32_t>(acceptedNext >> 32);
                            capture.rootsDiagnostic.dispatchEntriesLow = static_cast<std::uint32_t>(capture.nativeDispatch.presentEntries);
                            capture.rootsDiagnostic.dispatchEntriesHigh = static_cast<std::uint32_t>(capture.nativeDispatch.presentEntries >> 32);
                        }
                    }
                    runtime.snapshot = capture;
                    CommitDeviceCandidate(boundary, capture, result, runtime.accepted.load() + 1);
                    if (observeBoundary)
                        PublishFrameBoundaryEndpointDiagnostic(boundary, runtime.accepted.load() + 1);
                    runtime.accepted.fetch_add(1);
                    ReleaseSRWLockExclusive(&runtime.snapshotLock);
                }
                else
                    runtime.missed.fetch_add(1);
            }
            // No foreign COM operation runs under the publication lock. A miss,
            // false original, failed dispatch or late root/lifetime change owns
            // only this callback's ref and cannot publish it retrospectively.
            DiscardDeviceCandidate(boundary);
            return result;
        }

        void ResetCounters() noexcept
        {
            runtime.callbacks.store(0);
            runtime.accepted.store(0);
            runtime.rejected.store(0);
            runtime.nested.store(0);
            runtime.foreign.store(0);
            runtime.missed.store(0);
            runtime.failure.store(0);
            runtime.callbackThread.store(0);
            AcquireSRWLockExclusive(&runtime.snapshotLock);
            runtime.snapshot = {};
            ReleaseSRWLockExclusive(&runtime.snapshotLock);
        }
    }

    NativeRenderObservation::~NativeRenderObservation()
    {
        End();
    }

    bool NativeRenderObservation::Begin(RenderObservationRoute route, bool queryGpuOwnership, bool classifyCustomPresent, bool queryFrameBoundary,
                                        bool nativeDispatch, bool captureDevice, bool captureDeviceWindow) noexcept
    {
        if (captureDeviceWindow && (captureDevice || !nativeDispatch || !queryFrameBoundary))
            return false;
        ApiOperation operation;
        if (!operation.acquired || runtime.active.load() || runtime.devicePending.load() || runtime.deviceReleaseFault.load())
            return false;
        const Owner::Config config{route, queryGpuOwnership, classifyCustomPresent, queryFrameBoundary, nativeDispatch, captureDevice, captureDeviceWindow};
        if (!Owner::Reserve(operation, config, &PresentBody, &OwnerFailure, generation))
            return false;
        finalState = {};
        const auto owned = Owner::ReadOwnerState();
        runtime.producerThread = owned.producerThread;
        runtime.image = owned.image;
        runtime.route = owned.route;
        runtime.registeredGeneration.store(generation);
        ResetCounters();
        runtime.requestedRoute = static_cast<std::uint32_t>(route);
        runtime.queryGpuOwnership = queryGpuOwnership;
        runtime.classifyCustomPresent = classifyCustomPresent;
        runtime.queryFrameBoundary = queryFrameBoundary;
        runtime.nativeDispatch = nativeDispatch;
        runtime.captureDeviceWindow = captureDeviceWindow;
        runtime.devicePhase.store(captureDevice || captureDeviceWindow ? DeviceCapturePhase::Armed : DeviceCapturePhase::Off);
        runtime.deviceLease = {};
        runtime.deviceWindow = {};
        ResetFrameBoundaryObservation();
        if (!Owner::Install(operation, generation))
        {
            ReadState(finalState);
            Owner::FinishFailedInstall(operation, generation);
            return false;
        }
        runtime.started = GetTickCount64();
        runtime.active.store(true);
        ReadState(finalState);
        return true;
    }

    bool NativeRenderObservation::End() noexcept
    {
        if (!generation || generation != Owner::CurrentGeneration())
            return true;
        ApiOperation operation;
        if (!operation.acquired)
            return false;
        if (!generation || generation != Owner::CurrentGeneration())
            return true;
        runtime.active.store(false);
        runtime.devicePhase.store(DeviceCapturePhase::Stopped);
        Owner::StopBoundaryAdmission(operation, generation);
        if (GetCurrentThreadId() != runtime.producerThread)
        {
            Fail(Failure::WrongProducerThread);
            return false;
        }
        if (!Owner::Detach(operation, generation))
        {
            ReadState(finalState);
            return false;
        }
        // Hook detach and every owned resume complete before the worker calls
        // COM. An untaken device belongs to this lease; a taken one belongs only
        // to Core and survives End independently of native object lifetimes.
        ID3D12Device* untaken = nullptr;
        if (runtime.devicePending.load())
        {
            if (!TryAcquireSRWLockExclusive(&runtime.snapshotLock))
                return false;
            untaken = runtime.deviceLease.device;
            runtime.deviceLease.device = nullptr;
            runtime.devicePending.store(false);
            ReleaseSRWLockExclusive(&runtime.snapshotLock);
        }
        if (!ReleaseRenderDeviceReference(untaken))
            DeviceReleaseFailed();
        if (runtime.deviceReleaseFault.load())
        {
            Owner::CleanupIncomplete(operation, generation);
            ReadState(finalState);
            return false;
        }
        if (!Owner::CompleteCleanup(operation, generation))
            return false;
        ReadState(finalState);
        // The manager, not the bounded snapshot, retains process admission until
        // both hook recovery and the diagnostic's untaken COM work have retired.
        return Owner::Release(operation, generation);
    }

    void NativeRenderObservation::ReadState(NativeRenderObservationState& state) const noexcept
    {
        state = finalState;
        state.modulePinned = Owner::ModulePinned();
        if (!generation || generation != Owner::CurrentGeneration())
            return;
        if (runtime.active.load())
        {
            const auto now = GetTickCount64();
            if (now < runtime.started || now - runtime.started >= maximumObservationMs)
                Fail(Failure::TimeBudget);
        }
        // A failed resume may retain a callback while it owns this lock. The
        // worker must remain able to retry cleanup; keep its last coherent
        // value cache if a nonblocking read cannot obtain a new publication.
        const auto owned = Owner::ReadOwnerState();
        const bool retained = owned.retained;
        const bool locked = retained ? TryAcquireSRWLockShared(&runtime.snapshotLock) != FALSE : (AcquireSRWLockShared(&runtime.snapshotLock), true);
        if (locked)
        {
            state = runtime.snapshot;
            // Count and snapshot are one publication; never attach a newer
            // atomic accepted count to the cached state on a lock miss.
            state.acceptedSnapshots = runtime.accepted.load();
            ReleaseSRWLockShared(&runtime.snapshotLock);
        }
        state.generation = generation;
        state.requestedRoute = runtime.requestedRoute;
        state.modulePinned = Owner::ModulePinned();
        state.bound = owned.bound;
        state.active = runtime.active.load();
        state.pendingCleanup = (owned.pendingHookWork || runtime.devicePending.load() || runtime.deviceReleaseFault.load()) && !state.active;
        // Completed effects precede their monotonically increasing prerequisite.
        state.rejectedSnapshots = runtime.rejected.load();
        state.nestedCallbacks = runtime.nested.load();
        state.foreignThreadCallbacks = runtime.foreign.load();
        state.publicationMisses = runtime.missed.load();
        state.nativePresentCallbacks = runtime.callbacks.load();
        state.failureReason = runtime.failure.load();
        finalState = state;
    }
    bool NativeRenderObservation::ReadEndpointDiagnostic(RenderEndpointDiagnosticV1& state) const noexcept
    {
        if (!generation || generation != Owner::CurrentGeneration() || GetCurrentThreadId() != runtime.producerThread || !runtime.queryFrameBoundary ||
            Owner::ReadOwnerState().retained)
            return false;
        // Use the same lock order as publication so a separate diagnostic read
        // cannot observe next-count metadata before the native snapshot commits.
        AcquireSRWLockShared(&runtime.snapshotLock);
        const bool result = ReadFrameBoundaryEndpointDiagnostic(generation, state);
        ReleaseSRWLockShared(&runtime.snapshotLock);
        return result;
    }
    bool NativeRenderObservation::ReadSetupDiagnostic(RenderSetupDiagnosticV1& state) const noexcept
    {
        return Owner::ReadSetupDiagnostic(generation, state);
    }

    bool NativeRenderObservation::ReadRootsDiagnostic(RenderRootsDiagnosticV1& state) const noexcept
    {
        // Exclude another lease's Begin/End without waiting, then try the same
        // callback snapshot lock used for its accepted-count publication. Optional
        // logging must never obstruct cleanup of a retained suspended peer.
        ApiOperation operation;
        if (!operation.acquired || !generation || generation != Owner::CurrentGeneration() || GetCurrentThreadId() != runtime.producerThread ||
            !runtime.nativeDispatch || Owner::ReadOwnerState().retained)
            return false;
        if (!TryAcquireSRWLockShared(&runtime.snapshotLock))
            return false;
        const auto next = runtime.snapshot.rootsDiagnostic;
        ReleaseSRWLockShared(&runtime.snapshotLock);
        if (next.version != RenderRootsDiagnosticVersionV1 || next.generationLow != static_cast<std::uint32_t>(generation) ||
            next.generationHigh != static_cast<std::uint32_t>(generation >> 32))
            return false;
        state = next;
        return true;
    }
    Status NativeRenderObservation::TakeDeviceLease(RenderDeviceLeaseV1& state) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || Owner::ReadOwnerState().retained)
            return Status::Unsupported;
        if (!generation || generation != Owner::CurrentGeneration() || GetCurrentThreadId() != runtime.producerThread)
            return Status::Unsupported;
        if (runtime.captureDeviceWindow)
            return Status::NotInitialized;
        if (!runtime.active.load() || runtime.failure.load() || runtime.deviceReleaseFault.load() || runtime.devicePhase.load() != DeviceCapturePhase::Ready)
            return Status::NotInitialized;
        if (!TryAcquireSRWLockExclusive(&runtime.snapshotLock))
            return Status::Unsupported;
        const auto next = runtime.deviceLease;
        const bool ready = runtime.active.load() && !runtime.failure.load() && runtime.devicePhase.load() == DeviceCapturePhase::Ready && next.device &&
                           next.generation == generation && next.acceptedSnapshots && next.dispatchEntries && next.sourceProfileId == 2;
        if (ready)
        {
            runtime.deviceLease.device = nullptr;
            runtime.devicePending.store(false);
            runtime.devicePhase.store(DeviceCapturePhase::Taken);
        }
        ReleaseSRWLockExclusive(&runtime.snapshotLock);
        if (!ready)
            return Status::NotInitialized;
        state = next;
        return Status::Ok;
    }
    Status NativeRenderObservation::TakeDeviceWindowLease(RenderDeviceWindowLeaseV2& state) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || Owner::ReadOwnerState().retained)
            return Status::Unsupported;
        if (!generation || generation != Owner::CurrentGeneration() || GetCurrentThreadId() != runtime.producerThread)
            return Status::Unsupported;
        if (!runtime.captureDeviceWindow || !runtime.active.load() || runtime.failure.load() || runtime.deviceReleaseFault.load() ||
            runtime.devicePhase.load() != DeviceCapturePhase::Ready)
            return Status::NotInitialized;
        if (!TryAcquireSRWLockExclusive(&runtime.snapshotLock))
            return Status::Unsupported;
        const auto device = runtime.deviceLease;
        const auto window = runtime.deviceWindow;
        const bool ready = runtime.active.load() && !runtime.failure.load() && runtime.devicePhase.load() == DeviceCapturePhase::Ready && device.device &&
                           device.generation == generation && device.acceptedSnapshots && device.dispatchEntries && device.sourceProfileId == 2 &&
                           window.window && window.windowed == 1;
        if (ready)
        {
            runtime.deviceLease.device = nullptr;
            runtime.deviceWindow = {};
            runtime.devicePending.store(false);
            runtime.devicePhase.store(DeviceCapturePhase::Taken);
        }
        ReleaseSRWLockExclusive(&runtime.snapshotLock);
        if (!ready)
            return Status::NotInitialized;
        // The HWND is a same-frame observation only. Moving this record performs
        // no COM, window operations or reference resurrection under either lock.
        RenderDeviceWindowLeaseV2 next;
        next.device = device.device;
        next.window = window.window;
        next.generation = device.generation;
        next.acceptedSnapshots = device.acceptedSnapshots;
        next.dispatchEntries = device.dispatchEntries;
        next.sourceProfileId = device.sourceProfileId;
        next.windowed = window.windowed;
        state = next;
        return Status::Ok;
    }
}
