/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the actual MTA Core-owned Game DE module consumer
 *****************************************************************************/

#include "../../Client/core/CGameAdapterModule.h"

#include <filesystem>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <windows.h>

#if defined(MTA_TEST_SHUTDOWN_FAILURE_MODULE)
// This deliberately failing provider tests Core's DLL lifetime only. It is not
// an engine writer, replacement Game DE implementation or gameplay fixture.
namespace mta::game_de
{
    namespace
    {
        class ShutdownFailureAdapter final : public IGameAdapter
        {
        public:
            Status        Initialize(CCoreInterface*) noexcept override { return Status::Ok; }
            Status        Pulse() noexcept override { return Status::Ok; }
            Status        Reset() noexcept override { return Status::Unsupported; }
            Status        Shutdown() noexcept override { return ++shutdownCount == 1 ? Status::Unsupported : Status::Ok; }
            void          Release() noexcept override { delete this; }
            std::uint64_t GetCapabilities() const noexcept override { return 0; }
            CGame*        GetGameInterface() const noexcept override { return nullptr; }
            Status        PublishLocalObservation(const LocalObservation&) noexcept override { return Status::Unsupported; }
            Status        GetLocalObservation(LocalObservation&) noexcept override { return Status::Unsupported; }
            Status        SubmitControlActions(const ControlActionFrame&) noexcept override { return Status::Unsupported; }
            Status        GetMtaInputState(MtaInputState&) noexcept override { return Status::Unsupported; }
            Status        BindNativeInput() noexcept override { return Status::Unsupported; }
            Status        ReadNativeControllerState(std::uint32_t, NativeControllerState&) noexcept override { return Status::Unsupported; }
            Status        ReadNativeDeviceState(NativeDeviceState&) noexcept override { return Status::Unsupported; }
            Status        BeginNativeMovementTrial() noexcept override { return Status::Unsupported; }
            Status        GetNativeMovementTrialState(NativeMovementTrialState&) noexcept override { return Status::Unsupported; }
            Status        EndNativeMovementTrial() noexcept override { return Status::Unsupported; }

        private:
            unsigned int shutdownCount = 0;
        };
    }

    extern "C" __declspec(dllexport) IGameAdapter* CreateGameAdapter(std::uint32_t abiVersion) noexcept
    {
        return abiVersion == AdapterAbiVersion ? new (std::nothrow) ShutdownFailureAdapter : nullptr;
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
            const auto deviceOriginal = device.sources;
            Check(!host.ReadNativeDeviceState(device) && device.sources == deviceOriginal,
                  "Core cannot read unbound raw device sources or replace caller output");
            Check(!host.ReadNativeControllerState(0, native) && native.current.ButtonCross == 23,
                  "Core cannot read native input without explicit game binding");
            Check(!host.BindNativeInput(), "the real module rejects native binding in this non-game host");
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
    #endif
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc != 2 && argc != 3)
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
        if (argc == 3)
            TestFailedShutdownRetention(owner, argv[2]);
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
