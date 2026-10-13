/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the actual MTA Core-owned Game DE module consumer
 *****************************************************************************/

#include "../../Client/core/CGameAdapterModule.h"

#include <filesystem>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <windows.h>

#if defined(MTA_TEST_SHUTDOWN_FAILURE_MODULE)
    #ifndef MTA_TEST_FACADE_MODE
        #define MTA_TEST_FACADE_MODE 0
    #endif
    #ifndef MTA_TEST_RENDER_MODE
        #define MTA_TEST_RENDER_MODE 0
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
    }

    extern "C" __declspec(dllexport) IGameAdapter* CreateGameAdapter(std::uint32_t abiVersion) noexcept
    {
        return abiVersion == AdapterAbiVersion ? new (std::nothrow) ShutdownFailureAdapter : nullptr;
    }

    extern "C" __declspec(dllexport) void SetFacadeFixtureCounters(std::uint32_t* counters) noexcept
    {
        fixtureCounters = counters;
    }

    extern "C" __declspec(dllexport) IGameRenderObservation* CreateGameRenderObservation(std::uint32_t apiVersion, CCoreInterface* owner) noexcept
    {
        return MTA_TEST_RENDER_MODE && apiVersion == RenderObservationApiVersion && owner ? new (std::nothrow) RenderFixture : nullptr;
    }
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
        std::uint32_t counters[13]{};
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
        std::uint32_t counters[13]{};
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
    #endif
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc != 2 && argc != 3 && argc != 6 && argc != 11)
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
        if (argc == 11)
            for (unsigned mode = 1; mode <= 5; ++mode)
                TestRenderProvider(owner, argv[mode + 5], mode);
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
