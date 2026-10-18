/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the actual MTA Core-owned Game DE module consumer
 *****************************************************************************/

#include "../../Client/core/CGameAdapterModule.h"

#include <filesystem>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cstddef>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <windows.h>

// These independently fixed API1 offsets prevent adding route fields to the
// old public state by accident. API2 composes it rather than extending its ABI.
static_assert(mta::game_de::RenderObservationApiVersion == 1);
static_assert(sizeof(mta::game_de::RenderObservationState) == 120);
static_assert(offsetof(mta::game_de::RenderObservationState, generation) == 16);
static_assert(offsetof(mta::game_de::RenderObservationState, lastOriginalResult) == 112);
static_assert(offsetof(mta::game_de::RenderObservationStateV2, base) == 0);
static_assert(offsetof(mta::game_de::RenderObservationStateV2, requestedRoute) == 120);
static_assert(sizeof(mta::game_de::RenderObservationStateV2) == 136);
static_assert(mta::game_de::RenderObservationApiVersionV3 == 3);
static_assert(sizeof(mta::game_de::RenderObservationStateV3) == 272);
static_assert(mta::game_de::RenderObservationApiVersionV4 == 4);
static_assert(offsetof(mta::game_de::RenderObservationStateV4, base) == 0);
static_assert(offsetof(mta::game_de::RenderObservationStateV4, custom) == 272);
static_assert(sizeof(mta::game_de::RenderObservationStateV4) == 288);
static_assert(mta::game_de::RenderObservationApiVersionV5 == 5);
static_assert(offsetof(mta::game_de::RenderObservationStateV5, boundary) == 288);
static_assert(sizeof(mta::game_de::RenderObservationStateV5) == 392);
static_assert(offsetof(mta::game_de::RenderObservationStateV3, base) == 0);
static_assert(offsetof(mta::game_de::RenderObservationStateV3, gpu) == sizeof(mta::game_de::RenderObservationStateV2));

// This new pointer-bearing contract is separate from every scalar observation
// state. Its control fixture uses an opaque token and never calls COM or GPU.
#if defined(_WIN64)
static_assert(sizeof(mta::game_de::RenderDeviceLeaseV1) == 48);
static_assert(offsetof(mta::game_de::RenderDeviceLeaseV1, device) == 8);
static_assert(offsetof(mta::game_de::RenderDeviceLeaseV1, generation) == 16);
static_assert(offsetof(mta::game_de::RenderDeviceLeaseV1, sourceProfileId) == 40);
static_assert(sizeof(mta::game_de::RenderDeviceWindowLeaseV2) == 64);
static_assert(offsetof(mta::game_de::RenderDeviceWindowLeaseV2, window) == 16);
static_assert(offsetof(mta::game_de::RenderDeviceWindowLeaseV2, generation) == 24);
static_assert(offsetof(mta::game_de::RenderDeviceWindowLeaseV2, reserved) == 56);
#endif
struct FixtureDeviceStats
{
    std::uint32_t allocations = 0, requests = 0, begins = 0, takes = 0, transfers = 0, releases = 0, ends = 0;
    std::uint32_t order = 0, requestOrder = 0, beginOrder = 0, liveLeases = 0, control = 0;
};

struct FixtureClockStats
{
    std::uint32_t begins = 0, submits = 0, renewals = 0, reads = 0, ends = 0, shutdowns = 0, releases = 0;
    // These gates model provider-owned cleanup only; no GTA memory or suspended
    // process thread is needed to test the actual Core consumer's allocator.
    std::uint32_t control = 0;
};

struct FixtureLocalInputStats
{
    std::uint32_t               begins = 0, renewals = 0, reads = 0, ends = 0, resets = 0, shutdowns = 0, releases = 0, submissions = 0, canonicalReads = 0;
    std::uint32_t               allowCleanup = 0, neutralBegin = 0;
    mta::game_de::MtaInputState lastRenewed;
};

#if !defined(MTA_TEST_SHUTDOWN_FAILURE_MODULE)
namespace
{
    std::atomic<bool>     observeCoreAllocations{false};
    std::atomic<unsigned> observedCoreAllocations{0};
    [[maybe_unused]] void ArmCoreAllocationObservation() noexcept
    {
        observeCoreAllocations.store(true);
    }
}
// Observe the actual consumer's CRT requests without blocking or changing its
// allocator. The controlled provider arms this only as Begin/End returns, after
// preparation may allocate but retained cleanup must remain allocation-free.
void* operator new(std::size_t bytes)
{
    if (observeCoreAllocations.load())
        ++observedCoreAllocations;
    if (void* memory = std::malloc(bytes ? bytes : 1))
        return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes)
{
    return ::operator new(bytes);
}
void operator delete(void* memory) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory) noexcept
{
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}
#endif

#if defined(MTA_TEST_SHUTDOWN_FAILURE_MODULE)
    #ifndef MTA_TEST_FACADE_MODE
        #define MTA_TEST_FACADE_MODE 0
    #endif
    #ifndef MTA_TEST_RENDER_MODE
        #define MTA_TEST_RENDER_MODE 0
    #endif
    #ifndef MTA_TEST_RENDER_V2
        #define MTA_TEST_RENDER_V2 0
    #endif
    #ifndef MTA_TEST_RENDER_V3
        #define MTA_TEST_RENDER_V3 0
    #endif
    #ifndef MTA_TEST_RENDER_V6
        #define MTA_TEST_RENDER_V6 0
    #endif
    #ifndef MTA_TEST_RENDER_V5
        #define MTA_TEST_RENDER_V5 0
    #endif
    #ifndef MTA_TEST_RENDER_V4
        #define MTA_TEST_RENDER_V4 0
    #endif
    #ifndef MTA_TEST_CLOCK_MODE
        #define MTA_TEST_CLOCK_MODE 0
    #endif
    #ifndef MTA_TEST_LOCAL_INPUT_MODE
        #define MTA_TEST_LOCAL_INPUT_MODE 0
    #endif
// This deliberately failing provider tests Core's DLL lifetime only. It is not
// an engine writer, replacement Game DE implementation or gameplay fixture.
namespace mta::game_de
{
    namespace
    {
        std::uint32_t*            fixtureCounters = nullptr;
        IGameRenderObservationV5* diagnosticLease = nullptr;
        IGameRenderObservationV6* diagnosticLeaseV6 = nullptr;
        DWORD                     diagnosticOwnerThread = 0;
        std::uint32_t             diagnosticReads = 0;
        std::uint32_t             setupGeneration = 0;
        std::uint32_t             setupDiagnosticReads = 0, setupCleanupAttempted = 0, setupCleanupComplete = 0;
        std::uint32_t             rootsDiagnosticReads = 0;
        FixtureDeviceStats*       deviceFixtureStats = nullptr;
        alignas(void*) unsigned char deviceIdentity[sizeof(void*)]{};
        alignas(void*) unsigned char windowIdentity[sizeof(void*)]{};
        void (*v6OperationObserver)() noexcept = nullptr;
        alignas(void*) unsigned char facadeIdentity[sizeof(void*)]{};
    #if MTA_TEST_LOCAL_INPUT_MODE
        FixtureLocalInputStats*    localFixtureStats = nullptr;
        LocalInputLeaseStateV1     fixtureLocalState;
        MtaInputState              fixtureLocalInput;
        MtaInput::DirectionHistory fixtureLocalHistory;
        bool                       fixtureLocalHasInput = false, fixtureLocalUnknownDebt = false;
    #endif
    #if MTA_TEST_CLOCK_MODE
        FixtureClockStats*  clockFixtureStats = nullptr;
        ClockServiceStateV1 fixtureClockState;
        void (*clockOperationObserver)() noexcept = nullptr;
        void ObserveClockOperation() noexcept
        {
            if (clockOperationObserver)
                clockOperationObserver();
        }
    #endif

        class ShutdownFailureAdapter final : public IGameAdapter
        {
        public:
            Status Initialize(CCoreInterface*) noexcept override { return Status::Ok; }
            Status Pulse() noexcept override { return Status::Ok; }
            Status Reset() noexcept override
            {
                if (fixtureCounters)
                {
                    ++fixtureCounters[3];
                    fixtureCounters[10] = ++fixtureCounters[8];
                }
    #if MTA_TEST_LOCAL_INPUT_MODE
                if (localFixtureStats)
                    ++localFixtureStats->resets;
                if ((fixtureLocalState.active || fixtureLocalState.bound || fixtureLocalUnknownDebt) &&
                    (!localFixtureStats || !localFixtureStats->allowCleanup))
                    return Status::Unsupported;
                fixtureLocalState.active = fixtureLocalState.bound = 0;
                fixtureLocalUnknownDebt = fixtureLocalHasInput = false;
                return Status::Ok;
    #else
                return MTA_TEST_RENDER_MODE ? Status::Ok : Status::Unsupported;
    #endif
            }
            Status Shutdown() noexcept override
            {
                ++shutdownCount;
                if (fixtureCounters)
                {
                    ++fixtureCounters[0];
                    fixtureCounters[11] = ++fixtureCounters[8];
                }
    #if MTA_TEST_LOCAL_INPUT_MODE
                if (localFixtureStats)
                    ++localFixtureStats->shutdowns;
                return fixtureLocalState.active || fixtureLocalState.bound || fixtureLocalUnknownDebt ? Status::Unsupported : Status::Ok;
    #elif MTA_TEST_CLOCK_MODE
                if (clockFixtureStats)
                    ++clockFixtureStats->shutdowns;
                const bool retained = fixtureClockState.hooked || fixtureClockState.retainedPeerThreads ||
                                      (MTA_TEST_CLOCK_MODE == 3 && clockFixtureStats && !(clockFixtureStats->control & 2));
                ObserveClockOperation();
                return retained ? Status::Unsupported : Status::Ok;
    #else
                return !MTA_TEST_RENDER_MODE && (MTA_TEST_FACADE_MODE == 0 || MTA_TEST_FACADE_MODE == 3) && shutdownCount == 1 ? Status::Unsupported
                                                                                                                               : Status::Ok;
    #endif
            }
            void Release() noexcept override
            {
    #if MTA_TEST_LOCAL_INPUT_MODE
                if (localFixtureStats)
                    ++localFixtureStats->releases;
    #endif
    #if MTA_TEST_CLOCK_MODE
                if (clockFixtureStats)
                    ++clockFixtureStats->releases;
    #endif
                if (fixtureCounters)
                {
                    ++fixtureCounters[1];
                    fixtureCounters[12] = ++fixtureCounters[8];
                }
                delete this;
            }
            std::uint64_t GetCapabilities() const noexcept override
            {
                return MTA_TEST_FACADE_MODE >= 2 ? static_cast<std::uint64_t>(AdapterCapability::GameInterface) : 0;
            }
            CGame* GetGameInterface() const noexcept override
            {
                if (fixtureCounters)
                    ++fixtureCounters[2];
                return MTA_TEST_FACADE_MODE == 1 || MTA_TEST_FACADE_MODE == 3 ? reinterpret_cast<CGame*>(facadeIdentity) : nullptr;
            }
            Status PublishLocalObservation(const LocalObservation&) noexcept override { return Status::Unsupported; }
            Status GetLocalObservation(LocalObservation&) noexcept override { return Status::Unsupported; }
            Status SubmitControlActions(const ControlActionFrame& frame) noexcept override
            {
    #if MTA_TEST_LOCAL_INPUT_MODE
                fixtureLocalInput.previous = fixtureLocalInput.current;
                fixtureLocalInput.current = MtaInput::BuildControllerState(frame.heldActions, frame.context, fixtureLocalHistory);
                fixtureLocalHasInput = true;
                if (localFixtureStats)
                    ++localFixtureStats->submissions;
                return Status::Ok;
    #else
                (void)frame;
                return Status::Unsupported;
    #endif
            }
            Status GetMtaInputState(MtaInputState& state) noexcept override
            {
    #if MTA_TEST_LOCAL_INPUT_MODE
                if (localFixtureStats)
                    ++localFixtureStats->canonicalReads;
                if (!fixtureLocalHasInput)
                    return Status::NotInitialized;
                state = fixtureLocalInput;
                return Status::Ok;
    #else
                (void)state;
                return Status::Unsupported;
    #endif
            }
            Status BindNativeInput() noexcept override { return Status::Unsupported; }
            Status ReadNativeControllerState(std::uint32_t, NativeControllerState&) noexcept override { return Status::Unsupported; }
            Status ReadNativeDeviceState(NativeDeviceState&) noexcept override { return Status::Unsupported; }
            Status BeginNativeMovementTrial() noexcept override { return Status::Unsupported; }
            Status GetNativeMovementTrialState(NativeMovementTrialState&) noexcept override { return Status::Unsupported; }
            Status EndNativeMovementTrial() noexcept override { return Status::Unsupported; }
            Status GetNativeMovementEligibility(NativeMovementEligibility&) noexcept override { return Status::Unsupported; }

        private:
            unsigned int shutdownCount = 0;
        };

        // Controlled optional-provider values exercise the consumer lifetime;
        // they are not observations from native rendering or a pinned DLL.
        class RenderFixture final : public IGameRenderObservation
        {
        public:
            Status Begin() noexcept override
            {
                if (fixtureCounters)
                    ++fixtureCounters[4];
                state.bound = state.active = 1;
                state.generation = 17;
                state.nativePresentCallbacks = 9;
                state.acceptedSnapshots = 7;
                state.publicationMisses = 2;
                state.width = 640;
                state.height = 480;
                state.lastOriginalReturned = 1;
                state.lastOriginalResult = 0;
                return MTA_TEST_RENDER_MODE == 2 ? Status::Unsupported : Status::Ok;
            }
            Status ReadState(RenderObservationState& output) noexcept override
            {
                if (fixtureCounters)
                    ++fixtureCounters[5];
                if (MTA_TEST_RENDER_MODE == 3 && ended && !failedFinalRead)
                {
                    failedFinalRead = true;
                    return Status::Unsupported;
                }
                output = state;
                return Status::Ok;
            }
            Status End() noexcept override
            {
                if (fixtureCounters)
                    ++fixtureCounters[6];
                ++endCount;
                ended = true;
                state.active = 0;
                state.pendingCleanup = 0;
                if ((MTA_TEST_RENDER_MODE == 1 || MTA_TEST_RENDER_MODE == 2) && endCount == 1)
                {
                    state.pendingCleanup = 1;
                    return Status::Unsupported;
                }
                if (MTA_TEST_RENDER_MODE == 5 && endCount == 1)
                    state.active = state.pendingCleanup = 1;
                return Status::Ok;
            }
            void Release() noexcept override
            {
                if (fixtureCounters)
                {
                    ++fixtureCounters[7];
                    fixtureCounters[9] = ++fixtureCounters[8];
                }
                delete this;
            }

        private:
            RenderObservationState state;
            unsigned               endCount = 0;
            bool                   ended = false;
            bool                   failedFinalRead = false;
        };

        class RenderFixtureV2 final : public IGameRenderObservationV2
        {
        public:
            explicit RenderFixtureV2(RenderObservationRoute selected) : route(selected) {}
            Status Begin() noexcept override { return observation.Begin(); }
            Status End() noexcept override { return observation.End(); }
            Status ReadState(RenderObservationStateV2& output) noexcept override
            {
                RenderObservationStateV2 next;
                const auto               status = observation.ReadState(next.base);
                if (status != Status::Ok)
                    return status;
                next.requestedRoute = static_cast<std::uint32_t>(route);
                if (MTA_TEST_RENDER_MODE == 6)
                {
                    // A bound request with no native callbacks is still unknown.
                    next.base.nativePresentCallbacks = next.base.acceptedSnapshots = next.base.publicationMisses = 0;
                    next.base.lastOriginalReturned = next.base.lastOriginalResult = 0;
                    next.base.width = next.base.height = 0;
                }
                else
                {
                    next.observedRoute = next.requestedRoute;
                    next.presenceKnownMask = route == RenderObservationRoute::D3D11 ? 15u : 19u;
                    next.base.windowPresent = next.base.swapchainPresent = 1;
                    next.base.devicePresent = route == RenderObservationRoute::D3D11 ? 1u : 0u;
                    next.base.contextPresent = route == RenderObservationRoute::D3D11 ? 1u : 0u;
                    next.queuePresent = route == RenderObservationRoute::D3D12 ? 1u : 0u;
                }
                if (MTA_TEST_RENDER_MODE == 7 && fixtureCounters && fixtureCounters[13])
                    next.observedRoute = route == RenderObservationRoute::D3D11 ? 2u : 1u;
                output = next;
                return Status::Ok;
            }
            void Release() noexcept override
            {
                if (fixtureCounters)
                {
                    ++fixtureCounters[7];
                    fixtureCounters[9] = ++fixtureCounters[8];
                }
                delete this;
            }

        private:
            const RenderObservationRoute route;
            RenderFixture                observation;
        };

        class RenderFixtureV3 final : public IGameRenderObservationV3
        {
        public:
            Status Begin() noexcept override { return observation.Begin(); }
            Status End() noexcept override { return observation.End(); }
            Status ReadState(RenderObservationStateV3& output) noexcept override
            {
                RenderObservationStateV3 next;
                const auto               status = observation.ReadState(next.base);
                if (status != Status::Ok)
                    return status;
                if (MTA_TEST_RENDER_MODE != 6)
                {
                    next.gpu.attempts = next.gpu.accepted = next.base.base.acceptedSnapshots;
                    next.gpu.queryStage = static_cast<std::uint32_t>(GpuOwnershipQueryStage::Complete);
                    next.gpu.metadataValid = next.gpu.supportPredicate = 1;
                    next.gpu.queryStatus = static_cast<std::uint32_t>(GpuOwnershipQueryStatus::Accepted);
                    next.gpu.deviceNodeCount = 1;
                    next.gpu.canonicalIdentityFlags = 7;
                    next.gpu.swapchainBufferCount = 3;
                    next.gpu.swapchainFormat = next.gpu.backbufferFormat = 87;
                    next.gpu.swapchainWidth = static_cast<std::uint32_t>(next.gpu.backbufferWidth = 640);
                    next.gpu.swapchainHeight = next.gpu.backbufferHeight = 480;
                    next.gpu.backbufferSampleCount = next.gpu.nativeExplicitGPUCount = next.gpu.nativeAFRCount = 1;
                    if (MTA_TEST_RENDER_MODE == 8)
                    {
                        // Controlled unsupported metadata must survive a read
                        // and final cache; the consumer grants no GPU capability.
                        next.gpu.accepted = 0;
                        next.gpu.rejected = next.gpu.attempts;
                        next.gpu.supportPredicate = 0;
                        next.gpu.queryStatus = static_cast<std::uint32_t>(GpuOwnershipQueryStatus::Unsupported);
                        next.gpu.queueType = 2;
                    }
                }
                if (MTA_TEST_RENDER_MODE == 7 && fixtureCounters && fixtureCounters[13] == 2)
                    next.base.requestedRoute = 1;
                output = next;
                return Status::Ok;
            }
            void Release() noexcept override
            {
                if (fixtureCounters)
                {
                    ++fixtureCounters[7];
                    fixtureCounters[9] = ++fixtureCounters[8];
                }
                delete this;
            }

        private:
            RenderFixtureV2 observation{RenderObservationRoute::D3D12};
        };

        class RenderFixtureV4 final : public IGameRenderObservationV4
        {
        public:
            Status Begin() noexcept override { return observation.Begin(); }
            Status End() noexcept override { return observation.End(); }
            Status ReadState(RenderObservationStateV4& output) noexcept override
            {
                RenderObservationStateV4 next;
                const auto               status = observation.ReadState(next.base);
                if (status != Status::Ok)
                    return status;
                if (MTA_TEST_RENDER_MODE != 6)
                {
                    // Classification is separate diagnostic evidence. Even a
                    // known implementation must leave custom GPU support off.
                    next.base.gpu.accepted = next.base.gpu.supportPredicate = 0;
                    next.base.gpu.rejected = next.base.gpu.attempts;
                    next.base.gpu.customPresentPresent = 1;
                    next.base.gpu.queryStatus = static_cast<std::uint32_t>(GpuOwnershipQueryStatus::Unsupported);
                    next.custom = MTA_TEST_RENDER_MODE == 8 ? CustomPresentClassificationState{3, 0, 0, 0} : CustomPresentClassificationState{2, 1, 127, 15};
                }
                output = next;
                return Status::Ok;
            }
            void Release() noexcept override
            {
                if (fixtureCounters)
                {
                    ++fixtureCounters[7];
                    fixtureCounters[9] = ++fixtureCounters[8];
                }
                delete this;
            }

        private:
            RenderFixtureV3 observation;
        };

        class RenderFixtureV5 final : public IGameRenderObservationV5
        {
        public:
            Status Begin() noexcept override { return observation.Begin(); }
            Status End() noexcept override { return observation.End(); }
            Status ReadState(RenderObservationStateV5& output) noexcept override
            {
                RenderObservationStateV5 next;
                const auto               status = observation.ReadState(next.base);
                if (status != Status::Ok)
                    return status;
                if (MTA_TEST_RENDER_MODE != 6)
                {
                    // These controlled CPU facts exercise Core's typed cache;
                    // they cannot change the historical GPU support predicate.
                    next.boundary.status = MTA_TEST_RENDER_MODE == 8 ? 1u : 2u;
                    if (MTA_TEST_RENDER_MODE != 8)
                        next.boundary = {2, 1, 255, 0, 2, 2, 0, 1, 7, 0, 2, 1, 1, 0, 100, 100, 100, 0, 9, 0};
                }
                output = next;
                return Status::Ok;
            }
            void Release() noexcept override
            {
                diagnosticLease = nullptr;
                if (fixtureCounters)
                {
                    ++fixtureCounters[7];
                    fixtureCounters[9] = ++fixtureCounters[8];
                }
                delete this;
            }

        private:
            RenderFixtureV4 observation;
        };
        class RenderFixtureV6 final : public IGameRenderObservationV6
        {
        public:
            Status Begin() noexcept override
            {
                deviceBegan = true;
                if (deviceFixtureStats)
                {
                    ++deviceFixtureStats->begins;
                    deviceFixtureStats->beginOrder = ++deviceFixtureStats->order;
                }
                const auto status = observation.Begin();
                if (v6OperationObserver)
                    v6OperationObserver();
                return status;
            }
            Status End() noexcept override
            {
                const auto status = observation.End();
                ++setupEndCount;
                if (deviceFixtureStats)
                    ++deviceFixtureStats->ends;
                if (status == Status::Ok && !(MTA_TEST_RENDER_MODE == 5 && setupEndCount == 1))
                    deviceEnded = true;
                setupCleanupAttempted = 1;
                setupCleanupComplete = status == Status::Ok && !(MTA_TEST_RENDER_MODE == 5 && setupEndCount == 1) ? 1u : 0u;
                if (v6OperationObserver)
                    v6OperationObserver();
                return status;
            }
            Status ReadState(RenderObservationStateV6& output) noexcept override
            {
                RenderObservationStateV6 next;
                const auto               status = observation.ReadState(next.base);
                if (status != Status::Ok)
                    return status;
                if (MTA_TEST_RENDER_MODE != 6)
                {
                    // These controlled CPU facts exercise Core's typed cache;
                    // they cannot change the historical GPU support predicate.
                    // A coherent native dispatch must remain representable
                    // beside a refused standard-DXGI entry in the V5 prefix.
                    next.base.boundary = {};
                    next.base.boundary.status = 1;
                    next.nativeDispatch.status = MTA_TEST_RENDER_MODE == 8 ? 1u : 2u;
                    if (MTA_TEST_RENDER_MODE != 8)
                        next.nativeDispatch = {2, 2, 255, 0, 2, 2, 0, 1, 7, 0, 2, 1, 1, 0, 100, 100, 100, 0, 9, 0};
                }
                output = next;
                return Status::Ok;
            }
            void Release() noexcept override
            {
                diagnosticLeaseV6 = nullptr;
                if (deviceFixtureStats)
                {
                    ++deviceFixtureStats->releases;
                    --deviceFixtureStats->liveLeases;
                }
                if (fixtureCounters)
                {
                    ++fixtureCounters[7];
                    fixtureCounters[9] = ++fixtureCounters[8];
                }
                delete this;
            }

            Status RequestDevice(bool window = false) noexcept
            {
                if (deviceFixtureStats)
                {
                    ++deviceFixtureStats->requests;
                    deviceFixtureStats->requestOrder = ++deviceFixtureStats->order;
                }
                if (deviceBegan || deviceRequested || deviceEnded)
                    return Status::InvalidArgument;
                if (MTA_TEST_RENDER_MODE == 8 || (deviceFixtureStats && deviceFixtureStats->control == 1))
                    return Status::Unsupported;
                deviceRequested = true;
                deviceWindowRequested = window;
                return Status::Ok;
            }

            Status TakeDevice(RenderDeviceLeaseV1& output, bool window = false) noexcept
            {
                if (!deviceRequested || deviceWindowRequested != window || !deviceBegan || deviceEnded || deviceTaken || MTA_TEST_RENDER_MODE == 2 ||
                    (deviceFixtureStats && deviceFixtureStats->control == 2))
                    return Status::NotInitialized;
                // This token tests transfer policy only. It has no COM vtable,
                // so a passing consumer test cannot claim device or GPU work.
                RenderDeviceLeaseV1 next;
                next.device = reinterpret_cast<ID3D12Device*>(deviceIdentity);
                next.generation = setupGeneration;
                next.acceptedSnapshots = next.dispatchEntries = 7;
                next.sourceProfileId = 2;
                output = next;
                deviceTaken = true;
                if (deviceFixtureStats)
                    ++deviceFixtureStats->transfers;
                return Status::Ok;
            }
            Status TakeDeviceWindow(RenderDeviceWindowLeaseV2& output) noexcept
            {
                // This HWND is an opaque policy token like deviceIdentity. No
                // fixture attaches a child, calls Win32 on it or claims pixels.
                RenderDeviceLeaseV1 device;
                const auto          result = TakeDevice(device, true);
                if (result != Status::Ok)
                    return result;
                RenderDeviceWindowLeaseV2 next;
                next.device = device.device;
                next.window = windowIdentity;
                next.generation = device.generation;
                next.acceptedSnapshots = device.acceptedSnapshots;
                next.dispatchEntries = device.dispatchEntries;
                next.sourceProfileId = device.sourceProfileId;
                next.windowed = 1;
                output = next;
                return Status::Ok;
            }

        private:
            RenderFixtureV5 observation;
            unsigned        setupEndCount = 0;
            bool            deviceRequested = false, deviceWindowRequested = false, deviceBegan = false, deviceEnded = false, deviceTaken = false;
        };
    }

    extern "C" IGameAdapter* CreateGameAdapter(std::uint32_t abiVersion) noexcept
    {
        return abiVersion == AdapterAbiVersion ? new (std::nothrow) ShutdownFailureAdapter : nullptr;
    }

    extern "C" __declspec(dllexport) void SetFacadeFixtureCounters(std::uint32_t* counters) noexcept
    {
        fixtureCounters = counters;
    }

    #if MTA_TEST_CLOCK_MODE
    extern "C" __declspec(dllexport) void SetClockFixtureStats(FixtureClockStats* stats) noexcept
    {
        clockFixtureStats = stats;
    }
    extern "C" __declspec(dllexport) void SetClockFixtureOperationObserver(void (*observer)() noexcept) noexcept
    {
        clockOperationObserver = observer;
    }
    extern "C" Status BeginGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, const ClockServiceRequestV1* request,
                                              ClockServiceStateV1* output) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || !request || !output)
            return Status::InvalidArgument;
        if (clockFixtureStats)
            ++clockFixtureStats->begins;
        fixtureClockState = {};
        fixtureClockState.generation = 17;
        fixtureClockState.producerThread = GetCurrentThreadId();
        fixtureClockState.bound = fixtureClockState.hooked = 1;
        fixtureClockState.active = fixtureClockState.ready = MTA_TEST_CLOCK_MODE == 1 ? 0u : 1u;
        fixtureClockState.stopping = fixtureClockState.retainedPeerThreads = MTA_TEST_CLOCK_MODE == 1 ? 1u : 0u;
        fixtureClockState.failure = MTA_TEST_CLOCK_MODE == 1 ? ClockResult::RestoreFailure : ClockResult::Ok;
        *output = fixtureClockState;
        ObserveClockOperation();
        return MTA_TEST_CLOCK_MODE == 1 ? Status::Unsupported : Status::Ok;
    }
    extern "C" Status SubmitGameClockCommandV1(std::uint32_t version, IGameAdapter* adapter, const ClockCommandV1* command) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || !command)
            return Status::InvalidArgument;
        if (clockFixtureStats)
            ++clockFixtureStats->submits;
        ObserveClockOperation();
        return Status::Unsupported;
    }
    extern "C" Status RenewGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || generation != 17)
            return Status::InvalidArgument;
        if (clockFixtureStats)
            ++clockFixtureStats->renewals;
        ObserveClockOperation();
        return Status::Unsupported;
    }
    extern "C" Status ReadGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, ClockServiceStateV1* output) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || !output)
            return Status::InvalidArgument;
        if (clockFixtureStats)
            ++clockFixtureStats->reads;
        *output = fixtureClockState;
        ObserveClockOperation();
        return Status::Ok;
    }
        #if MTA_TEST_CLOCK_MODE != 4
    extern "C" Status EndGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || generation != 17)
            return Status::InvalidArgument;
        if (clockFixtureStats)
            ++clockFixtureStats->ends;
        const bool clean = clockFixtureStats && (clockFixtureStats->control & 1);
        fixtureClockState.active = fixtureClockState.ready = 0;
        fixtureClockState.stopping = 1;
        fixtureClockState.hooked = fixtureClockState.retainedPeerThreads = clean ? 0u : 1u;
        fixtureClockState.restored = clean ? 1u : 0u;
        ObserveClockOperation();
        return clean ? Status::Ok : Status::Unsupported;
    }
        #endif
    #endif

    #if MTA_TEST_LOCAL_INPUT_MODE
    extern "C" __declspec(dllexport) void SetLocalInputFixtureStats(FixtureLocalInputStats* stats) noexcept
    {
        localFixtureStats = stats;
    }
    extern "C" Status BeginGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, const LocalInputLeaseRequestV1* request,
                                                 LocalInputLeaseStateV1* output) noexcept
    {
        if (version != 1 || !adapter || !request || !output)
            return Status::InvalidArgument;
        if (localFixtureStats)
        {
            ++localFixtureStats->begins;
            const MtaInputState neutral;
            localFixtureStats->neutralBegin = std::memcmp(&neutral, &request->neutral, sizeof(neutral)) == 0;
        }
        if (MTA_TEST_LOCAL_INPUT_MODE == 4)
        {
            fixtureLocalUnknownDebt = true;
            return Status::Unsupported;
        }
        fixtureLocalState = {};
        fixtureLocalState.generation = 31;
        fixtureLocalState.producerThread = GetCurrentThreadId();
        fixtureLocalState.active = fixtureLocalState.bound = 1;
        *output = fixtureLocalState;
        return MTA_TEST_LOCAL_INPUT_MODE == 3 ? Status::Unsupported : Status::Ok;
    }
    extern "C" Status RenewGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation, const MtaInputState* movement) noexcept
    {
        if (version != 1 || !adapter || generation != 31 || !movement)
            return Status::InvalidArgument;
        if (localFixtureStats)
        {
            ++localFixtureStats->renewals;
            localFixtureStats->lastRenewed = *movement;
        }
        ++fixtureLocalState.renewals;
        return Status::Ok;
    }
    extern "C" Status ReadGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, LocalInputLeaseStateV1* output) noexcept
    {
        if (version != 1 || !adapter || !output)
            return Status::InvalidArgument;
        if (localFixtureStats)
            ++localFixtureStats->reads;
        if (!fixtureLocalState.generation)
            return Status::Unsupported;
        *output = fixtureLocalState;
        return Status::Ok;
    }
    extern "C" Status EndGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (version != 1 || !adapter || generation != 31)
            return Status::InvalidArgument;
        if (localFixtureStats)
            ++localFixtureStats->ends;
        if (!localFixtureStats || !localFixtureStats->allowCleanup)
            return Status::Unsupported;
        fixtureLocalState.active = fixtureLocalState.bound = 0;
        fixtureLocalState.revoked = 1;
        fixtureLocalState.reason = static_cast<std::uint32_t>(LocalInputLeaseReason::Stopped);
        return Status::Ok;
    }
    #endif

    extern "C" IGameRenderObservation* CreateGameRenderObservation(std::uint32_t apiVersion, CCoreInterface* owner) noexcept
    {
        return MTA_TEST_RENDER_MODE && apiVersion == RenderObservationApiVersion && owner ? new (std::nothrow) RenderFixture : nullptr;
    }

    #if MTA_TEST_RENDER_V2
    extern "C" IGameRenderObservationV2* CreateGameRenderObservationV2(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        return apiVersion == RenderObservationApiVersionV2 && owner && (route == RenderObservationRoute::D3D11 || route == RenderObservationRoute::D3D12)
                   ? new (std::nothrow) RenderFixtureV2(route)
                   : nullptr;
    }
    #endif
    #if MTA_TEST_RENDER_V3
    extern "C" IGameRenderObservationV3* CreateGameRenderObservationV3(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        return apiVersion == RenderObservationApiVersionV3 && owner && route == RenderObservationRoute::D3D12 ? new (std::nothrow) RenderFixtureV3 : nullptr;
    }
    #endif
    #if MTA_TEST_RENDER_V4
    extern "C" IGameRenderObservationV4* CreateGameRenderObservationV4(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        return apiVersion == RenderObservationApiVersionV4 && owner && route == RenderObservationRoute::D3D12 ? new (std::nothrow) RenderFixtureV4 : nullptr;
    }
    #endif

    #if MTA_TEST_RENDER_V5
    extern "C" IGameRenderObservationV5* CreateGameRenderObservationV5(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        if (apiVersion != RenderObservationApiVersionV5 || !owner || route != RenderObservationRoute::D3D12 || (fixtureCounters && fixtureCounters[13] == 3))
            return nullptr;
        diagnosticLease = new (std::nothrow) RenderFixtureV5;
        diagnosticOwnerThread = GetCurrentThreadId();
        return diagnosticLease;
    }
        // Mode6 is an actual historical provider with no optional diagnostic export.
        // Mode8 deliberately returns a different diagnostic version, not admission.
        #if MTA_TEST_RENDER_MODE != 6
    extern "C" Status ReadGameRenderEndpointDiagnosticV1(std::uint32_t version, IGameRenderObservationV5* lease, RenderEndpointDiagnosticV1* output) noexcept
    {
        if (version != 1 || !output || !lease || lease != diagnosticLease)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != diagnosticOwnerThread)
            return Status::WrongThread;
        ++diagnosticReads;
        RenderEndpointDiagnosticV1 next;
        next.version = MTA_TEST_RENDER_MODE == 8 ? 99u : 1u;
        next.generationLow = 17;
        next.acceptedSnapshotsLow = 7;
        next.preparationGate = 13;
        next.preparationReason = 3;
        next.loadedEntryMatch = 2;
        next.slotStatus = 1;
        next.slotOwnerClass = 3;
        next.slotRva = 0x1234;
        *output = next;
        return Status::Ok;
    }
        #endif
    extern "C" __declspec(dllexport) std::uint32_t GetFixtureDiagnosticReads() noexcept
    {
        return diagnosticReads;
    }
    #endif

    #if MTA_TEST_RENDER_V6
    extern "C" __declspec(dllexport) void SetFixtureDeviceStats(FixtureDeviceStats* stats) noexcept
    {
        deviceFixtureStats = stats;
    }
    extern "C" __declspec(dllexport) void SetFixtureV6OperationObserver(void (*observer)() noexcept) noexcept
    {
        v6OperationObserver = observer;
    }
    extern "C" IGameRenderObservationV6* CreateGameRenderObservationV6(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        if (apiVersion != RenderObservationApiVersionV6 || !owner || route != RenderObservationRoute::D3D12 || (fixtureCounters && fixtureCounters[13] == 3))
            return nullptr;
        diagnosticLeaseV6 = new (std::nothrow) RenderFixtureV6;
        if (diagnosticLeaseV6)
        {
            if (deviceFixtureStats)
            {
                ++deviceFixtureStats->allocations;
                ++deviceFixtureStats->liveLeases;
            }
            ++setupGeneration;
            setupCleanupAttempted = setupCleanupComplete = 0;
        }
        diagnosticOwnerThread = GetCurrentThreadId();
        return diagnosticLeaseV6;
    }
        #if MTA_TEST_RENDER_MODE != 6
    extern "C" Status RequestGameRenderDeviceLeaseV1(std::uint32_t version, IGameRenderObservationV6* lease) noexcept
    {
        if (version != RenderDeviceLeaseVersionV1 || !lease || lease != diagnosticLeaseV6)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != diagnosticOwnerThread)
            return Status::WrongThread;
        return static_cast<RenderFixtureV6*>(lease)->RequestDevice();
    }
    extern "C" Status TakeGameRenderDeviceLeaseV1(std::uint32_t version, IGameRenderObservationV6* lease, RenderDeviceLeaseV1* output) noexcept
    {
        if (deviceFixtureStats)
            ++deviceFixtureStats->takes;
        if (version != RenderDeviceLeaseVersionV1 || !lease || lease != diagnosticLeaseV6 || !output)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != diagnosticOwnerThread)
            return Status::WrongThread;
        return static_cast<RenderFixtureV6*>(lease)->TakeDevice(*output);
    }
    extern "C" Status RequestGameRenderDeviceWindowLeaseV2(std::uint32_t version, IGameRenderObservationV6* lease) noexcept
    {
        if (version != RenderDeviceWindowLeaseVersionV2 || !lease || lease != diagnosticLeaseV6)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != diagnosticOwnerThread)
            return Status::WrongThread;
        return static_cast<RenderFixtureV6*>(lease)->RequestDevice(true);
    }
            #if !defined(MTA_TEST_DEVICE_WINDOW_MISSING_TAKE)
    extern "C" Status TakeGameRenderDeviceWindowLeaseV2(std::uint32_t version, IGameRenderObservationV6* lease, RenderDeviceWindowLeaseV2* output) noexcept
    {
        if (deviceFixtureStats)
            ++deviceFixtureStats->takes;
        if (version != RenderDeviceWindowLeaseVersionV2 || !lease || lease != diagnosticLeaseV6 || !output)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != diagnosticOwnerThread)
            return Status::WrongThread;
        return static_cast<RenderFixtureV6*>(lease)->TakeDeviceWindow(*output);
    }
            #endif
        #endif
        // Mode6 is an actual historical provider with no optional diagnostic export.
        // Mode8 deliberately returns a different diagnostic version, not admission.
        #if MTA_TEST_RENDER_MODE != 6
    extern "C" Status ReadGameRenderEndpointDiagnosticV2(std::uint32_t version, IGameRenderObservationV6* lease, RenderEndpointDiagnosticV1* output) noexcept
    {
        if (version != 1 || !output || !lease || lease != diagnosticLeaseV6)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != diagnosticOwnerThread)
            return Status::WrongThread;
        ++diagnosticReads;
        RenderEndpointDiagnosticV1 next;
        next.version = MTA_TEST_RENDER_MODE == 8 ? 99u : 1u;
        next.generationLow = 17;
        next.acceptedSnapshotsLow = 7;
        next.preparationGate = 13;
        next.preparationReason = 3;
        next.loadedEntryMatch = 2;
        next.slotStatus = 1;
        next.slotOwnerClass = 3;
        next.slotRva = 0x1234;
        *output = next;
        return Status::Ok;
    }
        #endif
        #if MTA_TEST_RENDER_MODE != 6
    extern "C" Status ReadGameRenderSetupDiagnosticV1(std::uint32_t version, IGameRenderObservationV6* lease, RenderSetupDiagnosticV1* output) noexcept
    {
        // Count every entry, including an invalid released lease, so the host
        // can prove its final value cache makes no provider calls after Release.
        ++setupDiagnosticReads;
        if (version != RenderSetupDiagnosticVersionV1 || !lease || lease != diagnosticLeaseV6 || !output)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != diagnosticOwnerThread)
            return Status::WrongThread;
        // A retained provider can deny fresh setup evidence without permitting
        // Core to reuse the previously copied generation as a current report.
        if (fixtureCounters && fixtureCounters[13] == 4)
            return Status::Unsupported;
        RenderSetupDiagnosticV1 next;
        next.version = MTA_TEST_RENDER_MODE == 8 ? 99u : 1u;
        next.generationLow = setupGeneration;
        next.attempted = 1;
        next.failureStage = MTA_TEST_RENDER_MODE == 2 ? static_cast<std::uint32_t>(RenderSetupStage::NativeThreadStubMatch) : 0u;
        next.stage = next.failureStage ? next.failureStage : static_cast<std::uint32_t>(RenderSetupStage::Complete);
        next.initialThreads = MTA_TEST_RENDER_MODE == 2 ? 0u : 3u;
        next.heldThreads = MTA_TEST_RENDER_MODE == 2 ? 0u : 3u;
        next.stablePasses = MTA_TEST_RENDER_MODE == 2 ? 0u : 2u;
        next.cleanupAttempted = setupCleanupAttempted;
        next.cleanupComplete = setupCleanupComplete;
        next.transactionCommitted = next.hooksCommitted = next.installSucceeded = MTA_TEST_RENDER_MODE == 2 ? 0u : 1u;
        *output = next;
        return Status::Ok;
    }
        #endif
        #if MTA_TEST_RENDER_MODE != 6
    extern "C" Status ReadGameRenderRootsDiagnosticV1(std::uint32_t version, IGameRenderObservationV6* lease, RenderRootsDiagnosticV1* output) noexcept
    {
        ++rootsDiagnosticReads;
        if (version != RenderRootsDiagnosticVersionV1 || !lease || lease != diagnosticLeaseV6 || !output)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != diagnosticOwnerThread)
            return Status::WrongThread;
        // Model a missed current-generation publication/final read. Core must
        // preserve caller output and clear its final cache before Release.
        if (MTA_TEST_RENDER_MODE == 2 || (fixtureCounters && fixtureCounters[13] == 4))
            return Status::Unsupported;
        RenderRootsDiagnosticV1 next;
        next.version = MTA_TEST_RENDER_MODE == 8 ? 99u : 1u;
        next.generationLow = setupGeneration;
        next.acceptedSnapshotsLow = next.dispatchEntriesLow = 7;
        next.queryPhase = 1;
        next.checkpoint = 26;
        next.refusalReason = 3;
        next.scalarValidMask = 3;
        next.contextNodeMask = 1;
        next.nativeBufferIndex = 2;
        *output = next;
        return Status::Ok;
    }
        #endif
    extern "C" __declspec(dllexport) std::uint32_t GetFixtureRootsDiagnosticReadsV6() noexcept
    {
        return rootsDiagnosticReads;
    }
    extern "C" __declspec(dllexport) std::uint32_t GetFixtureSetupDiagnosticReadsV6() noexcept
    {
        return setupDiagnosticReads;
    }
    extern "C" __declspec(dllexport) std::uint32_t GetFixtureDiagnosticReadsV6() noexcept
    {
        return diagnosticReads;
    }
    #endif

}
    #if MTA_TEST_RENDER_V6 && MTA_TEST_RENDER_MODE == 8
// A distinct disposable provider holds the real loader lock only when the host
// creates both named gates. Its timeout makes a lazy-lookup regression fail
// cleanly instead of hanging the suite or adding a production admission bypass.
extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        wchar_t enteredName[96]{}, resumeName[96]{}, timeoutName[96]{};
        swprintf_s(enteredName, L"Local\\MtaApi6DiagnosticLoaderEntered-%lu", GetCurrentProcessId());
        swprintf_s(resumeName, L"Local\\MtaApi6DiagnosticLoaderResume-%lu", GetCurrentProcessId());
        swprintf_s(timeoutName, L"Local\\MtaApi6DiagnosticLoaderTimeout-%lu", GetCurrentProcessId());
        const auto entered = OpenEventW(EVENT_MODIFY_STATE, FALSE, enteredName);
        const auto resume = OpenEventW(SYNCHRONIZE, FALSE, resumeName);
        const auto timeout = OpenEventW(EVENT_MODIFY_STATE, FALSE, timeoutName);
        if (entered && resume && timeout)
        {
            SetEvent(entered);
            if (WaitForSingleObject(resume, 3000) != WAIT_OBJECT_0)
                SetEvent(timeout);
        }
        if (entered)
            CloseHandle(entered);
        if (resume)
            CloseHandle(resume);
        if (timeout)
            CloseHandle(timeout);
    }
    return TRUE;
}
    #endif
#else
namespace
{
    int checks = 0;

    void Check(bool condition, const std::string& message)
    {
        ++checks;
        // Core's early capability gate must remain enforced in optimized builds.
        if (!condition)
            throw std::runtime_error(message);
    }

    #if defined(_WIN64)
    void TestRejectedArguments(CCoreInterface* owner, const std::wstring& modulePath)
    {
        CGameAdapterModule host;
        Check(!host.Start(nullptr, modulePath), "a missing MTA owner cannot initialize a module");
        Check(!host.IsStarted(), "missing owner leaves no module instance");
        Check(!host.Start(owner, L"game_de.dll"), "a relative library path cannot use the DLL search path");
        Check(!host.IsStarted(), "relative path leaves no module instance");
        Check(!host.Start(owner, modulePath + std::wstring(1, L'\0') + L"ignored"), "embedded NUL cannot select an unintended module path");
        Check(!host.Start(owner, modulePath + L".missing"), "an absent explicit module cannot fall back to game_sa");
        Check(!host.GetError().empty(), "failed module load retains a precise error");
        wchar_t systemDirectory[MAX_PATH]{};
        Check(GetSystemDirectoryW(systemDirectory, MAX_PATH) != 0, "system fixture directory is available");
        const auto missingFactory = (std::filesystem::path(systemDirectory) / L"kernel32.dll").wstring();
        Check(!host.Start(owner, missingFactory), "a real DLL without the adapter factory cannot initialize");
        Check(host.GetError().find("CreateGameAdapter") != std::string::npos && !host.IsStarted(), "missing-factory rejection reports its ABI boundary");
        Check(!host.Pulse() && !host.Reset(), "an uninitialized consumer cannot invoke adapter callbacks");
        mta::game_de::MtaInputState input;
        input.current.ButtonCross = 19;
        Check(!host.SubmitControlActions({}) && !host.GetMtaInputState(input) && input.current.ButtonCross == 19,
              "an uninitialized consumer cannot submit or replace logical input");
        mta::game_de::NativeMovementTrialState trial;
        trial.callbackCount = 19;
        Check(!host.BeginNativeMovementTrial() && !host.GetNativeMovementTrialState(trial) && !host.EndNativeMovementTrial() && trial.callbackCount == 19,
              "uninitialized Core cannot start, inspect or end a movement trial");
        Check(host.GetCapabilities() == 0 && !host.IsGameplayReady(), "an uninitialized consumer has no capabilities");
        CGame* unavailableGame = reinterpret_cast<CGame*>(std::uintptr_t{1});
        Check(!host.BorrowGameInterface(unavailableGame) && unavailableGame == reinterpret_cast<CGame*>(std::uintptr_t{1}),
              "an uninitialized consumer preserves the caller's game facade output");
        mta::game_de::RenderObservationState render;
        render.width = 17;
        Check(!host.BeginNativeRenderObservation() && !host.GetNativeRenderObservationState(render) && !host.EndNativeRenderObservation() && render.width == 17,
              "uninitialized optional renderer calls preserve output and never enter a provider");
        mta::game_de::RenderObservationStateV2 renderV2;
        renderV2.requestedRoute = 99;
        Check(!host.BeginNativeRenderObservation(mta::game_de::RenderObservationRoute::D3D12) && !host.GetNativeRenderObservationState(renderV2) &&
                  renderV2.requestedRoute == 99,
              "uninitialized route-aware renderer calls preserve output");
        mta::game_de::RenderObservationStateV3 renderV3;
        renderV3.gpu.queryStage = 99;
        Check(!host.BeginNativeRenderOwnershipObservation(mta::game_de::RenderObservationRoute::D3D12) && !host.GetNativeRenderObservationState(renderV3) &&
                  renderV3.gpu.queryStage == 99,
              "uninitialized API3 preserves the separate ownership output");
        host.Stop();
        host.Stop();
        Check(!host.IsStarted(), "cleanup is idempotent after failure");
    }

    void TestActualModule(CCoreInterface* owner, const std::wstring& modulePath)
    {
        Check(GetModuleHandleW(modulePath.c_str()) == nullptr, "test begins without a preloaded Game DE module");
        {
            CGameAdapterModule host;
            Check(host.Start(owner, modulePath), "Core consumer initializes the actual Game DE module: " + host.GetError());
            Check(host.IsStarted(), "successful factory initialization establishes ownership");
            Check(GetModuleHandleW(modulePath.c_str()) != nullptr, "owned module remains loaded while its instance exists");
            Check(host.GetCapabilities() == (static_cast<std::uint64_t>(mta::game_de::AdapterCapability::ObservationIngest) |
                                             static_cast<std::uint64_t>(mta::game_de::AdapterCapability::MtaInputSynthesis)),
                  "module declares only observation ingestion and MTA action synthesis");
            Check(!host.IsGameplayReady(), "observation ingestion cannot satisfy full gameplay readiness");
            Check(host.GameplayUnavailableReason().find("GameInterface") != std::string::npos, "full-game gate explains missing game interface");
            Check(host.GameplayUnavailableReason().find("EngineInputWrite") != std::string::npos, "full-game gate explains missing input write");
            Check(host.GameplayUnavailableReason().find("RemotePlayerContext") != std::string::npos, "full-game gate explains missing remote context");
            Check(host.Pulse(), "owning Core can pulse the initialized real module");
            mta::game_de::ClockServiceRequestV1 clockRequest;
            mta::game_de::ClockServiceStateV1   clockState;
            clockState.hour = 99;
            Check(!host.BeginNativeClockService(clockRequest, clockState), "Core clock consumer refuses this non-game executable");
            Check(!clockState.bound && !clockState.hooked && !clockState.patchInstalled && !clockState.modulePinned,
                  "Core receives failed clock admission without native patch or hook ownership");
            Check(!host.IsGameplayReady(), "an optional clock contract does not remove the complete gameplay gate");
            bool        otherClockRead = true;
            std::thread clockThread([&] { otherClockRead = host.ReadNativeClockService(clockState); });
            clockThread.join();
            Check(!otherClockRead, "a foreign Core consumer thread cannot read or adopt the native clock owner");
            Check(host.Reset(), "explicit reset retires the refused clock attempt before unrelated native service admission");
            for (const auto route : {mta::game_de::RenderObservationRoute::D3D11, mta::game_de::RenderObservationRoute::D3D12})
            {
                Check(!host.BeginNativeRenderObservation(route), "actual optional route binding rejects this non-game host");
                mta::game_de::RenderObservationStateV2 render;
                Check(host.GetNativeRenderObservationState(render) && render.requestedRoute == static_cast<std::uint32_t>(route) && !render.observedRoute &&
                          !render.presenceKnownMask && !render.queuePresent && render.base.failureReason && !render.base.bound && !render.base.active &&
                          !render.base.modulePinned && !render.base.nativePresentCallbacks,
                      "actual version2 export reports explicit failed admission without a fallback or inferred RHI");
                Check(host.EndNativeRenderObservation() && host.GetNativeRenderObservationState(render) && !render.base.active && !render.base.pendingCleanup &&
                          !render.observedRoute,
                      "actual route lease cleanup caches ended values before release");
            }
            Check(!host.BeginNativeRenderOwnershipObservation(mta::game_de::RenderObservationRoute::D3D12), "actual API3 keeps wrong-image admission closed");
            mta::game_de::RenderObservationStateV3 gpu;
            Check(host.GetNativeRenderObservationState(gpu) && gpu.base.requestedRoute == 2 && !gpu.base.observedRoute && gpu.base.base.failureReason &&
                      !gpu.base.base.bound && !gpu.base.base.active && !gpu.base.base.modulePinned && !gpu.gpu.attempts && !gpu.gpu.metadataValid,
                  "actual API3 factory reports failed admission without native references or inferred GPU ownership");
            Check(host.EndNativeRenderObservation() && host.GetNativeRenderObservationState(gpu) && !gpu.gpu.attempts,
                  "actual API3 final cache preserves no-query wrong-image evidence");
            Check(!host.BeginNativeRenderFrameBoundaryObservation(mta::game_de::RenderObservationRoute::D3D12),
                  "actual API5 rejects the nongame image before installing any system or native hook");
            mta::game_de::RenderObservationStateV5 boundary;
            Check(host.GetNativeRenderObservationState(boundary) && boundary.base.base.base.requestedRoute == 2 && !boundary.base.base.base.observedRoute &&
                      boundary.base.base.base.base.failureReason && !boundary.base.base.gpu.attempts && !boundary.boundary.presentEntries &&
                      !boundary.boundary.factsMask && !boundary.boundary.status,
                  "actual API5 factory preserves wrong-image failure without inventing endpoint or buffer facts");
            Check(host.EndNativeRenderObservation() && host.GetNativeRenderObservationState(boundary) && !boundary.base.base.base.base.active &&
                      !boundary.base.base.base.base.pendingCleanup && !boundary.boundary.presentEntries,
                  "actual API5 caches completed zero-query admission failure before releasing its typed lease");
            mta::game_de::RenderObservationStateV6 dispatch;
            Check(!host.BeginNativeRenderDispatchObservation(mta::game_de::RenderObservationRoute::D3D12) && host.GetNativeRenderObservationState(dispatch) &&
                      dispatch.base.base.base.base.requestedRoute == 2 && !dispatch.base.base.base.base.observedRoute &&
                      dispatch.base.base.base.base.base.failureReason && !dispatch.base.base.base.gpu.attempts && !dispatch.base.boundary.status &&
                      !dispatch.base.boundary.presentEntries && !dispatch.nativeDispatch.status && !dispatch.nativeDispatch.presentEntries,
                  "actual API6 rejects the nongame image before callsite changes and preserves both zero observation tails");
            boundary.boundary.status = 99;
            Check(!host.GetNativeRenderObservationState(boundary) && boundary.boundary.status == 99,
                  "actual retained API6 cannot replace a historical API5 caller layout");
            mta::game_de::RenderEndpointDiagnosticV1 endpointDiagnostic;
            endpointDiagnostic.version = 99;
            Check(!host.GetNativeRenderEndpointDiagnostic(endpointDiagnostic) && endpointDiagnostic.version == 99,
                  "actual wrong-image API6 exposes no diagnostic through its optional V2 export");
            mta::game_de::RenderSetupDiagnosticV1 setupDiagnostic;
            setupDiagnostic.version = 99;
            Check(!host.GetNativeRenderSetupDiagnostic(setupDiagnostic) && setupDiagnostic.version == 99,
                  "actual wrong-image API6 never admits a setup diagnostic cache");
            mta::game_de::RenderRootsDiagnosticV1 rootsDiagnostic;
            rootsDiagnostic.version = 99;
            Check(!host.GetNativeRenderRootsDiagnostic(rootsDiagnostic) && rootsDiagnostic.version == 99,
                  "actual wrong-image API6 cannot publish roots diagnostics before a callback");
            Check(host.EndNativeRenderObservation() && host.GetNativeRenderObservationState(dispatch) && !dispatch.base.base.base.base.base.active &&
                      !dispatch.base.base.base.base.base.pendingCleanup && !dispatch.nativeDispatch.presentEntries && !dispatch.base.boundary.presentEntries,
                  "actual API6 caches safe zero-query cleanup before releasing its larger lease");
            Check(GetProcAddress(GetModuleHandleW(modulePath.c_str()), "RequestGameRenderDeviceLeaseV1") &&
                      GetProcAddress(GetModuleHandleW(modulePath.c_str()), "TakeGameRenderDeviceLeaseV1"),
                  "actual module exposes the separately versioned explicit device handoff");
            mta::game_de::RenderDeviceLeaseV1 deviceLease;
            deviceLease.generation = 0xfedcba9876543210ull;
            Check(!host.BeginNativeRenderDeviceObservation() && host.GetNativeRenderObservationState(dispatch) &&
                      dispatch.base.base.base.base.base.failureReason && !dispatch.base.base.base.base.base.bound && !dispatch.nativeDispatch.presentEntries &&
                      !host.TakeNativeRenderDeviceLease(deviceLease) && !deviceLease.device && deviceLease.generation == 0xfedcba9876543210ull,
                  "actual requested device observation still refuses a non-game host without inventing device ownership");
            Check(host.EndNativeRenderObservation(), "actual failed device admission safely releases its fresh V6 lease");
            Check(GetProcAddress(GetModuleHandleW(modulePath.c_str()), "RequestGameRenderDeviceWindowLeaseV2") &&
                      GetProcAddress(GetModuleHandleW(modulePath.c_str()), "TakeGameRenderDeviceWindowLeaseV2"),
                  "actual module exposes the separately versioned optional device-window handoff");
            mta::game_de::RenderDeviceWindowLeaseV2 deviceWindowLease;
            deviceWindowLease.generation = 0xfedcba9876543210ull;
            Check(!host.BeginNativeRenderDeviceWindowObservation() && host.GetNativeRenderObservationState(dispatch) &&
                      dispatch.base.base.base.base.base.failureReason && !dispatch.base.base.base.base.base.bound && !dispatch.nativeDispatch.presentEntries &&
                      !host.TakeNativeRenderDeviceWindowLease(deviceWindowLease) && !deviceWindowLease.device && !deviceWindowLease.window &&
                      deviceWindowLease.generation == 0xfedcba9876543210ull,
                  "actual V2 admission rejects the owned non-game host without inventing a device or HWND");
            Check(host.EndNativeRenderObservation(), "actual failed device-window request safely disposes its retained fresh V6 lease");
            Check(!host.BeginNativeMovementTrial(), "movement trial requires an explicit synthesized frame");
            mta::game_de::LocalObservation observation;
            observation.sampleAtMs = 1000;
            observation.localPedHandle = 7;
            observation.transform.position = {1.0f, -2.0f, 3.0f};
            observation.rawVelocity = {0.25f, -0.5f, 0.0f};
            Check(host.PublishLocalObservation(observation), "Core publishes explicit caller evidence to its real adapter");
            mta::game_de::LocalObservation readback;
            Check(host.GetLocalObservation(readback), "Core reads evidence from its owned module");
            Check(readback.sampleAtMs == 1000 && readback.localPedHandle == 7 && readback.transform.position.y == -2.0f && readback.rawVelocity.y == -0.5f &&
                      readback.controllerKnownMask == 0,
                  "raw velocity and movement remain observations with unknown controller coverage");
            observation.sampleAtMs = 1100;
            observation.controllerKnownMask = mta::game_de::AdapterControllerMask;
            observation.controller.LeftStickY = -128;
            observation.controller.ButtonCross = 255;
            Check(host.PublishLocalObservation(observation) && host.GetLocalObservation(readback),
                  "explicit typed controller evidence can follow raw observations");
            Check(readback.controller.LeftStickY == -128 && readback.controller.ButtonCross == 255 &&
                      readback.controllerKnownMask == mta::game_de::AdapterControllerMask,
                  "real module retains the canonical typed controller evidence");
            mta::game_de::ControlActionFrame frame;
            frame.heldActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(WALK);
            mta::game_de::MtaInputState input;
            Check(host.SubmitControlActions(frame) && host.GetMtaInputState(input), "Core sends configured MTA actions through the actual module ABI");
            Check(input.current.LeftStickY == -128 && input.current.m_bPedWalk == 255 && input.current.ButtonCross == 0 && input.previous.LeftStickY == 0 &&
                      input.previous.m_bPedWalk == 0,
                  "MTA walk output includes its logical walk field without claiming native coverage");
            frame.heldActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(SPRINT);
            Check(host.SubmitControlActions(frame) && host.GetMtaInputState(input), "submit the next MTA sprint frame");
            Check(input.current.ButtonCross == 255 && input.current.m_bPedWalk == 0 && input.previous.m_bPedWalk == 255,
                  "walk and sprint are distinct actions with previous-frame history");
            mta::game_de::NativeMovementTrialState trial;
            Check(!host.BeginNativeMovementTrial() && !host.BeginNativeMovementTrial(), "nongame trial binding fails and can be retried explicitly");
            Check(host.GetNativeMovementTrialState(trial) && !trial.bound && !trial.active && trial.callbackCount == 0 && trial.scopedSteps == 0,
                  "nongame host executes no writer hooks or scoped native steps");
            Check(host.EndNativeMovementTrial() && host.EndNativeMovementTrial(), "Core trial cleanup is idempotent without hooks");
            Check(host.GetLocalObservation(readback) && readback.sampleAtMs == 1100 && readback.controller.ButtonCross == 255 &&
                      readback.controllerKnownMask == mta::game_de::AdapterControllerMask && !host.IsGameplayReady(),
                  "logical synthesis neither overwrites native evidence nor opens the gameplay gate");
            mta::game_de::NativeControllerState native;
            native.current.ButtonCross = 23;
            mta::game_de::NativeDeviceState device;
            device.sources.fill(-1234567);
            const auto                              deviceOriginal = device.sources;
            mta::game_de::NativeMovementEligibility eligibility;
            eligibility.knownMask = 123;
            Check(!host.GetNativeMovementEligibility(eligibility) && eligibility.knownMask == 123, "unbound Core eligibility preserves unknown caller context");
            Check(!host.ReadNativeDeviceState(device) && device.sources == deviceOriginal,
                  "Core cannot read unbound raw device sources or replace caller output");
            Check(!host.ReadNativeControllerState(0, native) && native.current.ButtonCross == 23,
                  "Core cannot read native input without explicit game binding");
            Check(!host.BindNativeInput(), "the real module rejects native binding in this non-game host");
            Check(!host.GetNativeMovementEligibility(eligibility) && eligibility.knownMask == 123,
                  "wrong-image binding cannot authorize Core movement eligibility");
            Check(!host.ReadNativeDeviceState(device) && device.sources == deviceOriginal &&
                      (host.GetCapabilities() & static_cast<std::uint64_t>(mta::game_de::AdapterCapability::NativeDeviceStateCandidateRead)) == 0,
                  "failed binding cannot claim a captured device table");
            Check((host.GetCapabilities() & static_cast<std::uint64_t>(mta::game_de::AdapterCapability::EngineInputRead)) == 0 && !host.IsGameplayReady(),
                  "a rejected native binding leaves engine/gameplay capabilities unavailable");
            Check(host.GetMtaInputState(input) && input.current.ButtonCross == 255 && host.GetLocalObservation(readback) && readback.sampleAtMs == 1100,
                  "failed native binding preserves MTA logical input and caller observations");
            bool foreignPulse = true;
            bool foreignReset = true;
            bool foreignSubmit = true;
            bool foreignRead = true;
            bool foreignBind = true;
            bool foreignNativeRead = true;
            bool foreignDeviceRead = true;
            bool foreignBeginTrial = true;
            bool foreignReadTrial = true;
            bool foreignEndTrial = true;
            bool foreignEligibility = true;
            trial.callbackCount = 123;
            mta::game_de::MtaInputState foreignInput;
            foreignInput.current.ButtonCross = 19;
            std::uint64_t foreignCapabilities = 99;
            std::thread   foreign(
                [&]()
                {
                    foreignPulse = host.Pulse();
                    foreignReset = host.Reset();
                    foreignCapabilities = host.GetCapabilities();
                    foreignSubmit = host.SubmitControlActions({});
                    foreignRead = host.GetMtaInputState(foreignInput);
                    foreignBind = host.BindNativeInput();
                    foreignNativeRead = host.ReadNativeControllerState(0, native);
                    foreignDeviceRead = host.ReadNativeDeviceState(device);
                    foreignBeginTrial = host.BeginNativeMovementTrial();
                    foreignReadTrial = host.GetNativeMovementTrialState(trial);
                    foreignEndTrial = host.EndNativeMovementTrial();
                    foreignEligibility = host.GetNativeMovementEligibility(eligibility);
                    host.Stop();
                });
            foreign.join();
            Check(!foreignPulse && !foreignReset && foreignCapabilities == 0, "foreign thread cannot invoke owned callbacks or claim capabilities");
            Check(!foreignSubmit && !foreignRead && foreignInput.current.ButtonCross == 19,
                  "logical input shares Core thread ownership and preserves failed reader output");
            Check(!foreignBind && !foreignNativeRead && native.current.ButtonCross == 23, "native binding and reads retain the owning Core thread boundary");
            Check(!foreignDeviceRead && device.sources == deviceOriginal, "foreign Core device reads preserve the complete caller state");
            Check(!foreignBeginTrial && !foreignReadTrial && !foreignEndTrial && trial.callbackCount == 123,
                  "foreign Core cannot mutate trial lifetime or replace metrics");
            Check(!foreignEligibility && eligibility.knownMask == 123, "foreign Core cannot read or replace native eligibility");
            Check(host.IsStarted() && GetModuleHandleW(modulePath.c_str()) != nullptr, "foreign teardown cannot unload code owned by Core");
            Check(host.GetLocalObservation(readback) && readback.sampleAtMs == 1100, "rejected foreign reset preserves caller evidence");
            Check(host.Reset(), "owning Core can reset the initialized real module");
            Check(!host.ReadNativeDeviceState(device) && device.sources == deviceOriginal, "Core reset clears device authority without publishing state");
            readback.sampleAtMs = 99999;
            readback.controller.LeftStickY = 123;
            Check(!host.GetLocalObservation(readback), "reset clears caller observation coverage in the real module");
            Check(readback.sampleAtMs == 99999 && readback.controller.LeftStickY == 123, "failed readback preserves the caller's existing output");
            input.current.ButtonCross = 19;
            Check(!host.GetMtaInputState(input) && input.current.ButtonCross == 19, "reset clears logical input as well as observations");
            Check(!host.Start(owner, modulePath), "a second start cannot replace a live owned instance");
            Check(host.IsStarted(), "a rejected duplicate start retains the original owned instance");
            Check(host.Pulse(), "the original instance remains usable after rejected duplicate start");
            host.Stop();
            Check(!host.IsStarted() && host.GetCapabilities() == 0, "stop removes ownership and capabilities");
            Check(GetModuleHandleW(modulePath.c_str()) == nullptr, "Shutdown and Release finish before module unload");
            Check(!host.Pulse() && !host.Reset(), "no adapter callback is possible after unload");
            Check(!host.ReadNativeDeviceState(device) && device.sources == deviceOriginal, "unloaded Core consumer cannot publish device sources");
            Check(!host.SubmitControlActions(frame) && !host.GetMtaInputState(input), "no logical input callback is possible after unload");
            Check(host.Start(owner, modulePath), "explicit stop permits a fresh module session");
        }
        Check(GetModuleHandleW(modulePath.c_str()) == nullptr, "RAII destruction releases and unloads the second session");
    }

    void TestAdjacentPath()
    {
        std::string error;
        const auto  path = CGameAdapterModule::AdjacentModulePath(error);
        Check(error.empty() && !path.empty(), "host-module path resolves without changing current directory");
        Check(std::filesystem::path(path).is_absolute(), "adjacent adapter path is absolute");
        Check(std::filesystem::path(path).filename() == L"game_de.dll", "adjacent module name remains fixed");
    }

    void TestControlledLocalInputProvider(CCoreInterface* owner, const std::wstring& path, unsigned mode)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "Core starts the existing ABI6 provider with an optional local input group");
        const auto module = GetModuleHandleW(path.c_str());
        using Configure = void (*)(FixtureLocalInputStats*) noexcept;
        Configure  configure = nullptr;
        const auto address = GetProcAddress(module, "SetLocalInputFixtureStats");
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "local fixture exposes independent provider counters only");
        FixtureLocalInputStats stats;
        configure(&stats);
        LocalInputLeaseStateV1 state;
        state.generation = 99;
        if (mode == 2)
        {
            Check(!host.BeginLocalInputLease(reinterpret_cast<void*>(static_cast<std::uintptr_t>(1)), state) && state.generation == 99 && !stats.begins &&
                      host.GetError().find("complete optional local input") != std::string::npos,
                  "all four optional local exports are required before provider admission");
            Check(!host.RenewLocalInputLease(31) && !host.ReadLocalInputLease(state) && !host.EndLocalInputLease(31) && state.generation == 99,
                  "an incomplete optional group cannot expose partial Read/Renew/End");
            Check(host.SubmitControlActions({}), "unavailable local service leaves historical action staging usable");
            host.Stop();
            Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && stats.shutdowns == 1 && stats.releases == 1,
                  "missing optional local service leaves ABI6 shutdown and module unload usable");
            return;
        }
        Check(host.SubmitControlActions({}), "stage real canonical neutral input before local lease admission");
        const bool began = host.BeginLocalInputLease(reinterpret_cast<void*>(static_cast<std::uintptr_t>(1)), state);
        Check(began == (mode == 1) && stats.begins == 1 && stats.neutralBegin == 1,
              "local Begin uses neutral controller history without substituting a bounded trial");
        if (mode == 4)
        {
            Check(state.generation == 99 && !host.EndLocalInputLease(0) && !host.EndLocalInputLease(31) && !stats.ends,
                  "Unsupported without an owned token preserves output and cannot certify End(0)");
            host.Stop();
            Check(host.IsStarted() && GetModuleHandleW(path.c_str()) && stats.resets == 1 && !stats.shutdowns && !stats.releases,
                  "unidentified partial admission retains the module while provider Reset is pending");
            stats.allowCleanup = 1;
            Check(host.Reset(), "provider Reset retry explicitly retires unidentified local ownership");
            host.Stop();
            Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && stats.releases == 1,
                  "only acknowledged unknown-token cleanup permits adapter unload");
            return;
        }
        Check(state.generation == 31 && state.producerThread == GetCurrentThreadId(), "Begin retains a real provider cleanup token even on refusal");
        Check(!host.BeginLocalInputLease(reinterpret_cast<void*>(static_cast<std::uintptr_t>(1)), state) && stats.begins == 1,
              "pending local ownership refuses duplicate Begin before provider entry");
        ClockServiceStateV1 clock;
        Check(!host.BeginNativeClockService({}, clock) && host.GetError().find("local input lease") != std::string::npos && !host.BeginNativeMovementTrial() &&
                  !host.EndNativeMovementTrial(),
              "pending local input cannot overlap or be silently retired through clock or old bounded movement APIs");
        LocalInputLeaseStateV1 foreignState;
        foreignState.generation = 77;
        bool        foreignRejected = false;
        std::thread foreign([&]
                            { foreignRejected = !host.ReadLocalInputLease(foreignState) && !host.RenewLocalInputLease(31) && !host.EndLocalInputLease(31); });
        foreign.join();
        Check(foreignRejected && foreignState.generation == 77 && !stats.reads && !stats.renewals && !stats.ends,
              "all local loader operations preserve Core thread ownership and refused output");
        if (mode == 1)
        {
            ControlActionFrame movement;
            movement.heldActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(WALK);
            Check(host.SubmitControlActions(movement) && !stats.renewals, "MTA staging alone cannot renew or publish a native local frame");
            Check(host.RenewLocalInputLease(31) && stats.renewals == 1 && stats.canonicalReads == 1 && stats.lastRenewed.current.LeftStickY == -128 &&
                      stats.lastRenewed.current.m_bPedWalk == 255 && !stats.lastRenewed.previous.LeftStickY,
                  "explicit local renewal consumes the adapter's actual staged canonical walk history");
            movement.heldActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(SPRINT);
            Check(host.SubmitControlActions(movement) && stats.renewals == 1 && host.RenewLocalInputLease(31) && stats.renewals == 2 &&
                      stats.lastRenewed.current.ButtonCross == 255 && !stats.lastRenewed.current.m_bPedWalk && stats.lastRenewed.previous.m_bPedWalk == 255,
                  "next explicit renewal carries canonical sprint and preceding walk without rereading raw keys");
        }
        Check(!host.EndLocalInputLease(0) && !host.EndLocalInputLease(32) && !stats.ends, "stale and zero cleanup tokens never reach the provider");
        Check(!host.EndLocalInputLease(31) && stats.ends == 1 && host.IsStarted(), "busy local End retains the owned adapter");
        host.Stop();
        Check(host.IsStarted() && GetModuleHandleW(path.c_str()) && stats.ends == 2 && !stats.shutdowns && !stats.releases,
              "Stop retries busy End and retains all code before Shutdown/Release/unload");
        stats.allowCleanup = 1;
        Check(host.EndLocalInputLease(31) && host.ReadLocalInputLease(state) && !state.active && !state.bound && state.revoked,
              "successful same-token cleanup keeps terminal local state readable");
        Check(!host.RenewLocalInputLease(31) && host.EndLocalInputLease(31), "ended local input cannot renew while cleanup remains idempotent");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && stats.shutdowns == 1 && stats.releases == 1 && !host.IsGameplayReady(),
              "only successful local retirement releases the module without promoting full gameplay");
    }

    void TestControlledClockProvider(CCoreInterface* owner, const std::wstring& path, unsigned mode)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "Core loads a controlled optional clock provider without a CGame facade");
        const auto module = GetModuleHandleW(path.c_str());
        using Configure = void (*)(FixtureClockStats*) noexcept;
        using Observe = void (*)(void (*)() noexcept) noexcept;
        Configure configure = nullptr;
        Observe   observe = nullptr;
        auto      address = GetProcAddress(module, "SetClockFixtureStats");
        std::memcpy(&configure, &address, sizeof(configure));
        address = GetProcAddress(module, "SetClockFixtureOperationObserver");
        std::memcpy(&observe, &address, sizeof(observe));
        Check(configure && observe, "controlled clock provider exposes independent lifetime and post-operation allocation boundaries");
        FixtureClockStats stats;
        configure(&stats);
        ClockServiceStateV1 state;
        state.hour = 99;
        if (mode == 4)
        {
            Check(!host.BeginNativeClockService({}, state) && state.hour == 99 && !stats.begins &&
                      host.GetError().find("complete optional native clock") != std::string::npos,
                  "a partial optional clock export group refuses before provider entry and preserves caller output");
            host.Stop();
            Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && stats.shutdowns == 1 && stats.releases == 1,
                  "an incomplete optional service leaves its historical ABI6 adapter usable and unloadable");
            return;
        }
        observedCoreAllocations.store(0);
        observe(&ArmCoreAllocationObservation);
        const bool began = host.BeginNativeClockService({}, state);
        observeCoreAllocations.store(false);
        observe(nullptr);
        Check(began == (mode != 1) && state.generation == 17 && stats.begins == 1,
              "failed native clock Begin remains distinguishable while preserving its returned cleanup generation");
        Check(!observedCoreAllocations.load(), "clock post-Begin status diagnostics allocate nothing after a provider can retain stopped heap peers");
        observedCoreAllocations.store(0);
        observeCoreAllocations.store(true);
        LocalInputLeaseStateV1 localInputState;
        localInputState.generation = 99;
        const bool excluded = !host.BeginNativeClockService({}, state) && !host.BeginNativeRenderObservation() &&
                              !host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && !host.BeginNativeMovementTrial() &&
                              !host.BeginLocalInputLease(reinterpret_cast<void*>(static_cast<std::uintptr_t>(1)), localInputState) &&
                              localInputState.generation == 99 && !host.StartAdjacent(owner) && host.GameplayUnavailableReason().empty();
        observeCoreAllocations.store(false);
        Check(excluded && stats.begins == 1 && !observedCoreAllocations.load(),
              "an active or failed clock attempt excludes duplicate services and loader paths without allocating recovery diagnostics");
        observedCoreAllocations.store(0);
        observeCoreAllocations.store(true);
        ClockCommandV1 command;
        command.generation = 17;
        command.sequence = 1;
        const bool refused = !host.SubmitNativeClockCommand(command) && !host.RenewNativeClockService(17);
        observeCoreAllocations.store(false);
        Check(refused && stats.submits == 1 && stats.renewals == 1 && !observedCoreAllocations.load(),
              "clock Submit and Renew failure formatting remain allocation-free during pending native ownership");
        bool foreignRead = true;
        observedCoreAllocations.store(0);
        std::thread foreign(
            [&]
            {
                ClockServiceStateV1 untouched;
                untouched.hour = 99;
                observeCoreAllocations.store(true);
                foreignRead = host.ReadNativeClockService(untouched) || untouched.hour != 99;
                observeCoreAllocations.store(false);
            });
        foreign.join();
        Check(!foreignRead && !stats.reads && !observedCoreAllocations.load(),
              "foreign-thread clock refusal preserves output and formats its error without consumer CRT requests");
        observedCoreAllocations.store(0);
        observe(&ArmCoreAllocationObservation);
        const bool ended = host.EndNativeClockService(17);
        observeCoreAllocations.store(false);
        observe(nullptr);
        Check(!ended && stats.ends == 1 && !observedCoreAllocations.load(),
              "failed clock End retains its diagnostic guard without allocating after the provider's risky return");
        Check(host.IsStarted() && GetModuleHandleW(path.c_str()) && !stats.releases,
              "failed clock Begin or End cannot release the adapter or unload its callbacks");
        observedCoreAllocations.store(0);
        observe(&ArmCoreAllocationObservation);
        host.Stop();
        observeCoreAllocations.store(false);
        observe(nullptr);
        Check(host.IsStarted() && stats.shutdowns == 1 && !stats.releases && !observedCoreAllocations.load(),
              "failed adapter Shutdown keeps clock ownership and formats retained cleanup without allocating");
        stats.control = 1;
        observedCoreAllocations.store(0);
        observeCoreAllocations.store(true);
        const bool cleaned = host.EndNativeClockService(17);
        observeCoreAllocations.store(false);
        Check(cleaned && stats.ends == 2 && !observedCoreAllocations.load() && host.ReadNativeClockService(state) && state.restored && !state.hooked &&
                  !state.retainedPeerThreads,
              "confirmed End retries release provider clock ownership while keeping allocation-safe diagnostics available");
        if (mode == 3)
        {
            observedCoreAllocations.store(0);
            observe(&ArmCoreAllocationObservation);
            host.Stop();
            observeCoreAllocations.store(false);
            observe(nullptr);
            Check(host.IsStarted() && stats.shutdowns == 2 && !stats.releases && !observedCoreAllocations.load(),
                  "successful clock End does not discard the diagnostic guard before a later failed adapter Shutdown");
            stats.control |= 2;
        }
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && stats.releases == 1 && !host.IsGameplayReady(),
              "only confirmed cleanup releases the optional clock adapter and loaded module without granting gameplay readiness");
        Check(host.Start(owner, path), "the actual same consumer can own a fresh provider after confirmed cleanup reset");
        // The reloaded DLL has no clock attempt or configured state. Its clean
        // Shutdown demonstrates that old pending/diagnostic flags did not block
        // a new owner or carry stale cleanup across an unload boundary.
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()), "confirmed Stop resets clock ownership for a later Core consumer lifetime");
    }

    void TestFailedShutdownRetention(CCoreInterface* owner, const std::wstring& fixturePath)
    {
        Check(GetModuleHandleW(fixturePath.c_str()) == nullptr, "cleanup fixture begins unloaded");
        CGameAdapterModule host;
        Check(host.Start(owner, fixturePath), "load an explicit provider whose first Shutdown fails");
        mta::game_de::ClockServiceRequestV1 clockRequest;
        mta::game_de::ClockServiceStateV1   clockState;
        clockState.hour = 99;
        Check(!host.BeginNativeClockService(clockRequest, clockState) && clockState.hour == 99 &&
                  host.GetError().find("complete optional native clock") != std::string::npos,
              "older ABI6 providers remain usable and explicitly refuse the absent optional clock group");
        host.Stop();
        Check(host.IsStarted() && GetModuleHandleW(fixturePath.c_str()) != nullptr, "failed Shutdown retains both adapter and loaded callback code");
        Check(host.GetError().find("retained") != std::string::npos && host.GetError().find("Shutdown") != std::string::npos,
              "failed cleanup reports retained ownership");
        Check(!host.Start(owner, fixturePath) && host.Pulse(), "cleanup failure neither replaces nor deletes the retained adapter");
        host.Stop();
        Check(!host.IsStarted() && GetModuleHandleW(fixturePath.c_str()) == nullptr,
              "a successful cleanup retry releases the instance before unloading its DLL");
    }

    void TestFacadeProvider(CCoreInterface* owner, const std::wstring& fixturePath, unsigned mode)
    {
        Check(GetModuleHandleW(fixturePath.c_str()) == nullptr, "facade fixture begins unloaded");
        CGameAdapterModule host;
        Check(host.Start(owner, fixturePath), "Core owns the explicitly selected facade fixture");
        std::uint32_t counters[14]{};
        const auto    address = GetProcAddress(GetModuleHandleW(fixturePath.c_str()), "SetFacadeFixtureCounters");
        using ConfigureCounters = void (*)(std::uint32_t*) noexcept;
        ConfigureCounters configure = nullptr;
        static_assert(sizeof(configure) == sizeof(address));
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "facade fixture exposes lifetime counters without another module reference");
        configure(counters);
        // This address has no CGame vtable. The positive fixture proves only a
        // borrowed pointer's admission and lifetime, never a working game API.
        CGame* const sentinel = reinterpret_cast<CGame*>(std::uintptr_t{1});
        CGame*       game = sentinel;
        const bool   borrowed = host.BorrowGameInterface(game);
        Check(borrowed == (mode == 3), "facade admission requires both its capability and nonnull pointer");
        if (mode != 3)
        {
            Check(game == sentinel, "rejected facade provider preserves caller output");
            Check(counters[2] == (mode == 1 ? 0u : 1u), "missing capability never invokes the provider's facade getter");
            host.Stop();
            Check(!host.IsStarted() && GetModuleHandleW(fixturePath.c_str()) == nullptr && counters[0] == 1 && counters[1] == 1,
                  "rejected facade provider shuts down and releases before unload");
        }
        else
        {
            Check(game && game != sentinel && counters[2] == 1, "positive facade is an explicitly borrowed identity only");
            Check(!host.IsGameplayReady(), "GameInterface alone cannot claim input, remote context or gameplay readiness");
            bool        foreignBorrow = true;
            bool        foreignGameplay = true;
            CGame*      foreignGame = sentinel;
            std::thread foreign(
                [&]
                {
                    foreignBorrow = host.BorrowGameInterface(foreignGame);
                    foreignGameplay = host.IsGameplayReady();
                });
            foreign.join();
            Check(!foreignBorrow && !foreignGameplay && foreignGame == sentinel && counters[2] == 1,
                  "foreign borrower and gameplay preflight cannot invoke the getter or replace output");
            CGame* const identity = game;
            // Core relinquishes its borrowed view before adapter teardown. If
            // the consumer ever calls Terminate/delete on this identity the
            // fixture cannot run successfully; only adapter callbacks own it.
            game = nullptr;
            host.Stop();
            Check(host.IsStarted() && GetModuleHandleW(fixturePath.c_str()) != nullptr && counters[0] == 1 && counters[1] == 0,
                  "failed borrowed-provider Shutdown retains adapter and DLL without releasing the facade");
            Check(host.BorrowGameInterface(game) && game == identity && counters[2] == 2, "retained adapter still owns the same facade through cleanup retry");
            game = nullptr;
            host.Stop();
            Check(!host.IsStarted() && GetModuleHandleW(fixturePath.c_str()) == nullptr && counters[0] == 2 && counters[1] == 1,
                  "borrowed teardown uses Shutdown/Release before unload without invoking CGame");
        }
        game = sentinel;
        Check(!host.BorrowGameInterface(game) && game == sentinel, "released provider cannot replace caller facade output");
    }

    void TestRenderProvider(CCoreInterface* owner, const std::wstring& fixturePath, unsigned mode)
    {
        using namespace mta::game_de;
        Check(GetModuleHandleW(fixturePath.c_str()) == nullptr, "render provider begins unloaded");
        CGameAdapterModule host;
        Check(host.Start(owner, fixturePath), "Core owns the controlled optional render provider");
        std::uint32_t counters[14]{};
        const auto    address = GetProcAddress(GetModuleHandleW(fixturePath.c_str()), "SetFacadeFixtureCounters");
        using ConfigureCounters = void (*)(std::uint32_t*) noexcept;
        ConfigureCounters configure = nullptr;
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "render provider lifetime counters are available");
        configure(counters);
        RenderObservationState readback;
        readback.width = 123;
        Check(!host.GetNativeRenderObservationState(readback) && readback.width == 123, "no render lease preserves caller output");
        const auto factoryAddress = GetProcAddress(GetModuleHandleW(fixturePath.c_str()), "CreateGameRenderObservation");
        using Factory = decltype(&CreateGameRenderObservation);
        Factory factory = nullptr;
        std::memcpy(&factory, &factoryAddress, sizeof(factory));
        Check(factory && !factory(RenderObservationApiVersion + 1, owner) && !factory(RenderObservationApiVersion, nullptr),
              "optional factory rejects wrong versions and absent owners before allocation");
        Check(host.BeginNativeRenderObservation() == (mode != 2), "failed Begin is distinguishable from a successful observation lease");
        Check(counters[4] == 1 && !host.BeginNativeRenderObservation() && counters[4] == 1, "duplicate Begin cannot replace a retained lease");
        Check(host.GetNativeRenderObservationState(readback) && readback.active == 1 && readback.generation == 17,
              "even a failed Begin retains actual provider state for cleanup");
        const auto             readsBeforeForeign = counters[5];
        bool                   foreignBegin = true, foreignRead = true, foreignEnd = true, foreignReset = true;
        RenderObservationState foreignState;
        foreignState.width = 321;
        std::thread foreign(
            [&]
            {
                foreignBegin = host.BeginNativeRenderObservation();
                foreignRead = host.GetNativeRenderObservationState(foreignState);
                foreignEnd = host.EndNativeRenderObservation();
                foreignReset = host.Reset();
                host.Stop();
            });
        foreign.join();
        Check(!foreignBegin && !foreignRead && !foreignEnd && !foreignReset && foreignState.width == 321 && counters[4] == 1 &&
                  counters[5] == readsBeforeForeign && counters[6] == 0 && counters[0] == 0 && counters[3] == 0,
              "foreign operations cannot call or clean up an owned observer");
        if (mode == 4)
        {
            Check(host.Reset(), "owner reset ends and releases its optional observer");
            Check(counters[6] == 1 && counters[7] == 1 && counters[3] == 1 && counters[9] < counters[10], "observer End and Release precede adapter Reset");
            readback.width = 234;
            Check(!host.GetNativeRenderObservationState(readback) && readback.width == 234, "successful Reset clears final observation availability");
        }
        else
        {
            if (mode <= 2)
                host.Stop();
            else
                Check(!host.EndNativeRenderObservation(), "unsafe final observer readback retains the lease");
            Check(host.IsStarted() && GetModuleHandleW(fixturePath.c_str()) != nullptr && counters[6] == 1 && counters[7] == 0 && counters[0] == 0,
                  "failed Begin cleanup, End, final read or contradictory state cannot release adapter/module");
            Check(host.EndNativeRenderObservation(), "owner retries optional observer cleanup safely");
            const auto finalReads = counters[5];
            Check(counters[6] == 2 && counters[7] == 1 && host.GetNativeRenderObservationState(readback) && readback.active == 0 &&
                      readback.pendingCleanup == 0 && readback.generation == 17 && readback.nativePresentCallbacks == 9 && readback.acceptedSnapshots == 7 &&
                      readback.publicationMisses == 2 && readback.width == 640 && readback.lastOriginalReturned == 1 && readback.lastOriginalResult == 0 &&
                      counters[5] == finalReads,
                  "actual final state is cached before Release and preserves a returned false native bool");
            Check(host.EndNativeRenderObservation() && counters[6] == 2 && counters[7] == 1, "completed observer End is idempotent without another callback");
        }
        host.Stop();
        Check(!host.IsStarted() && GetModuleHandleW(fixturePath.c_str()) == nullptr && counters[0] == 1 && counters[1] == 1 && counters[7] == 1 &&
                  counters[9] < counters[11] && counters[11] < counters[12],
              "observer lease release precedes adapter Shutdown/Release and module unload");
        readback.width = 345;
        Check(!host.GetNativeRenderObservationState(readback) && readback.width == 345, "unloaded observer state cannot replace caller output");
    }

    void TestMissingRenderV2(CCoreInterface* owner, const std::wstring& fixturePath)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, fixturePath), "own an API1-only provider for missing-version2 admission");
        Check(!GetProcAddress(GetModuleHandleW(fixturePath.c_str()), "CreateGameRenderObservationV2"), "legacy provider has no version2 export");
        Check(!host.BeginNativeRenderObservation(RenderObservationRoute::D3D12) && host.GetError().find("no legacy route") != std::string::npos,
              "missing version2 export never substitutes the legacy D3D11 factory");
        RenderObservationStateV2 output;
        output.requestedRoute = 99;
        Check(!host.GetNativeRenderObservationState(output) && output.requestedRoute == 99, "missing version2 lease preserves output");
        Check(host.BeginNativeRenderObservation(), "missing optional version2 export leaves explicit API1 usable");
        host.Stop();
        Check(host.IsStarted(), "legacy cleanup failure remains retained after version2 rejection");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(fixturePath.c_str()), "legacy cleanup retry releases the API1-only provider");
    }

    void TestRenderProviderV2(CCoreInterface* owner, const std::wstring& fixturePath, unsigned mode, mta::game_de::RenderObservationRoute route)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, fixturePath), "own an explicitly selected version2 provider");
        std::uint32_t counters[14]{};
        using ConfigureCounters = void (*)(std::uint32_t*) noexcept;
        ConfigureCounters configure = nullptr;
        auto              address = GetProcAddress(GetModuleHandleW(fixturePath.c_str()), "SetFacadeFixtureCounters");
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "version2 provider counters available");
        configure(counters);
        using Factory = decltype(&CreateGameRenderObservationV2);
        Factory factory = nullptr;
        address = GetProcAddress(GetModuleHandleW(fixturePath.c_str()), "CreateGameRenderObservationV2");
        std::memcpy(&factory, &address, sizeof(factory));
        Check(factory && !factory(RenderObservationApiVersion, owner, route) && !factory(RenderObservationApiVersionV2, nullptr, route) &&
                  !factory(RenderObservationApiVersionV2, owner, RenderObservationRoute::Unknown) &&
                  !factory(RenderObservationApiVersionV2, owner, static_cast<RenderObservationRoute>(99)),
              "version2 factory rejects legacy version, absent owner and unsupported route");
        RenderObservationStateV2 output;
        output.base.width = 123;
        output.requestedRoute = 99;
        Check(!host.BeginNativeRenderObservation(RenderObservationRoute::Unknown) &&
                  !host.BeginNativeRenderObservation(static_cast<RenderObservationRoute>(99)) && !host.GetNativeRenderObservationState(output) &&
                  output.base.width == 123 && output.requestedRoute == 99 && counters[4] == 0,
              "unsupported route rejection occurs before provider Begin and preserves unknown output");
        Check(host.BeginNativeRenderObservation(route) == (mode != 2), "explicit version2 route Begin retains both success and cleanup failure");
        const auto other = route == RenderObservationRoute::D3D11 ? RenderObservationRoute::D3D12 : RenderObservationRoute::D3D11;
        Check(!host.BeginNativeRenderObservation() && !host.BeginNativeRenderObservation(other) && counters[4] == 1,
              "a retained route lease cannot be replaced by API1 or another route");
        Check(host.GetNativeRenderObservationState(output) && output.requestedRoute == static_cast<std::uint32_t>(route),
              "version2 provider reports its explicit immutable requested route");
        if (mode == 6)
            Check(output.observedRoute == 0 && output.presenceKnownMask == 0 && output.queuePresent == 0 && !output.base.nativePresentCallbacks &&
                      !output.base.acceptedSnapshots && !output.base.lastOriginalReturned,
                  "no callback cannot infer observed route or member presence from a request");
        else
            Check(output.observedRoute == output.requestedRoute && output.presenceKnownMask == (route == RenderObservationRoute::D3D11 ? 15u : 19u) &&
                      output.queuePresent == (route == RenderObservationRoute::D3D12 ? 1u : 0u) &&
                      output.base.devicePresent == (route == RenderObservationRoute::D3D11 ? 1u : 0u) &&
                      output.base.contextPresent == (route == RenderObservationRoute::D3D11 ? 1u : 0u),
                  "route-specific known fields distinguish D3D12 queue from unknown device and immediate context");
        RenderObservationState legacyOutput;
        legacyOutput.width = 77;
        Check(!host.GetNativeRenderObservationState(legacyOutput) && legacyOutput.width == 77, "API1 read cannot reinterpret a version2 lease");
        const auto               priorReads = counters[5];
        bool                     foreignBegin = true, foreignRead = true, foreignEnd = true;
        RenderObservationStateV2 foreignOutput;
        foreignOutput.observedRoute = 99;
        std::thread foreign(
            [&]
            {
                foreignBegin = host.BeginNativeRenderObservation(route);
                foreignRead = host.GetNativeRenderObservationState(foreignOutput);
                foreignEnd = host.EndNativeRenderObservation();
                host.Stop();
            });
        foreign.join();
        Check(!foreignBegin && !foreignRead && !foreignEnd && foreignOutput.observedRoute == 99 && counters[5] == priorReads && !counters[6] && !counters[0],
              "foreign version2 calls preserve output without invoking the provider or teardown");
        if (mode == 7)
        {
            counters[13] = 1;
            output.observedRoute = 77;
            Check(!host.GetNativeRenderObservationState(output) && output.observedRoute == 77,
                  "cross-route observed identity is rejected without publishing caller output");
        }
        if (mode == 4)
        {
            Check(host.Reset() && counters[6] == 1 && counters[7] == 1 && counters[3] == 1 && counters[9] < counters[10],
                  "version2 End and Release precede adapter Reset");
            output.requestedRoute = 77;
            Check(!host.GetNativeRenderObservationState(output) && output.requestedRoute == 77, "Reset clears the version2 final cache");
        }
        else
        {
            if (mode <= 2 || mode == 7)
                host.Stop();
            else if (mode != 6)
                Check(!host.EndNativeRenderObservation(), "version2 failed final read or unsafe active state retains lease");
            if (mode != 6)
                Check(host.IsStarted() && counters[6] == 1 && !counters[7] && !counters[0] && GetModuleHandleW(fixturePath.c_str()),
                      "failed version2 End, read or route validation retains the adapter and DLL");
            counters[13] = 0;
            Check(host.EndNativeRenderObservation(), "owner finishes version2 cleanup after transient failure");
            const auto finalReads = counters[5];
            Check(host.GetNativeRenderObservationState(output) && !output.base.active && !output.base.pendingCleanup &&
                      output.requestedRoute == static_cast<std::uint32_t>(route) && counters[5] == finalReads && counters[7] == 1,
                  "actual final version2 state is cached before Release without another provider call");
            if (mode == 6)
                Check(output.observedRoute == 0 && !output.base.nativePresentCallbacks, "clean End with no callbacks still leaves observed route unknown");
            Check(host.EndNativeRenderObservation() && counters[7] == 1, "version2 completed End is idempotent");
        }
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(fixturePath.c_str()) && counters[7] == 1 && counters[0] == 1 && counters[1] == 1 &&
                  counters[9] < counters[11],
              "version2 lease Release precedes adapter Shutdown and module unload");
        output.requestedRoute = 99;
        Check(!host.GetNativeRenderObservationState(output) && output.requestedRoute == 99, "released version2 state preserves caller output");
    }
    void TestMissingRenderV3(CCoreInterface* owner, const std::wstring& path)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own a real API2-only provider for missing API3 admission");
        Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "CreateGameRenderObservationV3"), "API2 provider genuinely has no API3 export");
        RenderObservationStateV3 output;
        output.gpu.queryStage = 99;
        Check(!host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D12) && host.GetError().find("no legacy route") != std::string::npos &&
                  !host.GetNativeRenderObservationState(output) && output.gpu.queryStage == 99,
              "missing API3 never falls back to API1/2 or publishes their state as ownership evidence");
        Check(host.BeginNativeRenderObservation(RenderObservationRoute::D3D12), "explicit API2 remains usable after API3 rejection");
        Check(!host.EndNativeRenderObservation() && host.EndNativeRenderObservation(), "older API2 keeps its original retry lifetime");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()), "missing API3 leaves normal owner cleanup available");
    }

    void TestRenderProviderV3(CCoreInterface* owner, const std::wstring& path, unsigned mode)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own controlled separately versioned API3 provider");
        std::uint32_t counters[14]{};
        using Configure = void (*)(std::uint32_t*) noexcept;
        Configure configure = nullptr;
        auto      address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFacadeFixtureCounters");
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "API3 controlled lifetime counters available");
        configure(counters);
        using Factory = decltype(&CreateGameRenderObservationV3);
        Factory factory = nullptr;
        address = GetProcAddress(GetModuleHandleW(path.c_str()), "CreateGameRenderObservationV3");
        std::memcpy(&factory, &address, sizeof(factory));
        Check(factory && !factory(2, owner, RenderObservationRoute::D3D12) && !factory(4, owner, RenderObservationRoute::D3D12) &&
                  !factory(3, nullptr, RenderObservationRoute::D3D12) && !factory(3, owner, RenderObservationRoute::D3D11),
              "API3 factory rejects wrong version/owner and earlier route");
        RenderObservationStateV3 output;
        output.gpu.queryStage = 99;
        Check(!host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::Unknown) &&
                  !host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D11) && !host.GetNativeRenderObservationState(output) &&
                  output.gpu.queryStage == 99 && !counters[4],
              "invalid API3 route rejects before provider entry and preserves output");
        Check(host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D12) == (mode != 2),
              "API3 failed Begin remains distinguishable and cleanup-owned");
        Check(!host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D12) && !host.BeginNativeRenderObservation() &&
                  !host.BeginNativeRenderObservation(RenderObservationRoute::D3D12) && counters[4] == 1,
              "retained API3 lease excludes every renderer version and duplicate Begin");
        Check(host.GetNativeRenderObservationState(output) && output.base.requestedRoute == 2,
              "API3 reports its immutable explicit route through its own state layout");
        Check(mode == 6 ? !output.base.observedRoute && !output.gpu.attempts && !output.gpu.metadataValid
                        : output.base.observedRoute == 2 && output.gpu.attempts == 7 && output.gpu.queryStage == 19 &&
                              output.gpu.accepted == (mode == 8 ? 0u : 7u) && output.gpu.rejected == (mode == 8 ? 7u : 0u),
              "API3 copies actual controlled zero-query, accepted or unsupported metadata without promotion");
        RenderObservationState   legacy;
        RenderObservationStateV2 older;
        legacy.width = older.requestedRoute = 99;
        Check(!host.GetNativeRenderObservationState(legacy) && !host.GetNativeRenderObservationState(older) && legacy.width == 99 && older.requestedRoute == 99,
              "API1/2 output cannot reinterpret the API3 lease");
        const auto               reads = counters[5];
        bool                     foreignBegin = true, foreignRead = true, foreignEnd = true;
        RenderObservationStateV3 foreignOutput;
        foreignOutput.gpu.queryStage = 77;
        std::thread foreign(
            [&]
            {
                foreignBegin = host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D12);
                foreignRead = host.GetNativeRenderObservationState(foreignOutput);
                foreignEnd = host.EndNativeRenderObservation();
                host.Stop();
            });
        foreign.join();
        Check(!foreignBegin && !foreignRead && !foreignEnd && foreignOutput.gpu.queryStage == 77 && counters[5] == reads && !counters[6] && !counters[0],
              "foreign API3 calls cannot publish output or touch the provider lifetime");
        if (mode == 7)
        {
            counters[13] = 1;
            output.gpu.queryStage = 88;
            Check(!host.GetNativeRenderObservationState(output) && output.gpu.queryStage == 88, "cross-observed route preserves API3 caller output");
            counters[13] = 2;
            Check(!host.GetNativeRenderObservationState(output) && output.gpu.queryStage == 88, "cross-requested route preserves API3 caller output");
        }
        if (mode == 4)
        {
            Check(host.Reset() && counters[6] == 1 && counters[7] == 1 && counters[3] == 1 && counters[9] < counters[10],
                  "API3 End/Release precede adapter Reset");
            output.gpu.queryStage = 88;
            Check(!host.GetNativeRenderObservationState(output) && output.gpu.queryStage == 88, "Reset clears API3 final state availability");
        }
        else
        {
            if (mode <= 2 || mode == 7)
                host.Stop();
            else if (mode == 3 || mode == 5)
                Check(!host.EndNativeRenderObservation(), "failed API3 final read or active end state retains the lease");
            if (mode <= 3 || mode == 5 || mode == 7)
                Check(host.IsStarted() && counters[6] == 1 && !counters[7] && !counters[0] && GetModuleHandleW(path.c_str()),
                      "API3 Begin/End/read/route failure retains adapter and module for retry");
            counters[13] = 0;
            Check(host.EndNativeRenderObservation(), "owner completes API3 End after the retained failure");
            const auto finalReads = counters[5];
            Check(host.GetNativeRenderObservationState(output) && !output.base.base.active && !output.base.base.pendingCleanup && counters[5] == finalReads &&
                      counters[7] == 1 && output.gpu.queryStage == (mode == 6 ? 0u : 19u),
                  "ended API3 cache contains actual final metadata without touching released provider");
            if (mode == 8)
                Check(!output.gpu.supportPredicate && output.gpu.queryStatus == 2 && output.gpu.rejected == 7 && !host.IsGameplayReady(),
                      "unsupported GPU metadata remains unsupported after End and grants no gameplay readiness");
            Check(host.EndNativeRenderObservation() && counters[7] == 1, "API3 completed End is idempotent");
        }
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && counters[7] == 1 && counters[0] == 1 && counters[1] == 1 && counters[9] < counters[11],
              "API3 observer release precedes adapter shutdown and unload");
        output.gpu.queryStage = 99;
        Check(!host.GetNativeRenderObservationState(output) && output.gpu.queryStage == 99, "unloaded API3 cannot replace caller output");
    }
    void TestMissingRenderV4(CCoreInterface* owner, const std::wstring& path)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own a real API3-only provider for missing API4 admission");
        Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "CreateGameRenderObservationV4"), "API3 provider genuinely has no API4 export");
        RenderObservationStateV4 output;
        output.base.gpu.queryStage = 99;
        Check(!host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D12) && host.GetError().find("no legacy route") != std::string::npos &&
                  !host.GetNativeRenderObservationState(output) && output.base.gpu.queryStage == 99,
              "missing API4 never falls back to API1/2/3 or publishes their state as classification evidence");
        Check(host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D12), "explicit API3 remains usable after API4 rejection");
        Check(!host.EndNativeRenderObservation() && host.EndNativeRenderObservation(), "older API3 keeps its original retry lifetime");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()), "missing API4 leaves normal owner cleanup available");
    }

    void TestRenderProviderV4(CCoreInterface* owner, const std::wstring& path, unsigned mode)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own controlled separately versioned API4 provider");
        std::uint32_t counters[14]{};
        using Configure = void (*)(std::uint32_t*) noexcept;
        Configure configure = nullptr;
        auto      address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFacadeFixtureCounters");
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "API4 controlled lifetime counters available");
        configure(counters);
        using Factory = decltype(&CreateGameRenderObservationV4);
        Factory factory = nullptr;
        address = GetProcAddress(GetModuleHandleW(path.c_str()), "CreateGameRenderObservationV4");
        std::memcpy(&factory, &address, sizeof(factory));
        Check(factory && !factory(3, owner, RenderObservationRoute::D3D12) && !factory(5, owner, RenderObservationRoute::D3D12) &&
                  !factory(4, nullptr, RenderObservationRoute::D3D12) && !factory(4, owner, RenderObservationRoute::D3D11),
              "API4 factory rejects wrong version/owner and earlier route");
        RenderObservationStateV4 output;
        output.base.gpu.queryStage = 99;
        output.custom.status = 99;
        Check(!host.BeginNativeRenderClassificationObservation(RenderObservationRoute::Unknown) &&
                  !host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D11) && !host.GetNativeRenderObservationState(output) &&
                  output.base.gpu.queryStage == 99 && output.custom.status == 99 && !counters[4],
              "invalid API4 route rejects before provider entry and preserves output");
        Check(host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D12) == (mode != 2),
              "API4 failed Begin remains distinguishable and cleanup-owned");
        Check(!host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D12) && !host.BeginNativeRenderObservation() &&
                  !host.BeginNativeRenderObservation(RenderObservationRoute::D3D12) &&
                  !host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D12) &&
                  !host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12) && counters[4] == 1,
              "retained API4 lease excludes every renderer version and duplicate Begin");
        Check(host.GetNativeRenderObservationState(output) && output.base.base.requestedRoute == 2,
              "API4 reports its immutable explicit route through its own state layout");
        Check(mode == 6 ? !output.base.base.observedRoute && !output.base.gpu.attempts && !output.base.gpu.metadataValid
                        : output.base.base.observedRoute == 2 && output.base.gpu.attempts == 7 && output.base.gpu.queryStage == 19 &&
                              output.base.gpu.accepted == 0 && output.base.gpu.rejected == 7,
              "API4 copies actual controlled zero-query or custom-unsupported metadata without promotion");
        RenderObservationState   legacy;
        RenderObservationStateV2 older;
        RenderObservationStateV3 previous;
        previous.gpu.queryStage = 99;
        legacy.width = older.requestedRoute = 99;
        Check(!host.GetNativeRenderObservationState(legacy) && !host.GetNativeRenderObservationState(older) &&
                  !host.GetNativeRenderObservationState(previous) && previous.gpu.queryStage == 99 && legacy.width == 99 && older.requestedRoute == 99,
              "API1/2/3 output cannot reinterpret the API4 lease");
        Check(mode == 6 ? output.custom.status == 0 && output.custom.implementationId == 0 && output.custom.slotSignatureMask == 0 &&
                              output.custom.staticPolicyFlags == 0
              : mode == 8 ? output.custom.status == 3 && output.custom.implementationId == 0
                          : output.custom.status == 2 && output.custom.implementationId == 1 && output.custom.slotSignatureMask == 127 &&
                                output.custom.staticPolicyFlags == 15,
              "API4 classification must preserve unknown/known scalars without promoting GPU support");
        const auto               reads = counters[5];
        bool                     foreignBegin = true, foreignRead = true, foreignEnd = true;
        RenderObservationStateV4 foreignOutput;
        foreignOutput.base.gpu.queryStage = 77;
        foreignOutput.custom.status = 77;
        std::thread foreign(
            [&]
            {
                foreignBegin = host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D12);
                foreignRead = host.GetNativeRenderObservationState(foreignOutput);
                foreignEnd = host.EndNativeRenderObservation();
                host.Stop();
            });
        foreign.join();
        Check(!foreignBegin && !foreignRead && !foreignEnd && foreignOutput.base.gpu.queryStage == 77 && foreignOutput.custom.status == 77 &&
                  counters[5] == reads && !counters[6] && !counters[0],
              "foreign API4 calls cannot publish output or touch the provider lifetime");
        if (mode == 7)
        {
            counters[13] = 1;
            output.base.gpu.queryStage = 88;
            output.custom.status = 88;
            Check(!host.GetNativeRenderObservationState(output) && output.base.gpu.queryStage == 88 && output.custom.status == 88,
                  "cross-observed route preserves API4 caller output");
            counters[13] = 2;
            Check(!host.GetNativeRenderObservationState(output) && output.base.gpu.queryStage == 88 && output.custom.status == 88,
                  "cross-requested route preserves API4 caller output");
        }
        if (mode == 4)
        {
            Check(host.Reset() && counters[6] == 1 && counters[7] == 1 && counters[3] == 1 && counters[9] < counters[10],
                  "API4 End/Release precede adapter Reset");
            output.base.gpu.queryStage = 88;
            output.custom.status = 88;
            Check(!host.GetNativeRenderObservationState(output) && output.base.gpu.queryStage == 88 && output.custom.status == 88,
                  "Reset clears API4 final state availability");
        }
        else
        {
            if (mode <= 2 || mode == 7)
                host.Stop();
            else if (mode == 3 || mode == 5)
                Check(!host.EndNativeRenderObservation(), "failed API4 final read or active end state retains the lease");
            if (mode <= 3 || mode == 5 || mode == 7)
                Check(host.IsStarted() && counters[6] == 1 && !counters[7] && !counters[0] && GetModuleHandleW(path.c_str()),
                      "API4 Begin/End/read/route failure retains adapter and module for retry");
            counters[13] = 0;
            Check(host.EndNativeRenderObservation(), "owner completes API4 End after the retained failure");
            const auto finalReads = counters[5];
            Check(host.GetNativeRenderObservationState(output) && !output.base.base.base.active && !output.base.base.base.pendingCleanup &&
                      counters[5] == finalReads && counters[7] == 1 && output.base.gpu.queryStage == (mode == 6 ? 0u : 19u),
                  "ended API4 cache contains actual final metadata without touching released provider");
            if (mode != 6)
                Check(!output.base.gpu.supportPredicate && output.base.gpu.queryStatus == 2 && output.base.gpu.rejected == 7 && !host.IsGameplayReady(),
                      "unsupported GPU metadata remains unsupported after End and grants no gameplay readiness");
            Check(output.custom.status == (mode == 6 ? 0u : mode == 8 ? 3u : 2u), "API4 final cache preserves classification before releasing its provider");
            Check(host.EndNativeRenderObservation() && counters[7] == 1, "API4 completed End is idempotent");
        }
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && counters[7] == 1 && counters[0] == 1 && counters[1] == 1 && counters[9] < counters[11],
              "API4 observer release precedes adapter shutdown and unload");
        output.base.gpu.queryStage = 99;
        output.custom.status = 99;
        Check(!host.GetNativeRenderObservationState(output) && output.base.gpu.queryStage == 99 && output.custom.status == 99,
              "unloaded API4 cannot replace caller output");
    }
    void TestMissingRenderV5(CCoreInterface* owner, const std::wstring& path)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own a real API4-only provider for missing API5 admission");
        Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "CreateGameRenderObservationV5"), "API4 provider genuinely has no API5 export");
        RenderObservationStateV5 output;
        output.base.base.gpu.queryStage = 99;
        Check(!host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12) && host.GetError().find("no legacy route") != std::string::npos &&
                  !host.GetNativeRenderObservationState(output) && output.base.base.gpu.queryStage == 99,
              "missing API5 never falls back to API1/2/3 or publishes their state as classification evidence");
        Check(host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D12), "explicit API4 remains usable after API5 rejection");
        Check(!host.EndNativeRenderObservation() && host.EndNativeRenderObservation(), "older API4 keeps its original retry lifetime");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()), "missing API5 leaves normal owner cleanup available");
    }

    void TestRenderProviderV5(CCoreInterface* owner, const std::wstring& path, unsigned mode)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own controlled separately versioned API5 provider");
        RenderEndpointDiagnosticV1 diagnostic;
        diagnostic.version = 77;
        Check(!host.GetNativeRenderEndpointDiagnostic(diagnostic) && diagnostic.version == 77, "endpoint diagnostics cannot invent a V5 lease before Begin");
        std::uint32_t counters[14]{};
        using Configure = void (*)(std::uint32_t*) noexcept;
        Configure configure = nullptr;
        auto      address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFacadeFixtureCounters");
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "API5 controlled lifetime counters available");
        configure(counters);
        using Factory = decltype(&CreateGameRenderObservationV5);
        Factory factory = nullptr;
        address = GetProcAddress(GetModuleHandleW(path.c_str()), "CreateGameRenderObservationV5");
        std::memcpy(&factory, &address, sizeof(factory));
        Check(factory && !factory(4, owner, RenderObservationRoute::D3D12) && !factory(6, owner, RenderObservationRoute::D3D12) &&
                  !factory(5, nullptr, RenderObservationRoute::D3D12) && !factory(5, owner, RenderObservationRoute::D3D11),
              "API5 factory rejects wrong version/owner and earlier route");
        RenderObservationStateV5 output;
        output.base.base.gpu.queryStage = 99;
        output.base.custom.status = 99;
        Check(!host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::Unknown) &&
                  !host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D11) && !host.GetNativeRenderObservationState(output) &&
                  output.base.base.gpu.queryStage == 99 && output.base.custom.status == 99 && !counters[4],
              "invalid API5 route rejects before provider entry and preserves output");
        Check(host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12) == (mode != 2),
              "API5 failed Begin remains distinguishable and cleanup-owned");
        Check(!host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12) && !host.BeginNativeRenderObservation() &&
                  !host.BeginNativeRenderObservation(RenderObservationRoute::D3D12) &&
                  !host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D12) &&
                  !host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D12) && counters[4] == 1,
              "retained API5 lease excludes every renderer version and duplicate Begin");
        Check(host.GetNativeRenderObservationState(output) && output.base.base.base.requestedRoute == 2,
              "API5 reports its immutable explicit route through its own state layout");
        const bool diagnosticAvailable = mode != 6 && mode != 8;
        Check(host.GetNativeRenderEndpointDiagnostic(diagnostic) == diagnosticAvailable &&
                  (diagnosticAvailable
                       ? diagnostic.version == 1 && diagnostic.generationLow == 17 && diagnostic.slotOwnerClass == 3 && diagnostic.slotRva == 0x1234
                       : diagnostic.version == 77),
              "optional endpoint export preserves known scalars while missing/wrong-version providers preserve caller output");
        if (mode == 6)
            Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "ReadGameRenderEndpointDiagnosticV1"),
                  "historical API5 fixture genuinely has no separate diagnostic export");
        Check(mode == 6 ? !output.base.base.base.observedRoute && !output.base.base.gpu.attempts && !output.base.base.gpu.metadataValid
                        : output.base.base.base.observedRoute == 2 && output.base.base.gpu.attempts == 7 && output.base.base.gpu.queryStage == 19 &&
                              output.base.base.gpu.accepted == 0 && output.base.base.gpu.rejected == 7,
              "API5 copies actual controlled zero-query or custom-unsupported metadata without promotion");
        RenderObservationState   legacy;
        RenderObservationStateV2 older;
        RenderObservationStateV3 previous;
        previous.gpu.queryStage = 99;
        legacy.width = older.requestedRoute = 99;
        Check(!host.GetNativeRenderObservationState(legacy) && !host.GetNativeRenderObservationState(older) &&
                  !host.GetNativeRenderObservationState(previous) && previous.gpu.queryStage == 99 && legacy.width == 99 && older.requestedRoute == 99,
              "API1/2/3 output cannot reinterpret the API5 lease");
        Check(mode == 6 ? output.base.custom.status == 0 && output.base.custom.implementationId == 0 && output.base.custom.slotSignatureMask == 0 &&
                              output.base.custom.staticPolicyFlags == 0
              : mode == 8 ? output.base.custom.status == 3 && output.base.custom.implementationId == 0
                          : output.base.custom.status == 2 && output.base.custom.implementationId == 1 && output.base.custom.slotSignatureMask == 127 &&
                                output.base.custom.staticPolicyFlags == 15,
              "API5 classification must preserve unknown/known scalars without promoting GPU support");
        RenderObservationStateV4 classificationOutput;
        classificationOutput.custom.status = 99;
        Check(!host.GetNativeRenderObservationState(classificationOutput) && classificationOutput.custom.status == 99,
              "API4 cannot reinterpret the API5 endpoint lease");
        Check(mode == 6   ? output.boundary.status == 0 && !output.boundary.presentEntries
              : mode == 8 ? output.boundary.status == 1 && !output.boundary.endpointProfileId
                          : output.boundary.status == 2 && output.boundary.factsMask == 255 && output.boundary.presentEntries == 7 &&
                                output.boundary.queryStage == 9 && output.boundary.nativeBufferIndex == output.boundary.dxgiBufferIndex,
              "API5 preserves actual zero, unadmitted and coherent association reports in its own typed state");
        const auto                 reads = counters[5];
        bool                       foreignBegin = true, foreignRead = true, foreignEnd = true, foreignDiagnostic = true;
        RenderEndpointDiagnosticV1 foreignDiagnosticOutput;
        foreignDiagnosticOutput.version = 88;
        RenderObservationStateV5 foreignOutput;
        foreignOutput.base.base.gpu.queryStage = 77;
        foreignOutput.base.custom.status = 77;
        std::thread foreign(
            [&]
            {
                foreignBegin = host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12);
                foreignRead = host.GetNativeRenderObservationState(foreignOutput);
                foreignDiagnostic = host.GetNativeRenderEndpointDiagnostic(foreignDiagnosticOutput);
                foreignEnd = host.EndNativeRenderObservation();
                host.Stop();
            });
        foreign.join();
        Check(!foreignDiagnostic && foreignDiagnosticOutput.version == 88,
              "a foreign Core thread cannot inspect an optional endpoint diagnostic or replace output");
        Check(!foreignBegin && !foreignRead && !foreignEnd && foreignOutput.base.base.gpu.queryStage == 77 && foreignOutput.base.custom.status == 77 &&
                  counters[5] == reads && !counters[6] && !counters[0],
              "foreign API5 calls cannot publish output or touch the provider lifetime");
        if (mode == 7)
        {
            counters[13] = 1;
            output.base.base.gpu.queryStage = 88;
            output.base.custom.status = 88;
            Check(!host.GetNativeRenderObservationState(output) && output.base.base.gpu.queryStage == 88 && output.base.custom.status == 88,
                  "cross-observed route preserves API5 caller output");
            counters[13] = 2;
            Check(!host.GetNativeRenderObservationState(output) && output.base.base.gpu.queryStage == 88 && output.base.custom.status == 88,
                  "cross-requested route preserves API5 caller output");
        }
        if (mode == 4)
        {
            Check(host.Reset() && counters[6] == 1 && counters[7] == 1 && counters[3] == 1 && counters[9] < counters[10],
                  "API5 End/Release precede adapter Reset");
            output.base.base.gpu.queryStage = 88;
            output.base.custom.status = 88;
            Check(!host.GetNativeRenderObservationState(output) && output.base.base.gpu.queryStage == 88 && output.base.custom.status == 88,
                  "Reset clears API5 final state availability");
            diagnostic.version = 77;
            Check(!host.GetNativeRenderEndpointDiagnostic(diagnostic) && diagnostic.version == 77, "Reset clears cached optional diagnostic availability too");
        }
        else
        {
            if (mode <= 2 || mode == 7)
                host.Stop();
            else if (mode == 3 || mode == 5)
                Check(!host.EndNativeRenderObservation(), "failed API5 final read or active end state retains the lease");
            if (mode <= 3 || mode == 5 || mode == 7)
                Check(host.IsStarted() && counters[6] == 1 && !counters[7] && !counters[0] && GetModuleHandleW(path.c_str()),
                      "API5 Begin/End/read/route failure retains adapter and module for retry");
            counters[13] = 0;
            Check(host.EndNativeRenderObservation(), "owner completes API5 End after the retained failure");
            diagnostic.version = 77;
            Check(host.GetNativeRenderEndpointDiagnostic(diagnostic) == diagnosticAvailable &&
                      (diagnosticAvailable ? diagnostic.version == 1 && diagnostic.slotRva == 0x1234 : diagnostic.version == 77),
                  "safe End preserves only successfully copied optional diagnostics after provider Release");
            const auto finalReads = counters[5];
            Check(host.GetNativeRenderObservationState(output) && !output.base.base.base.base.active && !output.base.base.base.base.pendingCleanup &&
                      counters[5] == finalReads && counters[7] == 1 && output.base.base.gpu.queryStage == (mode == 6 ? 0u : 19u),
                  "ended API5 cache contains actual final metadata without touching released provider");
            if (mode != 6)
                Check(!output.base.base.gpu.supportPredicate && output.base.base.gpu.queryStatus == 2 && output.base.base.gpu.rejected == 7 &&
                          !host.IsGameplayReady(),
                      "unsupported GPU metadata remains unsupported after End and grants no gameplay readiness");
            Check(output.base.custom.status == (mode == 6   ? 0u
                                                : mode == 8 ? 3u
                                                            : 2u),
                  "API5 final cache preserves classification before releasing its provider");
            Check(host.EndNativeRenderObservation() && counters[7] == 1, "API5 completed End is idempotent");
            counters[13] = 3;
            diagnostic.version = 77;
            output.base.custom.status = 99;
            Check(!host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12) && !host.GetNativeRenderEndpointDiagnostic(diagnostic) &&
                      diagnostic.version == 77 && !host.GetNativeRenderObservationState(output) && output.base.custom.status == 99,
                  "a rejected new V5 factory cannot relabel the completed run's cached diagnostic or observation");
            counters[13] = 0;
        }
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && counters[7] == 1 && counters[0] == 1 && counters[1] == 1 && counters[9] < counters[11],
              "API5 observer release precedes adapter shutdown and unload");
        output.base.base.gpu.queryStage = 99;
        output.base.custom.status = 99;
        Check(!host.GetNativeRenderObservationState(output) && output.base.base.gpu.queryStage == 99 && output.base.custom.status == 99,
              "unloaded API5 cannot replace caller output");
        diagnostic.version = 77;
        Check(!host.GetNativeRenderEndpointDiagnostic(diagnostic) && diagnostic.version == 77,
              "unloaded provider cannot expose a cached diagnostic through a new owner");
    }
    void TestMissingRenderV6(CCoreInterface* owner, const std::wstring& path)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own a real API5-only provider for missing API6 admission");
        Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "CreateGameRenderObservationV6"), "API5 provider genuinely has no API6 export");
        RenderObservationStateV6 output;
        output.base.base.base.gpu.queryStage = 99;
        Check(!host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && host.GetError().find("no legacy route") != std::string::npos &&
                  !host.GetNativeRenderObservationState(output) && output.base.base.base.gpu.queryStage == 99,
              "missing API6 never falls back to API1/2/3 or publishes their state as classification evidence");
        Check(host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D12), "explicit API4 remains usable after API6 rejection");
        Check(!host.EndNativeRenderObservation() && host.EndNativeRenderObservation(), "older API4 keeps its original retry lifetime");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()), "missing API6 leaves normal owner cleanup available");
    }

    void TestSetupDiagnosticGeneration(CCoreInterface* owner, const std::wstring& path)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own setup-generation consumer fixture");
        std::uint32_t counters[14]{};
        using Configure = void (*)(std::uint32_t*) noexcept;
        Configure  configure = nullptr;
        const auto address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFacadeFixtureCounters");
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "setup-generation provider exposes owned lifetime counters");
        configure(counters);
        RenderSetupDiagnosticV1 setup;
        Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && host.GetNativeRenderSetupDiagnostic(setup),
              "first API6 setup generation is readable");
        const auto firstGeneration = setup.generationLow;
        Check(firstGeneration != 0 && host.EndNativeRenderObservation() && host.GetNativeRenderSetupDiagnostic(setup) && setup.generationLow == firstGeneration,
              "clean End preserves the exact setup generation copied before Release");
        Check(host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12), "new historical API5 lease is independently selectable");
        setup.version = 77;
        Check(!host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77,
              "selecting an older API clears API6 setup evidence before its factory can return");
        Check(host.EndNativeRenderObservation() && host.Reset(), "older lease ends before setup-generation reset");
        setup.version = 77;
        Check(!host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77, "Reset cannot expose the prior setup cache");
        Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && host.GetNativeRenderSetupDiagnostic(setup) &&
                  setup.generationLow > firstGeneration,
              "a later successful API6 lease publishes its own generation instead of reusing cached values");
        Check(host.Reset(), "Reset safely ends and releases the later API6 lease");
        setup.version = 77;
        Check(!host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77, "second Reset clears the new setup generation too");
        Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && host.GetNativeRenderSetupDiagnostic(setup),
              "prime an active setup report before a controlled final-read miss");
        counters[13] = 4;
        Check(host.EndNativeRenderObservation(), "an optional final setup read miss cannot block otherwise safe Release");
        setup.version = 77;
        Check(!host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77,
              "final setup read miss drops pre-End cache instead of labeling stale cleanup fields as terminal evidence");
        counters[13] = 0;
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()), "setup-generation fixture unloads after safe cleanup");
    }

    void TestRootsDiagnosticGeneration(CCoreInterface* owner, const std::wstring& path)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own roots diagnostic generation consumer");
        auto module = GetModuleHandleW(path.c_str());
        using Configure = void (*)(std::uint32_t*) noexcept;
        Configure configure = nullptr;
        auto      address = GetProcAddress(module, "SetFacadeFixtureCounters");
        std::memcpy(&configure, &address, sizeof(configure));
        using ReadCount = std::uint32_t (*)() noexcept;
        ReadCount reads = nullptr;
        address = GetProcAddress(module, "GetFixtureRootsDiagnosticReadsV6");
        std::memcpy(&reads, &address, sizeof(reads));
        Check(configure && reads, "roots fixture exports owned counters");
        std::uint32_t counters[16]{};
        configure(counters);
        RenderRootsDiagnosticV1 roots;
        roots.version = 77;
        Check(!host.GetNativeRenderRootsDiagnostic(roots) && roots.version == 77, "roots cache unavailable before exact V6 lease");
        Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && host.GetNativeRenderRootsDiagnostic(roots) &&
                  roots.checkpoint == 26 && roots.refusalReason == 3 && roots.acceptedSnapshotsLow == 7 && roots.dispatchEntriesLow == 7 &&
                  roots.scalarValidMask == 3 && roots.contextNodeMask == 1 && roots.nativeBufferIndex == 2,
              "consumer copies the exact fourteen scalar diagnostic fields");
        const auto  generation = roots.generationLow;
        bool        foreignRead = true;
        std::thread foreign(
            [&]
            {
                RenderRootsDiagnosticV1 output;
                output.version = 77;
                foreignRead = host.GetNativeRenderRootsDiagnostic(output) || output.version != 77;
            });
        foreign.join();
        Check(!foreignRead, "foreign consumer thread cannot replace roots caller output");
        Check(host.EndNativeRenderObservation(), "roots diagnostic does not prevent safe End");
        const auto afterRelease = reads();
        Check(host.GetNativeRenderRootsDiagnostic(roots) && roots.generationLow == generation && reads() == afterRelease,
              "ended cache reads no released provider function");
        Check(host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12), "historical API5 lease starts after V6 cleanup");
        roots.version = 77;
        Check(!host.GetNativeRenderRootsDiagnostic(roots) && roots.version == 77, "selecting API5 drops roots generation availability");
        Check(host.EndNativeRenderObservation() && host.Reset(), "historical lease ends and resets before new roots generation");
        Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && host.GetNativeRenderRootsDiagnostic(roots) &&
                  roots.generationLow > generation,
              "later V6 lease cannot relabel the previous roots generation");
        counters[13] = 4;
        roots.version = 77;
        Check(!host.GetNativeRenderRootsDiagnostic(roots) && roots.version == 77, "busy current provider cannot replay cached roots as fresh evidence");
        Check(host.EndNativeRenderObservation(), "optional roots final-read miss cannot block Release");
        roots.version = 77;
        Check(!host.GetNativeRenderRootsDiagnostic(roots) && roots.version == 77, "final read miss clears pre-End roots cache");
        counters[13] = 0;
        Check(host.Reset(), "roots consumer Reset stays available");
        roots.version = 77;
        Check(!host.GetNativeRenderRootsDiagnostic(roots) && roots.version == 77, "Reset cannot expose prior roots values");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()), "roots consumer releases the owned provider normally");
    }

    mta::game_de::RenderDeviceLeaseV1 DeviceOutputSentinel() noexcept
    {
        mta::game_de::RenderDeviceLeaseV1 value;
        value.size = 77;
        value.version = 88;
        value.generation = 0xfedcba9876543210ull;
        value.acceptedSnapshots = 0x123456789abcdef0ull;
        value.dispatchEntries = 0xabcdef0123456789ull;
        value.sourceProfileId = 99;
        value.reserved = 66;
        return value;
    }

    bool SameDeviceOutput(const mta::game_de::RenderDeviceLeaseV1& a, const mta::game_de::RenderDeviceLeaseV1& b) noexcept
    {
        return a.size == b.size && a.version == b.version && a.device == b.device && a.generation == b.generation &&
               a.acceptedSnapshots == b.acceptedSnapshots && a.dispatchEntries == b.dispatchEntries && a.sourceProfileId == b.sourceProfileId &&
               a.reserved == b.reserved;
    }

    mta::game_de::RenderDeviceWindowLeaseV2 DeviceWindowOutputSentinel() noexcept
    {
        mta::game_de::RenderDeviceWindowLeaseV2 value;
        value.size = 77;
        value.version = 88;
        value.generation = 0xfedcba9876543210ull;
        value.acceptedSnapshots = 0x123456789abcdef0ull;
        value.dispatchEntries = 0xabcdef0123456789ull;
        value.sourceProfileId = 99;
        value.windowed = 55;
        value.reserved[0] = 66;
        value.reserved[1] = 67;
        return value;
    }
    bool SameDeviceWindowOutput(const mta::game_de::RenderDeviceWindowLeaseV2& a, const mta::game_de::RenderDeviceWindowLeaseV2& b) noexcept
    {
        return a.size == b.size && a.version == b.version && a.device == b.device && a.window == b.window && a.generation == b.generation &&
               a.acceptedSnapshots == b.acceptedSnapshots && a.dispatchEntries == b.dispatchEntries && a.sourceProfileId == b.sourceProfileId &&
               a.windowed == b.windowed && a.reserved[0] == b.reserved[0] && a.reserved[1] == b.reserved[1];
    }

    bool FinishDeviceObservation(CGameAdapterModule& host) noexcept
    {
        // Existing provider modes deliberately refuse the first End/final read.
        // Exercise the actual consumer retry without discarding its live lease.
        for (unsigned attempt = 0; attempt < 3; ++attempt)
            if (host.EndNativeRenderObservation())
                return true;
        return false;
    }

    void TestDeviceLeaseProvider(CCoreInterface* owner, const std::wstring& path, unsigned mode, const std::wstring& loaderProbePath)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "device policy fixture starts an owned controlled V6 provider");
        FixtureDeviceStats stats;
        using ConfigureDevice = void (*)(FixtureDeviceStats*) noexcept;
        ConfigureDevice configureDevice = nullptr;
        auto            address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFixtureDeviceStats");
        std::memcpy(&configureDevice, &address, sizeof(configureDevice));
        Check(configureDevice != nullptr, "device fixture exposes separate allocation/request/transfer counters");
        configureDevice(&stats);
        std::uint32_t lifetime[14]{};
        using ConfigureLifetime = void (*)(std::uint32_t*) noexcept;
        ConfigureLifetime configureLifetime = nullptr;
        address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFacadeFixtureCounters");
        std::memcpy(&configureLifetime, &address, sizeof(configureLifetime));
        Check(configureLifetime != nullptr, "device fixture retains the existing adapter lifetime counters");
        configureLifetime(lifetime);
        auto       output = DeviceOutputSentinel();
        const auto sentinel = output;
        Check(!host.TakeNativeRenderDeviceLease(output) && SameDeviceOutput(output, sentinel) && !stats.takes,
              "Take before an explicit lease preserves every output field without calling the provider");
        Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) == (mode != 2) && !stats.requests,
              "historical read-only V6 Begin never requests persistent device ownership");
        const auto readonlyAllocations = stats.allocations;
        Check(!host.BeginNativeRenderDeviceObservation() && stats.allocations == readonlyAllocations && !stats.requests,
              "explicit device Begin cannot replace a retained read-only observer");
        Check(!host.TakeNativeRenderDeviceLease(output) && SameDeviceOutput(output, sentinel) && !stats.takes,
              "read-only V6 never calls an optional Take export or upgrades into a device lease");
        Check(FinishDeviceObservation(host) && stats.releases == stats.allocations && !stats.liveLeases,
              "read-only fixture cleans its observation before any explicit device allocation");
        const auto previousBegins = stats.begins;
        const auto previousReleases = stats.releases;
        const bool began = host.BeginNativeRenderDeviceObservation();
        Check(began == (mode != 2 && mode != 6 && mode != 8), "explicit device Begin distinguishes missing, refused and failed admission");
        auto       windowOutput = DeviceWindowOutputSentinel();
        const auto windowSentinel = windowOutput;
        const auto takesBeforeWindow = stats.takes;
        Check(!host.TakeNativeRenderDeviceWindowLease(windowOutput) && SameDeviceWindowOutput(windowOutput, windowSentinel) && stats.takes == takesBeforeWindow,
              "legacy V1 request never caches or invokes the optional V2 window Take");
        if (mode == 6 || mode == 8)
        {
            if (mode == 6)
                Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "RequestGameRenderDeviceLeaseV1") &&
                          !GetProcAddress(GetModuleHandleW(path.c_str()), "TakeGameRenderDeviceLeaseV1") && !stats.requests,
                      "historical provider genuinely omits both optional device exports");
            else
                Check(stats.requests == 1, "refused Request is called once on the fresh allocation");
            Check(stats.begins == previousBegins && stats.releases == previousReleases + 1 && !stats.liveLeases && !lifetime[0],
                  "missing/refused Request disposes the never-begun lease immediately without fallback or adapter shutdown");
            const auto takes = stats.takes;
            Check(!host.TakeNativeRenderDeviceLease(output) && SameDeviceOutput(output, sentinel) && stats.takes == takes,
                  "rejected fresh device setup clears cached Take and preserves output");
            host.Stop();
            Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && stats.releases == stats.allocations,
                  "missing/refused device exports leak no fresh observation or owned module");
            return;
        }
        Check(stats.requests == 1 && stats.begins == previousBegins + 1 && stats.requestOrder < stats.beginOrder && stats.liveLeases == 1,
              "fresh Request completes before the exact V6 Begin and establishes one cleanup owner");
        stats.control = 2;
        Check(!host.TakeNativeRenderDeviceLease(output) && SameDeviceOutput(output, sentinel),
              "an unavailable requested result leaves every caller field untouched");
        stats.control = 0;
        if (mode == 2)
        {
            Check(!host.TakeNativeRenderDeviceLease(output) && SameDeviceOutput(output, sentinel) && !stats.transfers && !host.EndNativeRenderObservation() &&
                      stats.liveLeases == 1 && stats.releases == previousReleases && GetModuleHandleW(path.c_str()),
                  "failed Begin supplies no device and failed End retains provider/lease for retry");
            Check(FinishDeviceObservation(host), "owner retries failed device Begin cleanup safely");
        }
        else
        {
            auto occupied = sentinel;
            occupied.device = reinterpret_cast<ID3D12Device*>(&stats);
            const auto occupiedBefore = occupied;
            const auto takesBeforeOccupied = stats.takes;
            Check(!host.TakeNativeRenderDeviceLease(occupied) && SameDeviceOutput(occupied, occupiedBefore) && stats.takes == takesBeforeOccupied,
                  "consumer refuses to overwrite an already owned device before provider entry");
            bool        foreignBegin = true, foreignTake = true;
            auto        foreignOutput = sentinel;
            const auto  foreignRequests = stats.requests, foreignTakes = stats.takes;
            std::thread foreign(
                [&]
                {
                    foreignBegin = host.BeginNativeRenderDeviceObservation();
                    foreignTake = host.TakeNativeRenderDeviceLease(foreignOutput);
                });
            foreign.join();
            Check(
                !foreignBegin && !foreignTake && SameDeviceOutput(foreignOutput, sentinel) && stats.requests == foreignRequests && stats.takes == foreignTakes,
                "foreign Core thread cannot request or take a device or replace caller output");
            if (mode == 4)
            {
                wchar_t enteredName[96]{}, resumeName[96]{}, timeoutName[96]{};
                swprintf_s(enteredName, L"Local\\MtaApi6DiagnosticLoaderEntered-%lu", GetCurrentProcessId());
                swprintf_s(resumeName, L"Local\\MtaApi6DiagnosticLoaderResume-%lu", GetCurrentProcessId());
                swprintf_s(timeoutName, L"Local\\MtaApi6DiagnosticLoaderTimeout-%lu", GetCurrentProcessId());
                const auto entered = CreateEventW(nullptr, TRUE, FALSE, enteredName);
                const auto resume = CreateEventW(nullptr, TRUE, FALSE, resumeName);
                const auto timeout = CreateEventW(nullptr, TRUE, FALSE, timeoutName);
                Check(entered && resume && timeout, "device cached-Take loader gates created");
                HMODULE     loadedProbe = nullptr;
                std::thread loader(
                    [&]
                    { loadedProbe = LoadLibraryExW(loaderProbePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS); });
                const bool held = WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0;
                const auto started = GetTickCount64();
                const bool transferred = held && host.TakeNativeRenderDeviceLease(output);
                const auto elapsed = GetTickCount64() - started;
                SetEvent(resume);
                loader.join();
                const bool timedOut = WaitForSingleObject(timeout, 0) != WAIT_TIMEOUT;
                if (loadedProbe)
                    FreeLibrary(loadedProbe);
                CloseHandle(entered);
                CloseHandle(resume);
                CloseHandle(timeout);
                Check(held && loadedProbe && transferred && !timedOut && elapsed < 1500,
                      "pre-resolved device Take completes while another owned provider holds the real loader lock");
            }
            else
                Check(host.TakeNativeRenderDeviceLease(output), "requested controlled device transfers once");
            Check(output.size == sizeof(output) && output.version == RenderDeviceLeaseVersionV1 && output.device && output.generation &&
                      output.acceptedSnapshots == 7 && output.dispatchEntries == 7 && output.sourceProfileId == 2 && !output.reserved && stats.transfers == 1,
                  "device transfer copies exact value provenance and one opaque control token without COM calls");
            const auto transferred = output;
            auto       duplicate = sentinel;
            Check(!host.TakeNativeRenderDeviceLease(duplicate) && SameDeviceOutput(duplicate, sentinel) && stats.transfers == 1,
                  "second Take cannot replay ownership or partially replace output");
            const bool ended = host.EndNativeRenderObservation();
            if (mode == 1 || mode == 3 || mode == 5)
                Check(!ended && stats.liveLeases == 1 && stats.releases == previousReleases && host.IsStarted() && GetModuleHandleW(path.c_str()),
                      "failed End/final-read retains the device provider and exact observation for cleanup retry");
            else
                Check(ended, "controlled device observer ends safely");
            Check(FinishDeviceObservation(host) && SameDeviceOutput(output, transferred),
                  "safe observer End never releases or changes the caller's transferred token");
            const auto takesAfterEnd = stats.takes;
            duplicate = sentinel;
            Check(!host.TakeNativeRenderDeviceLease(duplicate) && SameDeviceOutput(duplicate, sentinel) && stats.takes == takesAfterEnd,
                  "safe End clears cached Take before Release and never calls or replays a released provider");
            const auto requestsAfterEnd = stats.requests;
            Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && stats.requests == requestsAfterEnd &&
                      !host.TakeNativeRenderDeviceLease(duplicate) && SameDeviceOutput(duplicate, sentinel) && stats.takes == takesAfterEnd,
                  "a later read-only V6 lease cannot inherit a previous device request or cached Take");
            Check(FinishDeviceObservation(host), "later read-only observer cleans before a fresh explicit request");
            Check(host.BeginNativeRenderDeviceObservation() && stats.requests == requestsAfterEnd + 1,
                  "a separate fresh V6 allocation can explicitly request a new one-shot device result");
            auto next = sentinel;
            Check(host.TakeNativeRenderDeviceLease(next) && next.device && next.generation > transferred.generation && stats.transfers == 2,
                  "new explicit lease transfers current generation provenance instead of cached prior ownership");
            Check(FinishDeviceObservation(host), "new explicit device generation completes cleanup");
        }
        const auto finalTakes = stats.takes;
        output = sentinel;
        Check(!host.TakeNativeRenderDeviceLease(output) && SameDeviceOutput(output, sentinel) && stats.takes == finalTakes,
              "completed device observer leaves no stale function or output cache");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && !stats.liveLeases && stats.allocations == stats.releases && lifetime[0] == 1,
              "all controlled device allocations release before adapter shutdown and module unload");
    }

    void TestDeviceWindowLeaseProvider(CCoreInterface* owner, const std::wstring& path, unsigned mode, const std::wstring& loaderProbePath)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "device policy fixture starts an owned controlled V6 provider");
        FixtureDeviceStats stats;
        using ConfigureDevice = void (*)(FixtureDeviceStats*) noexcept;
        ConfigureDevice configureDevice = nullptr;
        auto            address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFixtureDeviceStats");
        std::memcpy(&configureDevice, &address, sizeof(configureDevice));
        Check(configureDevice != nullptr, "device fixture exposes separate allocation/request/transfer counters");
        configureDevice(&stats);
        std::uint32_t lifetime[14]{};
        using ConfigureLifetime = void (*)(std::uint32_t*) noexcept;
        ConfigureLifetime configureLifetime = nullptr;
        address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFacadeFixtureCounters");
        std::memcpy(&configureLifetime, &address, sizeof(configureLifetime));
        Check(configureLifetime != nullptr, "device fixture retains the existing adapter lifetime counters");
        configureLifetime(lifetime);
        auto       output = DeviceWindowOutputSentinel();
        const auto sentinel = output;
        Check(!host.TakeNativeRenderDeviceWindowLease(output) && SameDeviceWindowOutput(output, sentinel) && !stats.takes,
              "Take before an explicit lease preserves every output field without calling the provider");
        Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) == (mode != 2) && !stats.requests,
              "historical read-only V6 Begin never requests persistent device ownership");
        const auto readonlyAllocations = stats.allocations;
        Check(!host.BeginNativeRenderDeviceWindowObservation() && stats.allocations == readonlyAllocations && !stats.requests,
              "explicit device Begin cannot replace a retained read-only observer");
        Check(!host.TakeNativeRenderDeviceWindowLease(output) && SameDeviceWindowOutput(output, sentinel) && !stats.takes,
              "read-only V6 never calls an optional Take export or upgrades into a device lease");
        Check(FinishDeviceObservation(host) && stats.releases == stats.allocations && !stats.liveLeases,
              "read-only fixture cleans its observation before any explicit device allocation");
        const auto previousBegins = stats.begins;
        const auto previousReleases = stats.releases;
        const bool began = host.BeginNativeRenderDeviceWindowObservation();
        Check(began == (mode != 2 && mode != 6 && mode != 7 && mode != 8), "explicit device Begin distinguishes missing, refused and failed admission");
        if (mode == 6 || mode == 7 || mode == 8)
        {
            if (mode == 6 || mode == 7)
                Check((GetProcAddress(GetModuleHandleW(path.c_str()), "RequestGameRenderDeviceWindowLeaseV2") != nullptr) == (mode == 7) &&
                          !GetProcAddress(GetModuleHandleW(path.c_str()), "TakeGameRenderDeviceWindowLeaseV2") && !stats.requests,
                      "provider genuinely omits both V2 exports or only Take; neither case falls back to V1");
            else
                Check(stats.requests == 1, "refused Request is called once on the fresh allocation");
            Check(stats.begins == previousBegins && stats.releases == previousReleases + 1 && !stats.liveLeases && !lifetime[0],
                  "missing/refused Request disposes the never-begun lease immediately without fallback or adapter shutdown");
            const auto takes = stats.takes;
            Check(!host.TakeNativeRenderDeviceWindowLease(output) && SameDeviceWindowOutput(output, sentinel) && stats.takes == takes,
                  "rejected fresh device setup clears cached Take and preserves output");
            host.Stop();
            Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && stats.releases == stats.allocations,
                  "missing/refused device exports leak no fresh observation or owned module");
            return;
        }
        Check(stats.requests == 1 && stats.begins == previousBegins + 1 && stats.requestOrder < stats.beginOrder && stats.liveLeases == 1,
              "fresh Request completes before the exact V6 Begin and establishes one cleanup owner");
        auto       legacyOutput = DeviceOutputSentinel();
        const auto legacySentinel = legacyOutput;
        const auto takesBeforeLegacy = stats.takes;
        Check(!host.TakeNativeRenderDeviceLease(legacyOutput) && SameDeviceOutput(legacyOutput, legacySentinel) && stats.takes == takesBeforeLegacy,
              "an explicit window lease cannot be consumed through the legacy V1 Take cache");
        stats.control = 2;
        Check(!host.TakeNativeRenderDeviceWindowLease(output) && SameDeviceWindowOutput(output, sentinel),
              "an unavailable requested result leaves every caller field untouched");
        stats.control = 0;
        if (mode == 2)
        {
            Check(!host.TakeNativeRenderDeviceWindowLease(output) && SameDeviceWindowOutput(output, sentinel) && !stats.transfers &&
                      !host.EndNativeRenderObservation() && stats.liveLeases == 1 && stats.releases == previousReleases && GetModuleHandleW(path.c_str()),
                  "failed Begin supplies no device and failed End retains provider/lease for retry");
            Check(FinishDeviceObservation(host), "owner retries failed device Begin cleanup safely");
        }
        else
        {
            auto occupied = sentinel;
            occupied.device = reinterpret_cast<ID3D12Device*>(&stats);
            const auto occupiedBefore = occupied;
            const auto takesBeforeOccupied = stats.takes;
            Check(!host.TakeNativeRenderDeviceWindowLease(occupied) && SameDeviceWindowOutput(occupied, occupiedBefore) && stats.takes == takesBeforeOccupied,
                  "consumer refuses to overwrite an already owned device before provider entry");
            occupied = sentinel;
            occupied.window = &stats;
            const auto occupiedWindowBefore = occupied;
            Check(!host.TakeNativeRenderDeviceWindowLease(occupied) && SameDeviceWindowOutput(occupied, occupiedWindowBefore) &&
                      stats.takes == takesBeforeOccupied,
                  "consumer refuses to overwrite an occupied HWND even when the device output is empty");
            bool        foreignBegin = true, foreignTake = true;
            auto        foreignOutput = sentinel;
            const auto  foreignRequests = stats.requests, foreignTakes = stats.takes;
            std::thread foreign(
                [&]
                {
                    foreignBegin = host.BeginNativeRenderDeviceWindowObservation();
                    foreignTake = host.TakeNativeRenderDeviceWindowLease(foreignOutput);
                });
            foreign.join();
            Check(!foreignBegin && !foreignTake && SameDeviceWindowOutput(foreignOutput, sentinel) && stats.requests == foreignRequests &&
                      stats.takes == foreignTakes,
                  "foreign Core thread cannot request or take a device or replace caller output");
            if (mode == 4)
            {
                wchar_t enteredName[96]{}, resumeName[96]{}, timeoutName[96]{};
                swprintf_s(enteredName, L"Local\\MtaApi6DiagnosticLoaderEntered-%lu", GetCurrentProcessId());
                swprintf_s(resumeName, L"Local\\MtaApi6DiagnosticLoaderResume-%lu", GetCurrentProcessId());
                swprintf_s(timeoutName, L"Local\\MtaApi6DiagnosticLoaderTimeout-%lu", GetCurrentProcessId());
                const auto entered = CreateEventW(nullptr, TRUE, FALSE, enteredName);
                const auto resume = CreateEventW(nullptr, TRUE, FALSE, resumeName);
                const auto timeout = CreateEventW(nullptr, TRUE, FALSE, timeoutName);
                Check(entered && resume && timeout, "device cached-Take loader gates created");
                HMODULE     loadedProbe = nullptr;
                std::thread loader(
                    [&]
                    { loadedProbe = LoadLibraryExW(loaderProbePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS); });
                const bool held = WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0;
                const auto started = GetTickCount64();
                const bool transferred = held && host.TakeNativeRenderDeviceWindowLease(output);
                const auto elapsed = GetTickCount64() - started;
                SetEvent(resume);
                loader.join();
                const bool timedOut = WaitForSingleObject(timeout, 0) != WAIT_TIMEOUT;
                if (loadedProbe)
                    FreeLibrary(loadedProbe);
                CloseHandle(entered);
                CloseHandle(resume);
                CloseHandle(timeout);
                Check(held && loadedProbe && transferred && !timedOut && elapsed < 1500,
                      "pre-resolved device Take completes while another owned provider holds the real loader lock");
            }
            else
                Check(host.TakeNativeRenderDeviceWindowLease(output), "requested controlled device transfers once");
            Check(output.size == sizeof(output) && output.version == RenderDeviceWindowLeaseVersionV2 && output.device && output.generation &&
                      output.acceptedSnapshots == 7 && output.dispatchEntries == 7 && output.sourceProfileId == 2 && output.window && output.windowed == 1 &&
                      !output.reserved[0] && !output.reserved[1] && stats.transfers == 1,
                  "device transfer copies exact value provenance and one opaque control token without COM calls");
            const auto transferred = output;
            auto       duplicate = sentinel;
            Check(!host.TakeNativeRenderDeviceWindowLease(duplicate) && SameDeviceWindowOutput(duplicate, sentinel) && stats.transfers == 1,
                  "second Take cannot replay ownership or partially replace output");
            const bool ended = host.EndNativeRenderObservation();
            if (mode == 1 || mode == 3 || mode == 5)
                Check(!ended && stats.liveLeases == 1 && stats.releases == previousReleases && host.IsStarted() && GetModuleHandleW(path.c_str()),
                      "failed End/final-read retains the device provider and exact observation for cleanup retry");
            else
                Check(ended, "controlled device observer ends safely");
            Check(FinishDeviceObservation(host) && SameDeviceWindowOutput(output, transferred),
                  "safe observer End never releases or changes the caller's transferred token");
            const auto takesAfterEnd = stats.takes;
            duplicate = sentinel;
            Check(!host.TakeNativeRenderDeviceWindowLease(duplicate) && SameDeviceWindowOutput(duplicate, sentinel) && stats.takes == takesAfterEnd,
                  "safe End clears cached Take before Release and never calls or replays a released provider");
            const auto requestsAfterEnd = stats.requests;
            Check(host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && stats.requests == requestsAfterEnd &&
                      !host.TakeNativeRenderDeviceWindowLease(duplicate) && SameDeviceWindowOutput(duplicate, sentinel) && stats.takes == takesAfterEnd,
                  "a later read-only V6 lease cannot inherit a previous device request or cached Take");
            Check(FinishDeviceObservation(host), "later read-only observer cleans before a fresh explicit request");
            Check(host.BeginNativeRenderDeviceWindowObservation() && stats.requests == requestsAfterEnd + 1,
                  "a separate fresh V6 allocation can explicitly request a new one-shot device result");
            auto next = sentinel;
            Check(host.TakeNativeRenderDeviceWindowLease(next) && next.device && next.generation > transferred.generation && stats.transfers == 2,
                  "new explicit lease transfers current generation provenance instead of cached prior ownership");
            Check(FinishDeviceObservation(host), "new explicit device generation completes cleanup");
        }
        const auto finalTakes = stats.takes;
        output = sentinel;
        Check(!host.TakeNativeRenderDeviceWindowLease(output) && SameDeviceWindowOutput(output, sentinel) && stats.takes == finalTakes,
              "completed device observer leaves no stale function or output cache");
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && !stats.liveLeases && stats.allocations == stats.releases && lifetime[0] == 1,
              "all controlled device allocations release before adapter shutdown and module unload");
    }

    void TestRenderProviderV6(CCoreInterface* owner, const std::wstring& path, unsigned mode, const std::wstring& loaderProbePath)
    {
        using namespace mta::game_de;
        CGameAdapterModule host;
        Check(host.Start(owner, path), "own controlled separately versioned API6 provider");
        RenderEndpointDiagnosticV1 diagnostic;
        diagnostic.version = 77;
        Check(!host.GetNativeRenderEndpointDiagnostic(diagnostic) && diagnostic.version == 77, "endpoint diagnostics cannot invent a V6 lease before Begin");
        RenderSetupDiagnosticV1 setup;
        setup.version = 77;
        Check(!host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77, "setup diagnostics cannot invent a V6 lease before Begin");
        RenderRootsDiagnosticV1 roots;
        roots.version = 77;
        Check(!host.GetNativeRenderRootsDiagnostic(roots) && roots.version == 77, "roots diagnostics cannot invent a V6 lease before Begin");
        std::uint32_t counters[14]{};
        using Configure = void (*)(std::uint32_t*) noexcept;
        Configure configure = nullptr;
        auto      address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFacadeFixtureCounters");
        std::memcpy(&configure, &address, sizeof(configure));
        Check(configure != nullptr, "API6 controlled lifetime counters available");
        configure(counters);
        using Observe = void (*)(void (*)() noexcept) noexcept;
        Observe observe = nullptr;
        address = GetProcAddress(GetModuleHandleW(path.c_str()), "SetFixtureV6OperationObserver");
        std::memcpy(&observe, &address, sizeof(observe));
        Check(observe != nullptr, "owned API6 provider can mark the exact post-operation allocation boundary");
        using SetupReads = std::uint32_t (*)() noexcept;
        SetupReads setupReads = nullptr;
        address = GetProcAddress(GetModuleHandleW(path.c_str()), "GetFixtureSetupDiagnosticReadsV6");
        std::memcpy(&setupReads, &address, sizeof(setupReads));
        Check(setupReads != nullptr, "owned API6 provider counts setup calls even after exact lease Release");
        using Factory = decltype(&CreateGameRenderObservationV6);
        Factory factory = nullptr;
        address = GetProcAddress(GetModuleHandleW(path.c_str()), "CreateGameRenderObservationV6");
        std::memcpy(&factory, &address, sizeof(factory));
        Check(factory && !factory(5, owner, RenderObservationRoute::D3D12) && !factory(7, owner, RenderObservationRoute::D3D12) &&
                  !factory(6, nullptr, RenderObservationRoute::D3D12) && !factory(6, owner, RenderObservationRoute::D3D11),
              "API6 factory rejects wrong version/owner and earlier route");
        RenderObservationStateV6 output;
        output.base.base.base.gpu.queryStage = 99;
        output.base.base.custom.status = 99;
        Check(!host.BeginNativeRenderDispatchObservation(RenderObservationRoute::Unknown) &&
                  !host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D11) && !host.GetNativeRenderObservationState(output) &&
                  output.base.base.base.gpu.queryStage == 99 && output.base.base.custom.status == 99 && !counters[4],
              "invalid API6 route rejects before provider entry and preserves output");
        observedCoreAllocations.store(0);
        observe(&ArmCoreAllocationObservation);
        const bool began = host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12);
        observeCoreAllocations.store(false);
        observe(nullptr);
        Check(began == (mode != 2), "API6 failed Begin remains distinguishable and cleanup-owned");
        Check(observedCoreAllocations.load() == 0, "API6 post-Begin status formatting performs no consumer CRT allocation");
        Check(!host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && !host.BeginNativeRenderObservation() &&
                  !host.BeginNativeRenderObservation(RenderObservationRoute::D3D12) &&
                  !host.BeginNativeRenderOwnershipObservation(RenderObservationRoute::D3D12) &&
                  !host.BeginNativeRenderClassificationObservation(RenderObservationRoute::D3D12) && counters[4] == 1,
              "retained API6 lease excludes every renderer version and duplicate Begin");
        Check(host.GetNativeRenderObservationState(output) && output.base.base.base.base.requestedRoute == 2,
              "API6 reports its immutable explicit route through its own state layout");
        const bool diagnosticAvailable = mode != 6 && mode != 8;
        observedCoreAllocations.store(0);
        observeCoreAllocations.store(true);
        const bool copiedDiagnostic = host.GetNativeRenderEndpointDiagnostic(diagnostic);
        observeCoreAllocations.store(false);
        Check(observedCoreAllocations.load() == 0, "API6 missing/wrong-version optional diagnostic formatting performs no consumer CRT allocation");
        Check(copiedDiagnostic == diagnosticAvailable && (diagnosticAvailable ? diagnostic.version == 1 && diagnostic.generationLow == 17 &&
                                                                                    diagnostic.slotOwnerClass == 3 && diagnostic.slotRva == 0x1234
                                                                              : diagnostic.version == 77),
              "optional endpoint export preserves known scalars while missing/wrong-version providers preserve caller output");
        if (mode == 6)
            Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "ReadGameRenderEndpointDiagnosticV2"),
                  "historical API6 fixture genuinely has no separate diagnostic export");
        observedCoreAllocations.store(0);
        observeCoreAllocations.store(true);
        const bool copiedSetup = host.GetNativeRenderSetupDiagnostic(setup);
        observeCoreAllocations.store(false);
        Check(observedCoreAllocations.load() == 0 && copiedSetup == diagnosticAvailable &&
                  (diagnosticAvailable
                       ? setup.version == 1 && setup.generationLow == 1 && setup.attempted == 1 && !setup.cleanupAttempted && !setup.cleanupComplete &&
                             setup.stage == (mode == 2 ? static_cast<std::uint32_t>(RenderSetupStage::NativeThreadStubMatch)
                                                       : static_cast<std::uint32_t>(RenderSetupStage::Complete)) &&
                             setup.failureStage == (mode == 2 ? static_cast<std::uint32_t>(RenderSetupStage::NativeThreadStubMatch) : 0u)
                       : setup.version == 77),
              "setup diagnostics survive failed Begin without consumer allocations, while missing/wrong-version outputs stay untouched");
        if (mode == 6)
            Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "ReadGameRenderSetupDiagnosticV1"),
                  "historical API6 provider genuinely omits the additive setup export");
        const bool rootsAvailable = diagnosticAvailable && mode != 2;
        observedCoreAllocations.store(0);
        observeCoreAllocations.store(true);
        const bool copiedRoots = host.GetNativeRenderRootsDiagnostic(roots);
        observeCoreAllocations.store(false);
        Check(observedCoreAllocations.load() == 0 && copiedRoots == rootsAvailable &&
                  (rootsAvailable ? roots.version == 1 && roots.generationLow == 1 && roots.checkpoint == 26 && roots.refusalReason == 3 : roots.version == 77),
              "missing, wrong-version or unpublished roots diagnostics preserve output without consumer allocations");
        if (mode == 6)
            Check(!GetProcAddress(GetModuleHandleW(path.c_str()), "ReadGameRenderRootsDiagnosticV1"),
                  "historical API6 provider genuinely omits the optional roots export");
        if (rootsAvailable)
        {
            counters[13] = 4;
            roots.version = 77;
            Check(!host.GetNativeRenderRootsDiagnostic(roots) && roots.version == 77,
                  "an unavailable current roots reader cannot replay a previously successful diagnostic");
            counters[13] = 0;
        }
        if (diagnosticAvailable)
        {
            counters[13] = 4;
            setup.version = 77;
            Check(!host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77,
                  "a retained provider's unavailable setup read cannot expose earlier cached evidence");
            counters[13] = 0;
        }
        if (mode == 1)
        {
            wchar_t enteredName[96]{}, resumeName[96]{}, timeoutName[96]{};
            swprintf_s(enteredName, L"Local\\MtaApi6DiagnosticLoaderEntered-%lu", GetCurrentProcessId());
            swprintf_s(resumeName, L"Local\\MtaApi6DiagnosticLoaderResume-%lu", GetCurrentProcessId());
            swprintf_s(timeoutName, L"Local\\MtaApi6DiagnosticLoaderTimeout-%lu", GetCurrentProcessId());
            const auto entered = CreateEventW(nullptr, TRUE, FALSE, enteredName);
            const auto resume = CreateEventW(nullptr, TRUE, FALSE, resumeName);
            const auto timeout = CreateEventW(nullptr, TRUE, FALSE, timeoutName);
            Check(entered && resume && timeout, "owned loader-lock fixture gates created");
            HMODULE     loadedProbe = nullptr;
            std::thread loader(
                [&] { loadedProbe = LoadLibraryExW(loaderProbePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS); });
            const bool held = WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0;
            const auto started = GetTickCount64();
            const bool copied = held && host.GetNativeRenderEndpointDiagnostic(diagnostic);
            const bool copiedSetupUnderLoaderLock = held && host.GetNativeRenderSetupDiagnostic(setup);
            const bool copiedRootsUnderLoaderLock = held && host.GetNativeRenderRootsDiagnostic(roots);
            const auto elapsed = GetTickCount64() - started;
            SetEvent(resume);
            loader.join();
            const bool timedOut = WaitForSingleObject(timeout, 0) != WAIT_TIMEOUT;
            if (loadedProbe)
                FreeLibrary(loadedProbe);
            CloseHandle(entered);
            CloseHandle(resume);
            CloseHandle(timeout);
            Check(held && loadedProbe && !timedOut && copied && copiedSetupUnderLoaderLock && copiedRootsUnderLoaderLock && roots.version == 1 &&
                      setup.version == 1 && diagnostic.version == 1 && elapsed < 1500,
                  "all cached API6 diagnostic readers complete while another owned provider holds the loader lock");
        }
        Check(mode == 6 ? !output.base.base.base.base.observedRoute && !output.base.base.base.gpu.attempts && !output.base.base.base.gpu.metadataValid
                        : output.base.base.base.base.observedRoute == 2 && output.base.base.base.gpu.attempts == 7 &&
                              output.base.base.base.gpu.queryStage == 19 && output.base.base.base.gpu.accepted == 0 && output.base.base.base.gpu.rejected == 7,
              "API6 copies actual controlled zero-query or custom-unsupported metadata without promotion");
        RenderObservationState   legacy;
        RenderObservationStateV2 older;
        RenderObservationStateV3 previous;
        previous.gpu.queryStage = 99;
        legacy.width = older.requestedRoute = 99;
        Check(!host.GetNativeRenderObservationState(legacy) && !host.GetNativeRenderObservationState(older) &&
                  !host.GetNativeRenderObservationState(previous) && previous.gpu.queryStage == 99 && legacy.width == 99 && older.requestedRoute == 99,
              "API1/2/3 output cannot reinterpret the API6 lease");
        Check(mode == 6 ? output.base.base.custom.status == 0 && output.base.base.custom.implementationId == 0 &&
                              output.base.base.custom.slotSignatureMask == 0 && output.base.base.custom.staticPolicyFlags == 0
              : mode == 8 ? output.base.base.custom.status == 3 && output.base.base.custom.implementationId == 0
                          : output.base.base.custom.status == 2 && output.base.base.custom.implementationId == 1 &&
                                output.base.base.custom.slotSignatureMask == 127 && output.base.base.custom.staticPolicyFlags == 15,
              "API6 classification must preserve unknown/known scalars without promoting GPU support");
        RenderObservationStateV4 classificationOutput;
        classificationOutput.custom.status = 99;
        Check(!host.GetNativeRenderObservationState(classificationOutput) && classificationOutput.custom.status == 99,
              "API4 cannot reinterpret the API6 endpoint lease");
        Check((mode == 6 ? output.base.boundary.status == 0 : output.base.boundary.status == 1) && !output.base.boundary.endpointProfileId &&
                  !output.base.boundary.presentEntries && !output.base.boundary.factsMask,
              "API6 coherent native dispatch never upgrades the independently refused strict API5 prefix");
        Check(mode == 6   ? output.nativeDispatch.status == 0 && !output.nativeDispatch.presentEntries
              : mode == 8 ? output.nativeDispatch.status == 1 && !output.nativeDispatch.endpointProfileId
                          : output.nativeDispatch.status == 2 && output.nativeDispatch.endpointProfileId == 2 && output.nativeDispatch.factsMask == 255 &&
                                output.nativeDispatch.presentEntries == 7,
              "API6 dispatch stays separate from the retained API5 DXGI-entry record");
        RenderObservationStateV5 prior;
        prior.boundary.status = 99;
        Check(!host.GetNativeRenderObservationState(prior) && prior.boundary.status == 99 &&
                  !host.BeginNativeRenderFrameBoundaryObservation(RenderObservationRoute::D3D12),
              "API5 cannot reinterpret or replace a retained API6 dispatch lease");
        const auto                 reads = counters[5];
        bool                       foreignBegin = true, foreignRead = true, foreignEnd = true, foreignDiagnostic = true;
        RenderEndpointDiagnosticV1 foreignDiagnosticOutput;
        foreignDiagnosticOutput.version = 88;
        RenderSetupDiagnosticV1 foreignSetupOutput;
        foreignSetupOutput.version = 88;
        bool                     foreignSetup = true;
        RenderObservationStateV6 foreignOutput;
        foreignOutput.base.base.base.gpu.queryStage = 77;
        foreignOutput.base.base.custom.status = 77;
        foreignOutput.nativeDispatch.status = 77;
        foreignOutput.nativeDispatch.presentEntries = 0xfedcba9876543210ull;
        std::thread foreign(
            [&]
            {
                foreignBegin = host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12);
                foreignRead = host.GetNativeRenderObservationState(foreignOutput);
                foreignDiagnostic = host.GetNativeRenderEndpointDiagnostic(foreignDiagnosticOutput);
                foreignSetup = host.GetNativeRenderSetupDiagnostic(foreignSetupOutput);
                foreignEnd = host.EndNativeRenderObservation();
                host.Stop();
            });
        foreign.join();
        Check(!foreignSetup && foreignSetupOutput.version == 88, "a foreign Core thread cannot replace setup diagnostic output");
        Check(!foreignDiagnostic && foreignDiagnosticOutput.version == 88,
              "a foreign Core thread cannot inspect an optional endpoint diagnostic or replace output");
        Check(!foreignBegin && !foreignRead && !foreignEnd && foreignOutput.base.base.base.gpu.queryStage == 77 &&
                  foreignOutput.base.base.custom.status == 77 && foreignOutput.nativeDispatch.status == 77 &&
                  foreignOutput.nativeDispatch.presentEntries == 0xfedcba9876543210ull && counters[5] == reads && !counters[6] && !counters[0],
              "foreign API6 calls cannot publish output or touch the provider lifetime");
        if (mode == 7)
        {
            counters[13] = 1;
            output.base.base.base.gpu.queryStage = 88;
            output.base.base.custom.status = 88;
            Check(!host.GetNativeRenderObservationState(output) && output.base.base.base.gpu.queryStage == 88 && output.base.base.custom.status == 88,
                  "cross-observed route preserves API6 caller output");
            counters[13] = 2;
            Check(!host.GetNativeRenderObservationState(output) && output.base.base.base.gpu.queryStage == 88 && output.base.base.custom.status == 88,
                  "cross-requested route preserves API6 caller output");
        }
        if (mode == 4)
        {
            Check(host.Reset() && counters[6] == 1 && counters[7] == 1 && counters[3] == 1 && counters[9] < counters[10],
                  "API6 End/Release precede adapter Reset");
            output.base.base.base.gpu.queryStage = 88;
            output.base.base.custom.status = 88;
            Check(!host.GetNativeRenderObservationState(output) && output.base.base.base.gpu.queryStage == 88 && output.base.base.custom.status == 88,
                  "Reset clears API6 final state availability");
            diagnostic.version = 77;
            Check(!host.GetNativeRenderEndpointDiagnostic(diagnostic) && diagnostic.version == 77, "Reset clears cached optional diagnostic availability too");
            setup.version = 77;
            Check(!host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77, "Reset clears the setup generation and cached export");
        }
        else
        {
            observedCoreAllocations.store(0);
            observe(&ArmCoreAllocationObservation);
            bool retainedEnd = false;
            if (mode <= 2 || mode == 7)
                host.Stop();
            else if (mode == 3 || mode == 5)
                retainedEnd = !host.EndNativeRenderObservation();
            observeCoreAllocations.store(false);
            observe(nullptr);
            Check(observedCoreAllocations.load() == 0, "API6 retained End/read failure formatting performs no consumer CRT allocation");
            if (mode == 3 || mode == 5)
                Check(retainedEnd, "failed API6 final read or active end state retains the lease");
            if (mode <= 3 || mode == 5 || mode == 7)
                Check(host.IsStarted() && counters[6] == 1 && !counters[7] && !counters[0] && GetModuleHandleW(path.c_str()),
                      "API6 Begin/End/read/route failure retains adapter and module for retry");
            counters[13] = 0;
            Check(host.EndNativeRenderObservation(), "owner completes API6 End after the retained failure");
            diagnostic.version = 77;
            Check(host.GetNativeRenderEndpointDiagnostic(diagnostic) == diagnosticAvailable &&
                      (diagnosticAvailable ? diagnostic.version == 1 && diagnostic.slotRva == 0x1234 : diagnostic.version == 77),
                  "safe End preserves only successfully copied optional diagnostics after provider Release");
            const auto setupCallsAfterRelease = setupReads();
            setup.version = 77;
            Check(host.GetNativeRenderSetupDiagnostic(setup) == diagnosticAvailable &&
                      (diagnosticAvailable ? setup.version == 1 && setup.generationLow == 1 && setup.cleanupAttempted == 1 && setup.cleanupComplete == 1
                                           : setup.version == 77) &&
                      setupReads() == setupCallsAfterRelease,
                  "safe End copies fresh cleanup0-to1 facts before Release; final cache never calls the released provider");
            const auto finalReads = counters[5];
            Check(host.GetNativeRenderObservationState(output) && !output.base.base.base.base.base.active && !output.base.base.base.base.base.pendingCleanup &&
                      counters[5] == finalReads && counters[7] == 1 && output.base.base.base.gpu.queryStage == (mode == 6 ? 0u : 19u),
                  "ended API6 cache contains actual final metadata without touching released provider");
            Check((mode == 6 ? output.nativeDispatch.status == 0
                   : mode == 8
                       ? output.nativeDispatch.status == 1
                       : output.nativeDispatch.status == 2 && output.nativeDispatch.endpointProfileId == 2 && output.nativeDispatch.presentEntries == 7) &&
                      !output.base.boundary.endpointProfileId,
                  "safe API6 End retains its separate dispatch tail while the old DXGI-entry prefix remains unadmitted");
            if (mode != 6)
                Check(!output.base.base.base.gpu.supportPredicate && output.base.base.base.gpu.queryStatus == 2 && output.base.base.base.gpu.rejected == 7 &&
                          !host.IsGameplayReady(),
                      "unsupported GPU metadata remains unsupported after End and grants no gameplay readiness");
            Check(output.base.base.custom.status == (mode == 6   ? 0u
                                                     : mode == 8 ? 3u
                                                                 : 2u),
                  "API6 final cache preserves classification before releasing its provider");
            Check(host.EndNativeRenderObservation() && counters[7] == 1, "API6 completed End is idempotent");
            counters[13] = 3;
            setup.version = 77;
            diagnostic.version = 77;
            output.base.base.custom.status = 99;
            Check(!host.BeginNativeRenderDispatchObservation(RenderObservationRoute::D3D12) && !host.GetNativeRenderEndpointDiagnostic(diagnostic) &&
                      diagnostic.version == 77 && !host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77 &&
                      !host.GetNativeRenderObservationState(output) && output.base.base.custom.status == 99,
                  "a rejected new V6 factory cannot relabel the completed run's cached diagnostic or observation");
            counters[13] = 0;
        }
        host.Stop();
        Check(!host.IsStarted() && !GetModuleHandleW(path.c_str()) && counters[7] == 1 && counters[0] == 1 && counters[1] == 1 && counters[9] < counters[11],
              "API6 observer release precedes adapter shutdown and unload");
        output.base.base.base.gpu.queryStage = 99;
        output.base.base.custom.status = 99;
        Check(!host.GetNativeRenderObservationState(output) && output.base.base.base.gpu.queryStage == 99 && output.base.base.custom.status == 99,
              "unloaded API6 cannot replace caller output");
        diagnostic.version = 77;
        Check(!host.GetNativeRenderEndpointDiagnostic(diagnostic) && diagnostic.version == 77,
              "unloaded provider cannot expose a cached diagnostic through a new owner");
        setup.version = 77;
        Check(!host.GetNativeRenderSetupDiagnostic(setup) && setup.version == 77,
              "safe Stop clears setup availability instead of carrying a prior owner into the next Start");
    }
    #endif
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc != 2 && argc != 3 && argc != 6 && argc != 11 && argc != 18 && argc != 26 && argc != 34 && argc != 42 && argc != 50 && argc != 54 && argc != 58)
            throw std::runtime_error("Pass the absolute path of the built Game DE module");
        const std::wstring modulePath = argv[1];
        // The adapter stores this identity but must never dereference it. This
        // is an opaque ownership fixture, not a constructed or running CCore.
        alignas(void*) unsigned char ownerIdentity[sizeof(void*)]{};
        auto*                        owner = reinterpret_cast<CCoreInterface*>(ownerIdentity);
    #if defined(_WIN64)
        TestRejectedArguments(owner, modulePath);
        TestActualModule(owner, modulePath);
        TestAdjacentPath();
        if (argc >= 3)
            TestFailedShutdownRetention(owner, argv[2]);
        if (argc >= 6)
            for (unsigned mode = 1; mode <= 3; ++mode)
                TestFacadeProvider(owner, argv[mode + 2], mode);
        if (argc >= 11)
            for (unsigned mode = 1; mode <= 5; ++mode)
                TestRenderProvider(owner, argv[mode + 5], mode);
        if (argc >= 18)
        {
            TestMissingRenderV2(owner, argv[6]);
            for (unsigned mode = 1; mode <= 7; ++mode)
                for (const auto route : {mta::game_de::RenderObservationRoute::D3D11, mta::game_de::RenderObservationRoute::D3D12})
                    TestRenderProviderV2(owner, argv[mode + 10], mode, route);
        }
        if (argc >= 26)
        {
            TestMissingRenderV3(owner, argv[11]);
            for (unsigned mode = 1; mode <= 8; ++mode)
                TestRenderProviderV3(owner, argv[mode + 17], mode);
        }
        if (argc >= 34)
        {
            TestMissingRenderV4(owner, argv[18]);
            for (unsigned mode = 1; mode <= 8; ++mode)
                TestRenderProviderV4(owner, argv[mode + 25], mode);
        }
        if (argc >= 42)
        {
            TestMissingRenderV5(owner, argv[26]);
            for (unsigned mode = 1; mode <= 8; ++mode)
                TestRenderProviderV5(owner, argv[mode + 33], mode);
        }
        if (argc >= 50)
        {
            TestMissingRenderV6(owner, argv[34]);
            for (unsigned mode = 1; mode <= 8; ++mode)
                TestRenderProviderV6(owner, argv[mode + 41], mode, argv[49]);
            TestSetupDiagnosticGeneration(owner, argv[45]);
            TestRootsDiagnosticGeneration(owner, argv[45]);
            for (unsigned mode = 1; mode <= 8; ++mode)
                TestDeviceLeaseProvider(owner, argv[mode + 41], mode, argv[49]);
            for (unsigned mode = 1; mode <= 8; ++mode)
                TestDeviceWindowLeaseProvider(owner, argv[mode + 41], mode, argv[49]);
        }
        if (argc >= 54)
            for (unsigned mode = 1; mode <= 4; ++mode)
                TestControlledClockProvider(owner, argv[mode + 49], mode);
        if (argc == 58)
            for (unsigned mode = 1; mode <= 4; ++mode)
                TestControlledLocalInputProvider(owner, argv[mode + 53], mode);
    #else
        CGameAdapterModule host;
        Check(!host.Start(owner, modulePath), "x86 must reject the x64 backend before loading it");
        Check(!host.IsStarted() && GetModuleHandleW(modulePath.c_str()) == nullptr, "wrong architecture cannot create or load Game DE");
        Check(host.GetError().find("x64") != std::string::npos, "architecture rejection is explicit");
    #endif
        std::cout << "Passed " << checks << " MTA game adapter host checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MTA game adapter host failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
#endif
