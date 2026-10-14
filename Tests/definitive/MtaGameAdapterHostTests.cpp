/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the actual MTA Core-owned Game DE module consumer
 *****************************************************************************/

#include "../../Client/core/CGameAdapterModule.h"

#include <filesystem>
#include <cstring>
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
static_assert(offsetof(mta::game_de::RenderObservationStateV3, base) == 0);
static_assert(offsetof(mta::game_de::RenderObservationStateV3, gpu) == sizeof(mta::game_de::RenderObservationStateV2));

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
// This deliberately failing provider tests Core's DLL lifetime only. It is not
// an engine writer, replacement Game DE implementation or gameplay fixture.
namespace mta::game_de
{
    namespace
    {
        std::uint32_t* fixtureCounters = nullptr;
        alignas(void*) unsigned char facadeIdentity[sizeof(void*)]{};

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
                return MTA_TEST_RENDER_MODE ? Status::Ok : Status::Unsupported;
            }
            Status Shutdown() noexcept override
            {
                ++shutdownCount;
                if (fixtureCounters)
                {
                    ++fixtureCounters[0];
                    fixtureCounters[11] = ++fixtureCounters[8];
                }
                return !MTA_TEST_RENDER_MODE && (MTA_TEST_FACADE_MODE == 0 || MTA_TEST_FACADE_MODE == 3) && shutdownCount == 1 ? Status::Unsupported
                                                                                                                               : Status::Ok;
            }
            void Release() noexcept override
            {
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
            Status SubmitControlActions(const ControlActionFrame&) noexcept override { return Status::Unsupported; }
            Status GetMtaInputState(MtaInputState&) noexcept override { return Status::Unsupported; }
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
    }

    extern "C" IGameAdapter* CreateGameAdapter(std::uint32_t abiVersion) noexcept
    {
        return abiVersion == AdapterAbiVersion ? new (std::nothrow) ShutdownFailureAdapter : nullptr;
    }

    extern "C" __declspec(dllexport) void SetFacadeFixtureCounters(std::uint32_t* counters) noexcept
    {
        fixtureCounters = counters;
    }

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
}
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

    void TestFailedShutdownRetention(CCoreInterface* owner, const std::wstring& fixturePath)
    {
        Check(GetModuleHandleW(fixturePath.c_str()) == nullptr, "cleanup fixture begins unloaded");
        CGameAdapterModule host;
        Check(host.Start(owner, fixturePath), "load an explicit provider whose first Shutdown fails");
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
    #endif
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc != 2 && argc != 3 && argc != 6 && argc != 11 && argc != 18 && argc != 26)
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
        if (argc == 26)
        {
            TestMissingRenderV3(owner, argv[11]);
            for (unsigned mode = 1; mode <= 8; ++mode)
                TestRenderProviderV3(owner, argv[mode + 17], mode);
        }
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
