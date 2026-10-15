/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the MTA-owned DE module contract without engine bindings
 *****************************************************************************/

#include <game/CGameAdapter.h>
#include <game/CGameRenderObservation.h>
#include <game/CGameRenderEndpointDiagnostic.h>
#include <game/CGameRenderSetupDiagnostic.h>
#include <game/CGameRenderRootsDiagnostic.h>
#include <game/CGameRenderDeviceLease.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <windows.h>

namespace
{
    using namespace mta::game_de;
    constexpr auto implementedCapabilities =
        static_cast<std::uint64_t>(AdapterCapability::ObservationIngest) | static_cast<std::uint64_t>(AdapterCapability::MtaInputSynthesis);
    int checks = 0;

    void Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }

    struct OwnerIdentity
    {
        // The backend must never dereference this deliberately opaque identity
        // as a constructed Core or claim that the tests run a complete MTA host.
        alignas(void*) std::array<unsigned char, sizeof(void*)> storage{};
        CCoreInterface* Get() noexcept { return reinterpret_cast<CCoreInterface*>(storage.data()); }
    };

    struct OwnedAdapter
    {
        IGameAdapter* value = CreateGameAdapter(AdapterAbiVersion);
        ~OwnedAdapter()
        {
            if (value)
                value->Release();
        }
    };

    const std::array<signed short CControllerState::*, 24> fields = {&CControllerState::LeftStickX,
                                                                     &CControllerState::LeftStickY,
                                                                     &CControllerState::RightStickX,
                                                                     &CControllerState::RightStickY,
                                                                     &CControllerState::LeftShoulder1,
                                                                     &CControllerState::LeftShoulder2,
                                                                     &CControllerState::RightShoulder1,
                                                                     &CControllerState::RightShoulder2,
                                                                     &CControllerState::DPadUp,
                                                                     &CControllerState::DPadDown,
                                                                     &CControllerState::DPadLeft,
                                                                     &CControllerState::DPadRight,
                                                                     &CControllerState::Start,
                                                                     &CControllerState::Select,
                                                                     &CControllerState::ButtonSquare,
                                                                     &CControllerState::ButtonTriangle,
                                                                     &CControllerState::ButtonCross,
                                                                     &CControllerState::ButtonCircle,
                                                                     &CControllerState::ShockButtonL,
                                                                     &CControllerState::ShockButtonR,
                                                                     &CControllerState::m_bChatIndicated,
                                                                     &CControllerState::m_bPedWalk,
                                                                     &CControllerState::m_bVehicleMouseLook,
                                                                     &CControllerState::m_bRadioTrackSkip};

    LocalObservation Observation(std::uint64_t time = 1234)
    {
        LocalObservation value;
        value.sampleAtMs = time;
        value.localPedHandle = 7;
        value.transform = {{-12.5f, 23.25f, -3.75f}, 285.5f};
        value.rawVelocity = {-0.125f, 4.25f, -2.5f};
        return value;
    }

    void SameObservation(const LocalObservation& actual, const LocalObservation& expected)
    {
        Check(actual.sampleAtMs == expected.sampleAtMs && actual.localPedHandle == expected.localPedHandle, "retain the source clock and entity identity");
        Check(actual.transform.position.x == expected.transform.position.x && actual.transform.position.y == expected.transform.position.y &&
                  actual.transform.position.z == expected.transform.position.z && actual.transform.heading == expected.transform.heading,
              "retain observed pose without driving the engine");
        Check(
            actual.rawVelocity.x == expected.rawVelocity.x && actual.rawVelocity.y == expected.rawVelocity.y && actual.rawVelocity.z == expected.rawVelocity.z,
            "retain raw velocity without unit conversion");
        Check(actual.controllerKnownMask == expected.controllerKnownMask, "controller coverage belongs to the caller evidence");
        for (const auto field : fields)
            Check(actual.controller.*field == expected.controller.*field, "typed canonical fields round-trip without array aliasing");
    }

    void TestFactoryAndLifecycle()
    {
        Check(CreateGameAdapter(0) == nullptr && CreateGameAdapter(1) == nullptr && CreateGameAdapter(2) == nullptr && CreateGameAdapter(3) == nullptr &&
                  CreateGameAdapter(4) == nullptr && CreateGameAdapter(5) == nullptr && CreateGameAdapter(AdapterAbiVersion + 1) == nullptr,
              "reject old and unknown factory ABIs before exposing the changed vtable");
        OwnedAdapter adapter;
        Check(adapter.value != nullptr, "matching ABI creates a module-owned adapter");
        Check(adapter.value->GetCapabilities() == implementedCapabilities, "only observation ingestion and MTA action synthesis are implemented");
        Check(adapter.value->GetGameInterface() == nullptr, "do not fabricate the full CGame interface");
        auto output = Observation(999);
        Check(adapter.value->Pulse() == Status::NotInitialized && adapter.value->Reset() == Status::NotInitialized &&
                  adapter.value->Shutdown() == Status::NotInitialized,
              "lifecycle requires explicit initialization");
        Check(adapter.value->PublishLocalObservation(Observation()) == Status::NotInitialized &&
                  adapter.value->GetLocalObservation(output) == Status::NotInitialized,
              "storage is unavailable before initialization");
        Check(output.sampleAtMs == 999, "failed observation reads leave caller output untouched");
        Check(adapter.value->Initialize(nullptr) == Status::InvalidArgument, "a null owner never initializes the adapter");
        OwnerIdentity first, second;
        Check(adapter.value->Initialize(first.Get()) == Status::Ok, "Core identity establishes explicit owner-thread lifetime");
        Check(adapter.value->Initialize(first.Get()) == Status::Ok, "repeating the same owner initialization is idempotent");
        Check(adapter.value->Initialize(second.Get()) == Status::InvalidArgument, "another Core identity cannot replace the owner");
        Check(adapter.value->Pulse() == Status::Ok && adapter.value->GetLocalObservation(output) == Status::NotInitialized,
              "an initialized backend has no fabricated initial observation");
        const auto sample = Observation();
        Check(adapter.value->PublishLocalObservation(sample) == Status::Ok && adapter.value->GetLocalObservation(output) == Status::Ok,
              "publish and read a typed local observation");
        SameObservation(output, sample);
        Check(adapter.value->Reset() == Status::Ok && adapter.value->Pulse() == Status::Ok &&
                  adapter.value->GetLocalObservation(output) == Status::NotInitialized,
              "reset clears observation coverage while retaining the initialized lifetime");
        Check(adapter.value->PublishLocalObservation(sample) == Status::Ok && adapter.value->Shutdown() == Status::Ok,
              "shutdown clears owned observation state");
        Check(adapter.value->Pulse() == Status::NotInitialized && adapter.value->GetLocalObservation(output) == Status::NotInitialized,
              "shutdown prevents further publication/readback until reinitialization");
        Check(adapter.value->Initialize(first.Get()) == Status::Ok && adapter.value->GetLocalObservation(output) == Status::NotInitialized,
              "same owner may start a clean lifetime after shutdown");
    }

    void TestCoverageAndIsolation()
    {
        OwnerIdentity owner;
        OwnedAdapter  first, second;
        Check(first.value && second.value && first.value != second.value, "factory allocations have independent ownership");
        Check(first.value->Initialize(owner.Get()) == Status::Ok && second.value->Initialize(owner.Get()) == Status::Ok,
              "independent module instances can share a host identity");
        auto expected = Observation();
        expected.controllerKnownMask = AdapterControllerMask;
        for (std::size_t index = 0; index < 20; ++index)
            expected.controller.*fields[index] = static_cast<signed short>(index % 2 ? 32767 - static_cast<int>(index) : -32768 + static_cast<int>(index));
        Check(first.value->PublishLocalObservation(expected) == Status::Ok, "accept all twenty explicitly supplied signed-int16 fields");
        LocalObservation output;
        Check(first.value->GetLocalObservation(output) == Status::Ok, "read the supplied canonical controller evidence");
        SameObservation(output, expected);
        Check(second.value->GetLocalObservation(output) == Status::NotInitialized, "observations never leak between adapter instances");
        auto noControls = Observation(2000);
        Check(first.value->PublishLocalObservation(noControls) == Status::Ok && first.value->GetLocalObservation(output) == Status::Ok,
              "a pose-only observation explicitly discards earlier controller coverage");
        SameObservation(output, noControls);
        Check(first.value->GetCapabilities() == implementedCapabilities && first.value->GetGameInterface() == nullptr,
              "publishing controls does not enable engine input or gameplay capabilities");
    }

    void TestInvalidObservations()
    {
        OwnerIdentity owner;
        OwnedAdapter  adapter;
        Check(adapter.value && adapter.value->Initialize(owner.Get()) == Status::Ok, "prepare invalid-evidence fixture");
        const auto good = Observation();
        Check(adapter.value->PublishLocalObservation(good) == Status::Ok, "retain an independently specified earlier observation");
        const auto invalid = [&](LocalObservation value)
        {
            Check(adapter.value->PublishLocalObservation(value) == Status::InvalidArgument, "reject invalid evidence atomically");
            LocalObservation output;
            Check(adapter.value->GetLocalObservation(output) == Status::Ok, "an invalid observation cannot clear the preceding publication");
            SameObservation(output, good);
        };
        auto value = Observation();
        value.localPedHandle = 0;
        invalid(value);
        for (const float nonfinite : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            for (unsigned int component = 0; component < 7; ++component)
            {
                value = Observation();
                switch (component)
                {
                    case 0:
                        value.transform.position.x = nonfinite;
                        break;
                    case 1:
                        value.transform.position.y = nonfinite;
                        break;
                    case 2:
                        value.transform.position.z = nonfinite;
                        break;
                    case 3:
                        value.transform.heading = nonfinite;
                        break;
                    case 4:
                        value.rawVelocity.x = nonfinite;
                        break;
                    case 5:
                        value.rawVelocity.y = nonfinite;
                        break;
                    case 6:
                        value.rawVelocity.z = nonfinite;
                        break;
                }
                invalid(value);
            }
        }
        for (const std::uint32_t mask : {1u, 0xffffeu, 0xffffffu, 0x80000000u})
        {
            value = Observation();
            value.controllerKnownMask = mask;
            invalid(value);
        }
        value = Observation();
        value.controller.ButtonCross = 255;
        invalid(value);
        for (std::size_t index = 20; index < fields.size(); ++index)
        {
            value = Observation();
            value.controllerKnownMask = AdapterControllerMask;
            value.controller.*fields[index] = 1;
            invalid(value);
        }
    }

    void TestThreadOwnership()
    {
        OwnerIdentity owner;
        OwnedAdapter  adapter;
        Check(adapter.value && adapter.value->Initialize(owner.Get()) == Status::Ok, "bind the operational game thread");
        const auto sample = Observation();
        Check(adapter.value->PublishLocalObservation(sample) == Status::Ok, "publish before a foreign-thread attempt");
        std::array<Status, 15> statuses{};
        LocalObservation       foreignOutput = Observation(999);
        MtaInputState          foreignInput;
        foreignInput.current.ButtonCross = 19;
        NativeControllerState foreignNative;
        foreignNative.current.ButtonCross = 23;
        NativeDeviceState foreignDevice;
        foreignDevice.sources.fill(-1234567);
        NativeMovementTrialState foreignTrial;
        foreignTrial.callbackCount = 123;
        NativeMovementEligibility foreignEligibility;
        foreignEligibility.knownMask = 123;
        std::thread other(
            [&]
            {
                statuses = {adapter.value->Initialize(owner.Get()),
                            adapter.value->Pulse(),
                            adapter.value->Reset(),
                            adapter.value->Shutdown(),
                            adapter.value->PublishLocalObservation(Observation(2000)),
                            adapter.value->GetLocalObservation(foreignOutput),
                            adapter.value->SubmitControlActions({MtaInput::ActionBit(SPRINT), {}}),
                            adapter.value->GetMtaInputState(foreignInput),
                            adapter.value->BindNativeInput(),
                            adapter.value->ReadNativeControllerState(0, foreignNative),
                            adapter.value->ReadNativeDeviceState(foreignDevice),
                            adapter.value->BeginNativeMovementTrial(),
                            adapter.value->GetNativeMovementTrialState(foreignTrial),
                            adapter.value->EndNativeMovementTrial(),
                            adapter.value->GetNativeMovementEligibility(foreignEligibility)};
            });
        other.join();
        for (const auto status : statuses)
            Check(status == Status::WrongThread, "foreign threads cannot operate or tear down the adapter");
        Check(foreignOutput.sampleAtMs == 999, "foreign read leaves caller output untouched");
        Check(foreignInput.current.ButtonCross == 19, "foreign logical input read leaves caller output untouched");
        Check(foreignNative.current.ButtonCross == 23, "foreign native read leaves caller output untouched");
        Check(foreignTrial.callbackCount == 123, "foreign trial reads preserve caller metrics without starting or ending hooks");
        Check(foreignEligibility.knownMask == 123, "foreign eligibility reads cannot replace caller context");
        Check(std::all_of(foreignDevice.sources.begin(), foreignDevice.sources.end(), [](std::int32_t value) { return value == -1234567; }),
              "foreign native device read leaves all caller values untouched");
        LocalObservation output;
        Check(adapter.value->GetLocalObservation(output) == Status::Ok, "foreign thread attempts leave the owner publication intact");
        SameObservation(output, sample);
        Check(adapter.value->Shutdown() == Status::Ok, "the owner can still shut down after foreign operations");
    }

    void SameController(const CControllerState& actual, const CControllerState& expected)
    {
        for (const auto field : fields)
            Check(actual.*field == expected.*field, "all logical controller fields retain their independently specified values");
    }

    void TestMtaInput()
    {
        OwnerIdentity owner;
        OwnedAdapter  adapter, isolated;
        Check(adapter.value && isolated.value, "create independent logical input instances");
        MtaInputState output;
        output.current.ButtonCross = 19;
        Check(adapter.value->SubmitControlActions({}) == Status::NotInitialized && adapter.value->GetMtaInputState(output) == Status::NotInitialized &&
                  output.current.ButtonCross == 19,
              "logical input requires initialization and failed reads preserve output");
        Check(adapter.value->Initialize(owner.Get()) == Status::Ok && isolated.value->Initialize(owner.Get()) == Status::Ok,
              "initialize logical input fixtures");
        Check(adapter.value->GetMtaInputState(output) == Status::NotInitialized, "initialization invents no logical frame");
        const auto nativeSample = Observation();
        Check(adapter.value->PublishLocalObservation(nativeSample) == Status::Ok, "seed independent native evidence");
        ControlActionFrame frame;
        frame.heldActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(WALK);
        Check(adapter.value->SubmitControlActions(frame) == Status::Ok && adapter.value->GetMtaInputState(output) == Status::Ok,
              "MTA action state reaches the adapter");
        CControllerState walk;
        walk.LeftStickY = -128;
        walk.m_bPedWalk = 255;
        SameController(output.current, walk);
        SameController(output.previous, CControllerState());

        for (unsigned int bit = 0; bit < 64; ++bit)
        {
            if (MtaInput::KnownActionMask & (std::uint64_t{1} << bit))
                continue;
            auto invalid = frame;
            invalid.heldActions = MtaInput::ActionBit(BACKWARDS) | (std::uint64_t{1} << bit);
            Check(adapter.value->SubmitControlActions(invalid) == Status::InvalidArgument, "undefined actions are rejected before changing history");
        }
        Check(adapter.value->GetMtaInputState(output) == Status::Ok, "invalid action frames retain the preceding state");
        SameController(output.current, walk);
        SameController(output.previous, CControllerState());
        frame.heldActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(BACKWARDS) | MtaInput::ActionBit(SPRINT);
        Check(adapter.value->SubmitControlActions(frame) == Status::Ok && adapter.value->GetMtaInputState(output) == Status::Ok,
              "direction history survives rejected frames");
        CControllerState sprint;
        sprint.LeftStickY = 128;
        sprint.ButtonCross = 255;
        SameController(output.current, sprint);
        SameController(output.previous, walk);
        LocalObservation nativeOutput;
        Check(adapter.value->GetLocalObservation(nativeOutput) == Status::Ok, "logical synthesis retains separate native evidence");
        SameObservation(nativeOutput, nativeSample);
        Check(isolated.value->GetMtaInputState(output) == Status::NotInitialized, "logical input does not leak between instances");
        Check(adapter.value->Pulse() == Status::Ok && adapter.value->GetMtaInputState(output) == Status::Ok, "pulse does not synthesize an unsubmitted frame");
        SameController(output.previous, walk);
        frame.context.isDead = true;
        Check(adapter.value->SubmitControlActions(frame) == Status::Ok && adapter.value->GetMtaInputState(output) == Status::Ok,
              "dead-player frames release digital controls");
        SameController(output.current, CControllerState());
        SameController(output.previous, sprint);

        Check(adapter.value->Reset() == Status::Ok, "reset releases logical state and direction history");
        output.current.ButtonCross = 19;
        Check(adapter.value->GetMtaInputState(output) == Status::NotInitialized && output.current.ButtonCross == 19,
              "reset leaves no frame and preserves a failed reader's output");
        frame.context = {};
        frame.heldActions = MtaInput::ActionBit(FORWARDS);
        Check(adapter.value->SubmitControlActions(frame) == Status::Ok, "seed direction history before shutdown");
        Check(adapter.value->Shutdown() == Status::Ok && adapter.value->GetMtaInputState(output) == Status::NotInitialized &&
                  adapter.value->SubmitControlActions(frame) == Status::NotInitialized,
              "shutdown prevents reads and action application");
        Check(adapter.value->Initialize(owner.Get()) == Status::Ok, "restart a cleared logical input session");
        frame.heldActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(BACKWARDS);
        Check(adapter.value->SubmitControlActions(frame) == Status::Ok && adapter.value->GetMtaInputState(output) == Status::Ok,
              "a fresh session uses original simultaneous-key defaults");
        CControllerState both;
        both.LeftStickY = 128;
        SameController(output.current, both);
        SameController(output.previous, CControllerState());
    }

    void TestNativeInputRejection()
    {
        OwnerIdentity owner;
        OwnedAdapter  adapter;
        Check(adapter.value != nullptr, "create native input rejection fixture");
        NativeControllerState output;
        output.current.ButtonCross = 23;
        output.previous.LeftStickX = -19;
        const auto original = output;
        Check(adapter.value->BindNativeInput() == Status::NotInitialized && adapter.value->ReadNativeControllerState(0, output) == Status::NotInitialized,
              "native binding and reads require Core initialization");
        Check(adapter.value->Initialize(owner.Get()) == Status::Ok, "initialize the native input fixture");
        Check(adapter.value->ReadNativeControllerState(2, output) == Status::InvalidArgument, "reject a pad outside the two-element native array");
        Check(adapter.value->ReadNativeControllerState(0, output) == Status::Unsupported, "native reads require an explicit binding");
        Check(adapter.value->BindNativeInput() == Status::Unsupported, "a test executable cannot bind the pinned commercial game image");
        Check(adapter.value->GetCapabilities() == implementedCapabilities, "failed binding cannot claim engine input read or write capabilities");
        for (std::uint32_t pad = 0; pad < 2; ++pad)
            Check(adapter.value->ReadNativeControllerState(pad, output) == Status::Unsupported, "failed binding keeps both native pads unavailable");
        SameController(output.current, original.current);
        SameController(output.previous, original.previous);
        Check(adapter.value->Reset() == Status::Ok && adapter.value->GetCapabilities() == implementedCapabilities,
              "reset retains only the non-engine capabilities");
        Check(adapter.value->Shutdown() == Status::Ok && adapter.value->BindNativeInput() == Status::NotInitialized, "shutdown prevents late native binding");
    }

    void TestNativeDeviceRejection()
    {
        OwnerIdentity owner;
        OwnedAdapter  adapter;
        Check(adapter.value != nullptr, "create native device fixture");
        NativeDeviceState state;
        state.sources.fill((std::numeric_limits<std::int32_t>::min)());
        state.sources.back() = (std::numeric_limits<std::int32_t>::max)();
        const auto original = state.sources;
        Check(adapter.value->ReadNativeDeviceState(state) == Status::NotInitialized && state.sources == original,
              "device read requires Core initialization and preserves every output");
        Check(adapter.value->Initialize(owner.Get()) == Status::Ok, "initialize device read ownership");
        Check(adapter.value->ReadNativeDeviceState(state) == Status::Unsupported && state.sources == original,
              "unbound device read must not publish a zero-valued invented table");
        Check(adapter.value->BindNativeInput() == Status::Unsupported && adapter.value->ReadNativeDeviceState(state) == Status::Unsupported,
              "test executable cannot acquire device read authority");
        Check(state.sources == original && adapter.value->GetCapabilities() == implementedCapabilities,
              "rejected device capture preserves output and adds no engine/candidate capability");
        Check(adapter.value->Reset() == Status::Ok && adapter.value->ReadNativeDeviceState(state) == Status::Unsupported && state.sources == original,
              "reset clears native binding without publishing device state");
        Check(adapter.value->Shutdown() == Status::Ok && adapter.value->ReadNativeDeviceState(state) == Status::NotInitialized && state.sources == original,
              "shutdown prevents native device reads");
    }

    void TestNativeMovementEligibilityRejection()
    {
        OwnedAdapter              adapter;
        NativeMovementEligibility state;
        state.knownMask = 123;
        state.flags = 456;
        state.rawPedState = 789;
        state.rawDisabledControls = 42;
        state.eligible = true;
        state.context.inVehicle = true;
        const auto unchanged = [&]
        {
            return state.knownMask == 123 && state.flags == 456 && state.rawPedState == 789 && state.rawDisabledControls == 42 && state.eligible &&
                   state.context.inVehicle;
        };
        Check(adapter.value && adapter.value->GetNativeMovementEligibility(state) == Status::NotInitialized && unchanged(),
              "eligibility requires initialization and preserves unknown caller storage");
        OwnerIdentity owner;
        Check(adapter.value->Initialize(owner.Get()) == Status::Ok, "initialize the eligibility owner without binding a game");
        Check(adapter.value->GetNativeMovementEligibility(state) == Status::Unsupported && unchanged(),
              "unbound eligibility cannot fabricate valid native context");
        Check(
            adapter.value->BindNativeInput() == Status::Unsupported && adapter.value->GetNativeMovementEligibility(state) == Status::Unsupported && unchanged(),
            "wrong-image rejection precedes native eligibility reads");
        Check(adapter.value->Reset() == Status::Ok && adapter.value->GetNativeMovementEligibility(state) == Status::Unsupported && unchanged(),
              "reset leaves eligibility unknown until exact native binding");
        Check(adapter.value->Shutdown() == Status::Ok && adapter.value->GetNativeMovementEligibility(state) == Status::NotInitialized && unchanged(),
              "shutdown revokes native context access");
    }

    void TestRenderObservationFactoryRejection()
    {
        OwnerIdentity owner;
        Check(CreateGameRenderObservation(0, owner.Get()) == nullptr && CreateGameRenderObservation(RenderObservationApiVersion + 1, owner.Get()) == nullptr,
              "optional render factory requires its independent exact API version");
        Check(CreateGameRenderObservation(RenderObservationApiVersion, nullptr) == nullptr, "optional render diagnostics require an opaque MTA owner");
        auto* observation = CreateGameRenderObservation(RenderObservationApiVersion, owner.Get());
        Check(observation != nullptr, "actual module creates an optional render lease");
        RenderObservationState state;
        Check(observation->ReadState(state) == Status::Ok && !state.bound && !state.active && !state.modulePinned && !state.generation,
              "allocating a render lease performs no engine binding, pin or detour");
        Status                 foreignBegin = Status::Ok, foreignRead = Status::Ok, foreignEnd = Status::Ok;
        RenderObservationState foreignState;
        foreignState.width = 123;
        std::thread foreign(
            [&]
            {
                foreignBegin = observation->Begin();
                foreignRead = observation->ReadState(foreignState);
                foreignEnd = observation->End();
                observation->Release();
            });
        foreign.join();
        Check(foreignBegin == Status::WrongThread && foreignRead == Status::WrongThread && foreignEnd == Status::WrongThread && foreignState.width == 123,
              "foreign render calls reject without replacing output or releasing the owner");
        Check(observation->Begin() == Status::Unsupported, "actual renderer admission rejects the non-game executable");
        Check(observation->ReadState(state) == Status::Ok && state.generation > 0 && state.failureReason != 0 && !state.bound && !state.active &&
                  !state.modulePinned && !state.pendingCleanup && !state.nativePresentCallbacks && !state.acceptedSnapshots,
              "wrong-image render admission installs no hook, pins no DLL and reports no native observation");
        Check(observation->End() == Status::Ok && observation->End() == Status::Ok, "owner cleanup is idempotent after rejected renderer admission");
        Check(observation->ReadState(state) == Status::Ok && !state.active && !state.pendingCleanup,
              "actual ended state remains readable before releasing its lease");
        observation->Release();
    }

    void TestNativeMovementRejection()
    {
        OwnerIdentity owner;
        OwnedAdapter  adapter;
        Check(adapter.value != nullptr, "create explicit movement trial fixture");
        NativeMovementTrialState state;
        state.callbackCount = 123;
        Check(adapter.value->BeginNativeMovementTrial() == Status::NotInitialized &&
                  adapter.value->GetNativeMovementTrialState(state) == Status::NotInitialized &&
                  adapter.value->EndNativeMovementTrial() == Status::NotInitialized && state.callbackCount == 123,
              "movement trial requires initialization and preserves failed read output");
        Check(adapter.value->Initialize(owner.Get()) == Status::Ok, "initialize movement trial owner");
        Check(adapter.value->BeginNativeMovementTrial() == Status::NotInitialized, "trial requires a submitted MTA logical frame");
        for (const auto action : {FIRE, ENTER_EXIT, JUMP, AIM_WEAPON})
        {
            Check(adapter.value->SubmitControlActions({MtaInput::ActionBit(action), {}}) == Status::Ok, "ordinary logical synthesis retains nontrial actions");
            Check(adapter.value->BeginNativeMovementTrial() == Status::InvalidArgument, "trial rejects unrelated gameplay actions before any binding");
        }
        for (unsigned int contextBit = 0; contextBit < 5; ++contextBit)
        {
            ControlActionFrame frame;
            switch (contextBit)
            {
                case 0:
                    frame.context.inVehicle = true;
                    break;
                case 1:
                    frame.context.hasDetonator = true;
                    break;
                case 2:
                    frame.context.isDead = true;
                    break;
                case 3:
                    frame.context.aimingWeapon = true;
                    break;
                case 4:
                    frame.context.enteringVehicle = true;
                    break;
            }
            Check(adapter.value->SubmitControlActions(frame) == Status::Ok && adapter.value->BeginNativeMovementTrial() == Status::InvalidArgument,
                  "trial requires an unambiguous alive on-foot context");
        }
        Check(adapter.value->SubmitControlActions({}) == Status::Ok, "neutral MTA frame permits an explicit binding attempt");
        Check(adapter.value->BeginNativeMovementTrial() == Status::Unsupported && adapter.value->BeginNativeMovementTrial() == Status::Unsupported,
              "nongame executable rejects trial binding and permits a clean retry");
        Check(adapter.value->GetNativeMovementTrialState(state) == Status::Ok && !state.bound && !state.active && state.callbackCount == 0 &&
                  state.scopedSteps == 0 && state.routedQueries == 0 && state.restoredSteps == 0,
              "rejected binding installs no hooks and executes no native callback");
        Check(adapter.value->GetCapabilities() == implementedCapabilities && adapter.value->GetGameInterface() == nullptr,
              "candidate trial cannot manufacture full engine input or gameplay capabilities");
        Check(adapter.value->EndNativeMovementTrial() == Status::Ok && adapter.value->EndNativeMovementTrial() == Status::Ok,
              "ending an unbound trial is idempotent");
        Check(adapter.value->Reset() == Status::Ok && adapter.value->BeginNativeMovementTrial() == Status::NotInitialized,
              "reset clears the frame before any retry");
        Check(adapter.value->Shutdown() == Status::Ok, "clean nongame shutdown releases all trial authority");
    }

    void TestRenderObservationV2FactoryRejection()
    {
        OwnerIdentity owner;
        Check(!CreateGameRenderObservationV2(RenderObservationApiVersion, owner.Get(), RenderObservationRoute::D3D12) &&
                  !CreateGameRenderObservationV2(RenderObservationApiVersionV2 + 1, owner.Get(), RenderObservationRoute::D3D12),
              "actual version2 factory never returns its new vtable to a legacy or unknown version");
        Check(!CreateGameRenderObservationV2(RenderObservationApiVersionV2, nullptr, RenderObservationRoute::D3D12) &&
                  !CreateGameRenderObservationV2(RenderObservationApiVersionV2, owner.Get(), RenderObservationRoute::Unknown) &&
                  !CreateGameRenderObservationV2(RenderObservationApiVersionV2, owner.Get(), static_cast<RenderObservationRoute>(99)),
              "actual version2 factory rejects absent owner and unsupported route before engine admission");
        for (const auto route : {RenderObservationRoute::D3D11, RenderObservationRoute::D3D12})
        {
            auto* observation = CreateGameRenderObservationV2(RenderObservationApiVersionV2, owner.Get(), route);
            Check(observation != nullptr, "actual factory creates a separately selected route lease");
            RenderObservationStateV2 state;
            Check(observation->ReadState(state) == Status::Ok && !state.base.bound && !state.base.active && !state.base.modulePinned &&
                      !state.base.generation && !state.observedRoute && !state.presenceKnownMask && !state.queuePresent,
                  "version2 allocation performs no hook, module pin or observed-route inference");
            Status                   foreignBegin = Status::Ok, foreignRead = Status::Ok, foreignEnd = Status::Ok;
            RenderObservationStateV2 foreignState;
            foreignState.base.width = 123;
            foreignState.observedRoute = 99;
            std::thread foreign(
                [&]
                {
                    foreignBegin = observation->Begin();
                    foreignRead = observation->ReadState(foreignState);
                    foreignEnd = observation->End();
                    observation->Release();
                });
            foreign.join();
            Check(foreignBegin == Status::WrongThread && foreignRead == Status::WrongThread && foreignEnd == Status::WrongThread &&
                      foreignState.base.width == 123 && foreignState.observedRoute == 99,
                  "version2 foreign calls preserve caller values and cannot release the creating owner's lease");
            Check(observation->Begin() == Status::Unsupported, "each selected route retains the production wrong-executable rejection");
            Check(observation->ReadState(state) == Status::Ok && state.requestedRoute == static_cast<std::uint32_t>(route) && !state.observedRoute &&
                      !state.presenceKnownMask && !state.queuePresent && state.base.generation && state.base.failureReason && !state.base.bound &&
                      !state.base.active && !state.base.modulePinned && !state.base.pendingCleanup && !state.base.nativePresentCallbacks &&
                      !state.base.acceptedSnapshots,
                  "failed exact-image admission retains only the selected request, no callbacks, hooks, pins or fallback route");
            Check(observation->End() == Status::Ok && observation->End() == Status::Ok, "version2 owner cleanup is idempotent after rejected admission");
            Check(observation->ReadState(state) == Status::Ok && state.requestedRoute == static_cast<std::uint32_t>(route) && !state.observedRoute &&
                      !state.base.active && !state.base.pendingCleanup,
                  "ended version2 actual state preserves the explicit request and unknown observation before release");
            observation->Release();
        }
        // API1 still allocates its historical interface after API2 attempts;
        // the two factory contracts do not share a changed vtable or state.
        auto* legacy = CreateGameRenderObservation(RenderObservationApiVersion, owner.Get());
        Check(legacy && legacy->Begin() == Status::Unsupported, "legacy D3D11 API1 remains callable after version2 requests");
        RenderObservationState legacyState;
        Check(legacy->ReadState(legacyState) == Status::Ok && !legacyState.bound && !legacyState.active && !legacyState.modulePinned &&
                  !legacyState.nativePresentCallbacks,
              "API1 reports the unchanged wrong-image state layout");
        Check(legacy->End() == Status::Ok, "legacy lease still ends through the old interface");
        legacy->Release();
    }
    void TestRenderObservationV3FactoryRejection()
    {
        OwnerIdentity owner;
        for (const auto version : {0u, 1u, 2u, 4u})
            Check(!CreateGameRenderObservationV3(version, owner.Get(), RenderObservationRoute::D3D12),
                  "actual API3 never substitutes an earlier or unknown diagnostics contract");
        Check(!CreateGameRenderObservationV3(3, nullptr, RenderObservationRoute::D3D12), "API3 requires an opaque MTA owner identity");
        for (const auto route : {RenderObservationRoute::Unknown, RenderObservationRoute::D3D11, static_cast<RenderObservationRoute>(99)})
            Check(!CreateGameRenderObservationV3(3, owner.Get(), route), "API3 COM queries require the explicit D3D12 route");
        auto* observation = CreateGameRenderObservationV3(3, owner.Get(), RenderObservationRoute::D3D12);
        Check(observation != nullptr, "actual API3 allocates its separate owner lease");
        RenderObservationStateV3 state;
        Check(observation->ReadState(state) == Status::Ok && !state.base.base.bound && !state.base.base.active && !state.base.base.modulePinned &&
                  !state.gpu.attempts && !state.gpu.accepted && !state.gpu.rejected && !state.gpu.queryStage && !state.gpu.metadataValid,
              "API3 allocation acquires no engine references and invents no GPU metadata");
        Status                   foreignBegin = Status::Ok, foreignRead = Status::Ok, foreignEnd = Status::Ok;
        RenderObservationStateV3 foreignState;
        foreignState.gpu.queryStage = 99;
        std::thread foreign(
            [&]
            {
                foreignBegin = observation->Begin();
                foreignRead = observation->ReadState(foreignState);
                foreignEnd = observation->End();
                observation->Release();
            });
        foreign.join();
        Check(
            foreignBegin == Status::WrongThread && foreignRead == Status::WrongThread && foreignEnd == Status::WrongThread && foreignState.gpu.queryStage == 99,
            "foreign API3 calls preserve output and cannot release an owned lease");
        Check(observation->Begin() == Status::Unsupported, "real API3 binding rejects the owned nongame executable");
        Check(observation->ReadState(state) == Status::Ok && state.base.requestedRoute == 2 && !state.base.observedRoute && !state.base.base.bound &&
                  !state.base.base.active && !state.base.base.modulePinned && state.base.base.failureReason && !state.gpu.attempts && !state.gpu.queryStage &&
                  !state.gpu.metadataValid && !state.gpu.supportPredicate,
              "failed image binding performs no COM queries, hook, PIN or inferred D3D12 observation");
        Check(observation->End() == Status::Ok && observation->End() == Status::Ok, "wrong-image API3 End is idempotent");
        Check(observation->ReadState(state) == Status::Ok && !state.base.base.active && !state.base.base.pendingCleanup && !state.gpu.attempts,
              "actual ended API3 readback keeps failed admission and zero-query evidence");
        observation->Release();
    }
    void TestRenderObservationV4FactoryRejection()
    {
        OwnerIdentity owner;
        for (const auto version : {0u, 1u, 2u, 3u, 5u})
            Check(!CreateGameRenderObservationV4(version, owner.Get(), RenderObservationRoute::D3D12),
                  "actual API4 never substitutes an earlier or unknown diagnostics contract");
        Check(!CreateGameRenderObservationV4(4, nullptr, RenderObservationRoute::D3D12), "API4 requires an opaque MTA owner identity");
        for (const auto route : {RenderObservationRoute::Unknown, RenderObservationRoute::D3D11, static_cast<RenderObservationRoute>(99)})
            Check(!CreateGameRenderObservationV4(4, owner.Get(), route), "API4 COM queries require the explicit D3D12 route");
        auto* observation = CreateGameRenderObservationV4(4, owner.Get(), RenderObservationRoute::D3D12);
        Check(observation != nullptr, "actual API4 allocates its separate owner lease");
        RenderObservationStateV4 state;
        Check(observation->ReadState(state) == Status::Ok && !state.base.base.base.bound && !state.base.base.base.active &&
                  !state.base.base.base.modulePinned && !state.custom.status && !state.custom.implementationId && !state.custom.slotSignatureMask &&
                  !state.custom.staticPolicyFlags && !state.base.gpu.attempts && !state.base.gpu.accepted && !state.base.gpu.rejected &&
                  !state.base.gpu.queryStage && !state.base.gpu.metadataValid,
              "API4 allocation acquires no engine references and invents no GPU metadata");
        Status                   foreignBegin = Status::Ok, foreignRead = Status::Ok, foreignEnd = Status::Ok;
        RenderObservationStateV4 foreignState;
        foreignState.base.gpu.queryStage = 99;
        foreignState.custom.staticPolicyFlags = 99;
        std::thread foreign(
            [&]
            {
                foreignBegin = observation->Begin();
                foreignRead = observation->ReadState(foreignState);
                foreignEnd = observation->End();
                observation->Release();
            });
        foreign.join();
        Check(foreignBegin == Status::WrongThread && foreignRead == Status::WrongThread && foreignEnd == Status::WrongThread &&
                  foreignState.base.gpu.queryStage == 99 && foreignState.custom.staticPolicyFlags == 99,
              "foreign API4 calls preserve output and cannot release an owned lease");
        Check(observation->Begin() == Status::Unsupported, "real API4 binding rejects the owned nongame executable");
        Check(observation->ReadState(state) == Status::Ok && state.base.base.requestedRoute == 2 && !state.base.base.observedRoute &&
                  !state.base.base.base.bound && !state.base.base.base.active && !state.base.base.base.modulePinned && state.base.base.base.failureReason &&
                  !state.base.gpu.attempts && !state.base.gpu.queryStage && !state.base.gpu.metadataValid && !state.base.gpu.supportPredicate &&
                  !state.custom.status && !state.custom.implementationId && !state.custom.slotSignatureMask && !state.custom.staticPolicyFlags,
              "failed image binding performs no COM queries, hook, PIN or inferred D3D12 observation");
        Check(observation->End() == Status::Ok && observation->End() == Status::Ok, "wrong-image API4 End is idempotent");
        Check(observation->ReadState(state) == Status::Ok && !state.base.base.base.active && !state.base.base.base.pendingCleanup && !state.base.gpu.attempts,
              "actual ended API4 readback keeps failed admission and zero-query evidence");
        observation->Release();
    }

    void TestRenderObservationV5FactoryRejection()
    {
        OwnerIdentity owner;
        for (const auto version : {0u, 1u, 2u, 3u, 4u, 6u, std::numeric_limits<std::uint32_t>::max()})
            Check(!CreateGameRenderObservationV5(version, owner.Get(), RenderObservationRoute::D3D12),
                  "API5 rejects every earlier contract and unknown versions before constructing a boundary lease");
        Check(!CreateGameRenderObservationV5(RenderObservationApiVersionV5, nullptr, RenderObservationRoute::D3D12),
              "API5 boundary diagnostics require an opaque MTA owner");
        for (const auto route : {RenderObservationRoute::Unknown, RenderObservationRoute::D3D11, static_cast<RenderObservationRoute>(99)})
            Check(!CreateGameRenderObservationV5(RenderObservationApiVersionV5, owner.Get(), route),
                  "API5 never interprets a missing or different renderer request as D3D12 admission");
        Check(!CreateGameRenderObservation(RenderObservationApiVersionV5, owner.Get()) &&
                  !CreateGameRenderObservationV2(RenderObservationApiVersionV5, owner.Get(), RenderObservationRoute::D3D12) &&
                  !CreateGameRenderObservationV3(RenderObservationApiVersionV5, owner.Get(), RenderObservationRoute::D3D12) &&
                  !CreateGameRenderObservationV4(RenderObservationApiVersionV5, owner.Get(), RenderObservationRoute::D3D12),
              "all four historical factories refuse API5 instead of returning an incompatible output layout");

        const auto boundaryZero = [](const FrameBoundaryObservationState& value)
        {
            return !value.status && !value.endpointProfileId && !value.factsMask && !value.presentHresult && !value.nativeBufferIndex &&
                   !value.dxgiBufferIndex && !value.nativeTrackedState && !value.nativeTrackedMode && !value.presentEntries && !value.rejectedEntries &&
                   !value.resizeEpoch && !value.resizeEntries && !value.preReleaseEntries && !value.foreignLifecycleEntries && !value.presentThreadId &&
                   !value.resizeThreadId && !value.preReleaseThreadId && !value.lifecycleFlags && !value.queryStage && !value.queryHresult;
        };
        auto* observation = CreateGameRenderObservationV5(RenderObservationApiVersionV5, owner.Get(), RenderObservationRoute::D3D12);
        Check(observation != nullptr, "actual API5 factory allocates its separate module-owned boundary observer");
        RenderObservationStateV5 state;
        const auto&              historical = state.base.base.base.base;
        const auto&              route = state.base.base.base;
        const auto&              gpu = state.base.base.gpu;
        const auto&              custom = state.base.custom;
        Check(observation->ReadState(state) == Status::Ok && !historical.bound && !historical.active && !historical.modulePinned &&
                  !historical.pendingCleanup && !historical.generation && !historical.nativePresentCallbacks && !historical.acceptedSnapshots &&
                  !historical.rejectedSnapshots && !route.observedRoute && !route.presenceKnownMask && !gpu.attempts && !gpu.accepted && !gpu.rejected &&
                  !gpu.queryStage && !gpu.metadataValid && !gpu.supportPredicate && !custom.status && !custom.implementationId && !custom.slotSignatureMask &&
                  !custom.staticPolicyFlags && boundaryZero(state.boundary),
              "API5 construction performs no hook, endpoint admission, COM query, lifecycle callback or inferred association");

        Status                   foreignBegin = Status::Ok, foreignRead = Status::Ok, foreignEnd = Status::Ok;
        RenderObservationStateV5 foreignState;
        foreignState.base.base.base.base.width = 321;
        foreignState.base.base.gpu.queryStage = 98;
        foreignState.base.custom.staticPolicyFlags = 97;
        const std::array<std::uint32_t FrameBoundaryObservationState::*, 14> boundaryScalars = {&FrameBoundaryObservationState::status,
                                                                                                &FrameBoundaryObservationState::endpointProfileId,
                                                                                                &FrameBoundaryObservationState::factsMask,
                                                                                                &FrameBoundaryObservationState::presentHresult,
                                                                                                &FrameBoundaryObservationState::nativeBufferIndex,
                                                                                                &FrameBoundaryObservationState::dxgiBufferIndex,
                                                                                                &FrameBoundaryObservationState::nativeTrackedState,
                                                                                                &FrameBoundaryObservationState::nativeTrackedMode,
                                                                                                &FrameBoundaryObservationState::presentThreadId,
                                                                                                &FrameBoundaryObservationState::resizeThreadId,
                                                                                                &FrameBoundaryObservationState::preReleaseThreadId,
                                                                                                &FrameBoundaryObservationState::lifecycleFlags,
                                                                                                &FrameBoundaryObservationState::queryStage,
                                                                                                &FrameBoundaryObservationState::queryHresult};
        const std::array<std::uint64_t FrameBoundaryObservationState::*, 6>  boundaryCounters = {
            &FrameBoundaryObservationState::presentEntries,    &FrameBoundaryObservationState::rejectedEntries,
            &FrameBoundaryObservationState::resizeEpoch,       &FrameBoundaryObservationState::resizeEntries,
            &FrameBoundaryObservationState::preReleaseEntries, &FrameBoundaryObservationState::foreignLifecycleEntries};
        for (const auto field : boundaryScalars)
            foreignState.boundary.*field = 0xa5a5a5a5u;
        for (const auto field : boundaryCounters)
            foreignState.boundary.*field = 0xfedcba9876543210ull;
        std::thread foreign(
            [&]
            {
                foreignBegin = observation->Begin();
                foreignRead = observation->ReadState(foreignState);
                foreignEnd = observation->End();
                observation->Release();
            });
        foreign.join();
        Check(foreignBegin == Status::WrongThread && foreignRead == Status::WrongThread && foreignEnd == Status::WrongThread &&
                  foreignState.base.base.base.base.width == 321 && foreignState.base.base.gpu.queryStage == 98 &&
                  foreignState.base.custom.staticPolicyFlags == 97,
              "foreign API5 methods preserve historical caller output and cannot end or release the owner lease");
        for (const auto field : boundaryScalars)
            Check(foreignState.boundary.*field == 0xa5a5a5a5u, "wrong-thread API5 reads preserve every new scalar without partial publication");
        for (const auto field : boundaryCounters)
            Check(foreignState.boundary.*field == 0xfedcba9876543210ull, "wrong-thread API5 reads preserve every new 64-bit lifecycle counter");

        // API5 adds another native hook only after exact image admission. An
        // ordinary module host must reject before touching system DXGI or COM;
        // zero boundary state is therefore evidence of no attempted observation.
        Check(observation->Begin() == Status::Unsupported, "real API5 rejects the owned nongame executable before endpoint installation");
        Check(observation->ReadState(state) == Status::Ok && route.requestedRoute == 2 && !route.observedRoute && !route.presenceKnownMask &&
                  historical.generation && historical.failureReason && !historical.bound && !historical.active && !historical.modulePinned &&
                  !historical.pendingCleanup && !historical.nativePresentCallbacks && !historical.acceptedSnapshots && !gpu.attempts && !gpu.queryStage &&
                  !gpu.metadataValid && !gpu.supportPredicate && !custom.status && !custom.implementationId && !custom.slotSignatureMask &&
                  !custom.staticPolicyFlags && boundaryZero(state.boundary),
              "wrong-image API5 records only its requested route and failure, with no endpoints, references, frame facts or GPU permission");
        Check(observation->End() == Status::Ok && observation->End() == Status::Ok, "API5 owner cleanup is idempotent after unsupported admission");
        Check(observation->ReadState(state) == Status::Ok && route.requestedRoute == 2 && !route.observedRoute && !historical.active &&
                  !historical.pendingCleanup && !historical.modulePinned && !gpu.attempts && boundaryZero(state.boundary),
              "ended API5 state remains readable with all new observations unattempted until owner release");
        observation->Release();
    }

    void TestEndpointDiagnosticLeaseRejection()
    {
        OwnerIdentity              owner;
        RenderEndpointDiagnosticV1 output;
        std::memset(&output, 0xa5, sizeof(output));
        const auto sentinel = output;
        const auto unchanged = [&] { return std::memcmp(&output, &sentinel, sizeof(output)) == 0; };
        auto*      observation = CreateGameRenderObservationV5(RenderObservationApiVersionV5, owner.Get(), RenderObservationRoute::D3D12);
        auto*      other = CreateGameRenderObservationV5(RenderObservationApiVersionV5, owner.Get(), RenderObservationRoute::D3D12);
        Check(observation && other, "allocate two independent API5 leases for exact diagnostic identity checks");
        for (const auto version : {0u, 2u, 5u, std::numeric_limits<std::uint32_t>::max()})
            Check(ReadGameRenderEndpointDiagnosticV1(version, observation, &output) == Status::InvalidArgument && unchanged(),
                  "unknown diagnostic versions preserve every output byte before native access");
        Check(ReadGameRenderEndpointDiagnosticV1(1, nullptr, &output) == Status::InvalidArgument && unchanged() &&
                  ReadGameRenderEndpointDiagnosticV1(1, observation, nullptr) == Status::InvalidArgument,
              "the separate diagnostic export rejects null arguments without publishing partial output");
        Check(ReadGameRenderEndpointDiagnosticV1(1, observation, &output) == Status::Unsupported && unchanged() &&
                  ReadGameRenderEndpointDiagnosticV1(1, other, &output) == Status::Unsupported && unchanged(),
              "allocation alone cannot expose another lease's endpoint cache or invent a prepared endpoint");

        // An opaque argument may be unreadable. Exact registry comparison must
        // reject it before a vtable, object field or caller output is touched.
        auto* inaccessible = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
        Check(inaccessible != nullptr, "reserve an unreadable page for diagnostic lease validation");
        auto* inaccessibleLease = reinterpret_cast<IGameRenderObservationV5*>(inaccessible);
        Check(ReadGameRenderEndpointDiagnosticV1(1, inaccessibleLease, &output) == Status::InvalidArgument && unchanged(),
              "unreadable unregistered lease values are rejected without dereferencing caller memory");
        Check(ReadGameRenderEndpointDiagnosticV1(1, inaccessibleLease, static_cast<RenderEndpointDiagnosticV1*>(inaccessible)) == Status::InvalidArgument,
              "an unregistered lease cannot cause even an unreadable output pointer to be accessed");
        Check(ReadGameRenderEndpointDiagnosticV1(2, observation, static_cast<RenderEndpointDiagnosticV1*>(inaccessible)) == Status::InvalidArgument,
              "version rejection precedes all output writes even for an exact live lease");
        Check(VirtualFree(inaccessible, 0, MEM_RELEASE) != 0, "release the owned unreadable validation page");

        auto* legacy1 = CreateGameRenderObservation(1, owner.Get());
        auto* legacy2 = CreateGameRenderObservationV2(2, owner.Get(), RenderObservationRoute::D3D12);
        auto* legacy3 = CreateGameRenderObservationV3(3, owner.Get(), RenderObservationRoute::D3D12);
        auto* legacy4 = CreateGameRenderObservationV4(4, owner.Get(), RenderObservationRoute::D3D12);
        Check(legacy1 && legacy2 && legacy3 && legacy4, "allocate all four historical interfaces for separate-export isolation");
        for (auto* lease : {reinterpret_cast<IGameRenderObservationV5*>(legacy1), reinterpret_cast<IGameRenderObservationV5*>(legacy2),
                            reinterpret_cast<IGameRenderObservationV5*>(legacy3), reinterpret_cast<IGameRenderObservationV5*>(legacy4)})
            Check(ReadGameRenderEndpointDiagnosticV1(1, lease, &output) == Status::InvalidArgument && unchanged(),
                  "historical render leases never acquire the separate API5 endpoint diagnostic capability");
        legacy1->Release();
        legacy2->Release();
        legacy3->Release();
        legacy4->Release();

        Status      foreign = Status::Ok;
        std::thread wrongThread(
            [&]
            {
                foreign = ReadGameRenderEndpointDiagnosticV1(1, observation, &output);
                observation->Release();
            });
        wrongThread.join();
        Check(foreign == Status::WrongThread && unchanged(), "foreign diagnostic reads preserve output and cannot release the retained owner lease");
        Check(observation->Begin() == Status::Unsupported && ReadGameRenderEndpointDiagnosticV1(1, observation, &output) == Status::Unsupported && unchanged(),
              "wrong-image rejection produces no private endpoint diagnostic and cannot leak a previous generation");
        Check(observation->End() == Status::Ok && ReadGameRenderEndpointDiagnosticV1(1, observation, &output) == Status::Unsupported && unchanged(),
              "End preserves safe diagnostic eligibility without fabricating a cache for unsupported admission");

        const auto releasedAddress = reinterpret_cast<std::uintptr_t>(observation);
        observation->Release();
        Check(ReadGameRenderEndpointDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV5*>(releasedAddress), &output) == Status::InvalidArgument &&
                  unchanged(),
              "Release removes the exact registry identity before destruction while another live V5 lease remains registered");
        Check(ReadGameRenderEndpointDiagnosticV1(1, other, &output) == Status::Unsupported && unchanged(),
              "removing a non-head registry entry preserves the independent live lease");

        // A foreign reader can race owner Release, but it must observe either a
        // live wrong-thread identity or an absent identity, never freed fields.
        const auto        racingAddress = reinterpret_cast<std::uintptr_t>(other);
        std::atomic<bool> readerReady{false}, releaseStarted{false};
        bool              racePreserved = true;
        std::thread       reader(
            [&]
            {
                RenderEndpointDiagnosticV1 foreignOutput = sentinel;
                readerReady.store(true);
                while (!releaseStarted.load())
                    std::this_thread::yield();
                for (unsigned index = 0; index < 10000; ++index)
                {
                    const auto status = ReadGameRenderEndpointDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV5*>(racingAddress), &foreignOutput);
                    if ((status != Status::WrongThread && status != Status::InvalidArgument) || std::memcmp(&foreignOutput, &sentinel, sizeof(foreignOutput)))
                        racePreserved = false;
                }
            });
        while (!readerReady.load())
            std::this_thread::yield();
        releaseStarted.store(true);
        other->Release();
        reader.join();
        Check(racePreserved, "foreign read versus owner Release never accesses a destroyed lease or mutates rejected output");
        Check(ReadGameRenderEndpointDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV5*>(racingAddress), &output) == Status::InvalidArgument &&
                  unchanged(),
              "removing the final registry entry makes subsequent diagnostic reads reject without native access");
    }

    void TestSetupDiagnosticLeaseRejection()
    {
        OwnerIdentity owner;
        auto*         lease = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        auto*         other = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        auto*         older = CreateGameRenderObservationV5(5, owner.Get(), RenderObservationRoute::D3D12);
        Check(lease && other && older, "setup diagnostics retain only exact V6 lease identities");
        RenderSetupDiagnosticV1 output;
        std::memset(&output, 0xa5, sizeof(output));
        const auto sentinel = output;
        const auto unchanged = [&] { return std::memcmp(&output, &sentinel, sizeof(output)) == 0; };
        for (const auto version : {0u, 2u, 6u, std::numeric_limits<std::uint32_t>::max()})
            Check(ReadGameRenderSetupDiagnosticV1(version, lease, &output) == Status::InvalidArgument && unchanged(),
                  "setup payload version is independent of observation API6 and rejects without output mutation");
        Check(ReadGameRenderSetupDiagnosticV1(1, nullptr, &output) == Status::InvalidArgument && unchanged() &&
                  ReadGameRenderSetupDiagnosticV1(1, lease, nullptr) == Status::InvalidArgument,
              "setup export validates null arguments before native cache access");
        Check(ReadGameRenderSetupDiagnosticV1(1, lease, &output) == Status::Unsupported && unchanged() &&
                  ReadGameRenderSetupDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV6*>(older), &output) == Status::InvalidArgument && unchanged(),
              "allocation alone and an older interface cannot expose API6 setup evidence");
        auto* inaccessible = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
        Check(inaccessible != nullptr, "reserve unreadable setup lease validation page");
        Check(ReadGameRenderSetupDiagnosticV1(1, static_cast<IGameRenderObservationV6*>(inaccessible), static_cast<RenderSetupDiagnosticV1*>(inaccessible)) ==
                      Status::InvalidArgument &&
                  ReadGameRenderSetupDiagnosticV1(2, lease, static_cast<RenderSetupDiagnosticV1*>(inaccessible)) == Status::InvalidArgument,
              "unknown identity and wrong version never dereference caller lease or output pointers");
        Check(VirtualFree(inaccessible, 0, MEM_RELEASE) != 0, "release setup validation page");
        Status      foreign = Status::Ok;
        std::thread reader([&] { foreign = ReadGameRenderSetupDiagnosticV1(1, lease, &output); });
        reader.join();
        Check(foreign == Status::WrongThread && unchanged(), "setup cache is available only on the lease producer thread");
        Check(lease->Begin() == Status::Unsupported && ReadGameRenderSetupDiagnosticV1(1, lease, &output) == Status::Unsupported && unchanged(),
              "wrong-image Begin fails before setup cache admission without inventing diagnostics");
        Check(lease->End() == Status::Ok && ReadGameRenderSetupDiagnosticV1(1, lease, &output) == Status::Unsupported && unchanged(),
              "clean End preserves the lack of setup evidence for an unadmitted image");
        const auto released = reinterpret_cast<std::uintptr_t>(lease);
        lease->Release();
        Check(ReadGameRenderSetupDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV6*>(released), &output) == Status::InvalidArgument && unchanged() &&
                  ReadGameRenderSetupDiagnosticV1(1, other, &output) == Status::Unsupported && unchanged(),
              "Release removes only its exact setup lease while another allocated V6 identity remains valid");
        const auto        racing = reinterpret_cast<std::uintptr_t>(other);
        std::atomic<bool> ready{false}, releasing{false};
        bool              preserved = true;
        std::thread       racingReader(
            [&]
            {
                RenderSetupDiagnosticV1 next = sentinel;
                ready.store(true);
                while (!releasing.load())
                    std::this_thread::yield();
                for (unsigned index = 0; index < 2000; ++index)
                {
                    const auto result = ReadGameRenderSetupDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV6*>(racing), &next);
                    if ((result != Status::WrongThread && result != Status::InvalidArgument && result != Status::Unsupported) ||
                        std::memcmp(&next, &sentinel, sizeof(next)))
                        preserved = false;
                }
            });
        while (!ready.load())
            std::this_thread::yield();
        releasing.store(true);
        other->Release();
        racingReader.join();
        Check(preserved && ReadGameRenderSetupDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV6*>(racing), &output) == Status::InvalidArgument &&
                  unchanged(),
              "setup read versus Release cannot use freed leases or mutate rejected output");
        older->Release();
    }

    void TestRootsDiagnosticLeaseRejection()
    {
        OwnerIdentity owner;
        auto*         lease = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        auto*         other = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        auto*         older = CreateGameRenderObservationV5(5, owner.Get(), RenderObservationRoute::D3D12);
        Check(lease && other && older, "roots diagnostics retain only exact V6 lease identities");
        RenderRootsDiagnosticV1 output;
        std::memset(&output, 0xa5, sizeof(output));
        const auto sentinel = output;
        const auto unchanged = [&] { return std::memcmp(&output, &sentinel, sizeof(output)) == 0; };
        for (const auto version : {0u, 2u, 6u, std::numeric_limits<std::uint32_t>::max()})
            Check(ReadGameRenderRootsDiagnosticV1(version, lease, &output) == Status::InvalidArgument && unchanged(),
                  "roots payload version is independent of observation API6 and rejects without output mutation");
        Check(ReadGameRenderRootsDiagnosticV1(1, nullptr, &output) == Status::InvalidArgument && unchanged() &&
                  ReadGameRenderRootsDiagnosticV1(1, lease, nullptr) == Status::InvalidArgument,
              "roots export validates null arguments before native cache access");
        Check(ReadGameRenderRootsDiagnosticV1(1, lease, &output) == Status::Unsupported && unchanged() &&
                  ReadGameRenderRootsDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV6*>(older), &output) == Status::InvalidArgument && unchanged(),
              "allocation alone and an older interface cannot expose API6 roots evidence");
        auto* inaccessible = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
        Check(inaccessible != nullptr, "reserve unreadable roots lease validation page");
        Check(ReadGameRenderRootsDiagnosticV1(1, static_cast<IGameRenderObservationV6*>(inaccessible), static_cast<RenderRootsDiagnosticV1*>(inaccessible)) ==
                      Status::InvalidArgument &&
                  ReadGameRenderRootsDiagnosticV1(2, lease, static_cast<RenderRootsDiagnosticV1*>(inaccessible)) == Status::InvalidArgument,
              "unknown identity and wrong version never dereference caller lease or output pointers");
        Check(VirtualFree(inaccessible, 0, MEM_RELEASE) != 0, "release roots validation page");
        Status      foreign = Status::Ok;
        std::thread reader([&] { foreign = ReadGameRenderRootsDiagnosticV1(1, lease, &output); });
        reader.join();
        Check(foreign == Status::WrongThread && unchanged(), "roots cache is available only on the lease producer thread");
        Check(lease->Begin() == Status::Unsupported && ReadGameRenderRootsDiagnosticV1(1, lease, &output) == Status::Unsupported && unchanged(),
              "wrong-image Begin fails before roots cache admission without inventing diagnostics");
        Check(lease->End() == Status::Ok && ReadGameRenderRootsDiagnosticV1(1, lease, &output) == Status::Unsupported && unchanged(),
              "clean End preserves the lack of roots evidence for an unadmitted image");
        const auto released = reinterpret_cast<std::uintptr_t>(lease);
        lease->Release();
        Check(ReadGameRenderRootsDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV6*>(released), &output) == Status::InvalidArgument && unchanged() &&
                  ReadGameRenderRootsDiagnosticV1(1, other, &output) == Status::Unsupported && unchanged(),
              "Release removes only its exact roots lease while another allocated V6 identity remains valid");
        const auto        racing = reinterpret_cast<std::uintptr_t>(other);
        std::atomic<bool> ready{false}, releasing{false};
        bool              preserved = true;
        std::thread       racingReader(
            [&]
            {
                RenderRootsDiagnosticV1 next = sentinel;
                ready.store(true);
                while (!releasing.load())
                    std::this_thread::yield();
                for (unsigned index = 0; index < 2000; ++index)
                {
                    const auto result = ReadGameRenderRootsDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV6*>(racing), &next);
                    if ((result != Status::WrongThread && result != Status::InvalidArgument && result != Status::Unsupported) ||
                        std::memcmp(&next, &sentinel, sizeof(next)))
                        preserved = false;
                }
            });
        while (!ready.load())
            std::this_thread::yield();
        releasing.store(true);
        other->Release();
        racingReader.join();
        Check(preserved && ReadGameRenderRootsDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV6*>(racing), &output) == Status::InvalidArgument &&
                  unchanged(),
              "roots read versus Release cannot use freed leases or mutate rejected output");
        older->Release();
    }

    void TestDeviceLeaseContract()
    {
        OwnerIdentity owner;
        auto*         lease = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        auto*         older = CreateGameRenderObservationV5(5, owner.Get(), RenderObservationRoute::D3D12);
        Check(lease && older, "device handoff uses a separate explicit extension of an exact allocated V6 lease");
        auto* fresh = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        Check(fresh != nullptr, "independent fresh V6 registry allocation");
        const auto freshAddress = reinterpret_cast<std::uintptr_t>(fresh);
        fresh->Release();
        Check(RequestGameRenderDeviceLeaseV1(1, reinterpret_cast<IGameRenderObservationV6*>(freshAddress)) == Status::InvalidArgument,
              "never-begun V6 is removed from the exact registry independently of another allocated lease");
        RenderDeviceLeaseV1 output;
        std::memset(&output, 0xa5, sizeof(output));
        const auto sentinel = output;
        const auto unchanged = [&] { return std::memcmp(&output, &sentinel, sizeof(output)) == 0; };
        for (const auto version : {0u, 2u, 0xffffffffu})
            Check(RequestGameRenderDeviceLeaseV1(version, lease) == Status::InvalidArgument &&
                      TakeGameRenderDeviceLeaseV1(version, lease, &output) == Status::InvalidArgument && unchanged(),
                  "device extension rejects wrong versions without reading or changing output sentinels");
        Check(RequestGameRenderDeviceLeaseV1(1, nullptr) == Status::InvalidArgument &&
                  TakeGameRenderDeviceLeaseV1(1, lease, nullptr) == Status::InvalidArgument &&
                  TakeGameRenderDeviceLeaseV1(1, nullptr, &output) == Status::InvalidArgument && unchanged(),
              "device extension rejects null arguments");
        auto* foreign = reinterpret_cast<IGameRenderObservationV6*>(std::uintptr_t{1});
        Check(RequestGameRenderDeviceLeaseV1(1, foreign) == Status::InvalidArgument &&
                  TakeGameRenderDeviceLeaseV1(1, foreign, &output) == Status::InvalidArgument &&
                  RequestGameRenderDeviceLeaseV1(1, reinterpret_cast<IGameRenderObservationV6*>(older)) == Status::InvalidArgument && unchanged(),
              "opaque invalid and older leases never get dereferenced or armed");
        Check(TakeGameRenderDeviceLeaseV1(1, lease, &output) != Status::Ok && unchanged(), "allocated-only device request has no native reference");
        Status      request = Status::Ok, take = Status::Ok;
        std::thread wrongThread(
            [&]
            {
                request = RequestGameRenderDeviceLeaseV1(1, lease);
                take = TakeGameRenderDeviceLeaseV1(1, lease, &output);
            });
        wrongThread.join();
        Check(request == Status::WrongThread && take == Status::WrongThread && unchanged(), "foreign thread cannot arm or take an exact device lease");
        Check(RequestGameRenderDeviceLeaseV1(1, lease) == Status::Ok && RequestGameRenderDeviceLeaseV1(1, lease) == Status::Unsupported,
              "device request arms once before the first Begin");
        Check(lease->Begin() == Status::Unsupported && RequestGameRenderDeviceLeaseV1(1, lease) == Status::Unsupported &&
                  TakeGameRenderDeviceLeaseV1(1, lease, &output) != Status::Ok && unchanged(),
              "wrong-image Begin consumes the request without capturing a device or allowing rearm");
        Check(lease->End() == Status::Ok && TakeGameRenderDeviceLeaseV1(1, lease, &output) == Status::NotInitialized && unchanged(),
              "End keeps a refused hardware startup device-free and output untouched");
        const auto released = reinterpret_cast<std::uintptr_t>(lease);
        lease->Release();
        Check(RequestGameRenderDeviceLeaseV1(1, reinterpret_cast<IGameRenderObservationV6*>(released)) == Status::InvalidArgument &&
                  TakeGameRenderDeviceLeaseV1(1, reinterpret_cast<IGameRenderObservationV6*>(released), &output) == Status::InvalidArgument && unchanged(),
              "released device lease is removed from exact registry before any access");
        older->Release();
        auto* plain = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        Check(plain && plain->Begin() == Status::Unsupported && RequestGameRenderDeviceLeaseV1(1, plain) == Status::Unsupported,
              "ordinary V6 remains device-free and cannot arm retrospectively after Begin");
        plain->Release();
    }
    void TestRenderObservationV6Contract()
    {
        OwnerIdentity owner;
        for (const auto version : {0u, 1u, 2u, 3u, 4u, 5u, 7u, std::numeric_limits<std::uint32_t>::max()})
            Check(!CreateGameRenderObservationV6(version, owner.Get(), RenderObservationRoute::D3D12),
                  "API6 accepts only its separately declared native-dispatch contract");
        Check(!CreateGameRenderObservationV6(6, nullptr, RenderObservationRoute::D3D12), "API6 requires an opaque Core owner before allocation");
        for (const auto route : {RenderObservationRoute::Unknown, RenderObservationRoute::D3D11, static_cast<RenderObservationRoute>(99)})
            Check(!CreateGameRenderObservationV6(6, owner.Get(), route), "native-dispatch observation requires explicit D3D12 selection");
        Check(!CreateGameRenderObservation(6, owner.Get()) && !CreateGameRenderObservationV2(6, owner.Get(), RenderObservationRoute::D3D12) &&
                  !CreateGameRenderObservationV3(6, owner.Get(), RenderObservationRoute::D3D12) &&
                  !CreateGameRenderObservationV4(6, owner.Get(), RenderObservationRoute::D3D12) &&
                  !CreateGameRenderObservationV5(6, owner.Get(), RenderObservationRoute::D3D12),
              "five historical factories cannot return the larger API6 state through an older interface");
        auto* observation = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        auto* other = CreateGameRenderObservationV6(6, owner.Get(), RenderObservationRoute::D3D12);
        auto* historical = CreateGameRenderObservationV5(5, owner.Get(), RenderObservationRoute::D3D12);
        Check(observation && other && historical, "actual module allocates independent V6 leases alongside the unchanged V5 contract");
        const FrameBoundaryObservationState zero;
        const auto                          empty = [&](const FrameBoundaryObservationState& value) { return std::memcmp(&value, &zero, sizeof(value)) == 0; };
        RenderObservationStateV6            state;
        Check(observation->ReadState(state) == Status::Ok && !state.base.base.base.base.base.generation && empty(state.base.boundary) &&
                  empty(state.nativeDispatch) && !state.base.base.base.gpu.attempts && !state.base.base.custom.status,
              "successful API6 read after allocation reports both observation tails unattempted without native effects");

        RenderEndpointDiagnosticV1 diagnostic;
        std::memset(&diagnostic, 0xa5, sizeof(diagnostic));
        const auto diagnosticSentinel = diagnostic;
        const auto unchanged = [&] { return std::memcmp(&diagnostic, &diagnosticSentinel, sizeof(diagnostic)) == 0; };
        Check(ReadGameRenderEndpointDiagnosticV2(1, observation, &diagnostic) == Status::Unsupported && unchanged(),
              "V2 diagnostic export retains payload version1 but allocation alone grants no private cache");
        Check(ReadGameRenderEndpointDiagnosticV1(1, reinterpret_cast<IGameRenderObservationV5*>(observation), &diagnostic) == Status::InvalidArgument &&
                  ReadGameRenderEndpointDiagnosticV2(1, reinterpret_cast<IGameRenderObservationV6*>(historical), &diagnostic) == Status::InvalidArgument &&
                  unchanged(),
              "diagnostic V1 and V2 registries reject the other exact interface without dereferencing it");
        for (const auto version : {0u, 2u, 6u, std::numeric_limits<std::uint32_t>::max()})
            Check(ReadGameRenderEndpointDiagnosticV2(version, observation, &diagnostic) == Status::InvalidArgument && unchanged(),
                  "V2 export name does not change its counted diagnostic payload version argument");
        Check(ReadGameRenderEndpointDiagnosticV2(1, nullptr, &diagnostic) == Status::InvalidArgument && unchanged() &&
                  ReadGameRenderEndpointDiagnosticV2(1, observation, nullptr) == Status::InvalidArgument,
              "API6 diagnostic validates null arguments before native or output access");
        auto* inaccessible = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
        Check(inaccessible != nullptr, "reserve unreadable opaque API6 lease validation page");
        Check(ReadGameRenderEndpointDiagnosticV2(1, static_cast<IGameRenderObservationV6*>(inaccessible),
                                                 static_cast<RenderEndpointDiagnosticV1*>(inaccessible)) == Status::InvalidArgument,
              "unregistered API6 lease cannot cause a vtable read or even an unreadable output write");
        Check(VirtualFree(inaccessible, 0, MEM_RELEASE) != 0, "release the owned API6 validation page");

        // Preserve the entire larger state, including historical prefixes and
        // every native-dispatch word, when a foreign thread asks for a read.
        RenderObservationStateV6 foreignState;
        std::memset(&foreignState, 0xa5, sizeof(foreignState));
        std::array<unsigned char, sizeof(foreignState)> before;
        std::memcpy(before.data(), &foreignState, before.size());
        Status      foreignBegin = Status::Ok, foreignRead = Status::Ok, foreignEnd = Status::Ok, foreignDiagnostic = Status::Ok;
        std::thread foreign(
            [&]
            {
                foreignBegin = observation->Begin();
                foreignRead = observation->ReadState(foreignState);
                foreignDiagnostic = ReadGameRenderEndpointDiagnosticV2(1, observation, &diagnostic);
                foreignEnd = observation->End();
                observation->Release();
            });
        foreign.join();
        Check(foreignBegin == Status::WrongThread && foreignRead == Status::WrongThread && foreignEnd == Status::WrongThread &&
                  foreignDiagnostic == Status::WrongThread && unchanged() && std::memcmp(&foreignState, before.data(), before.size()) == 0,
              "foreign API6 calls preserve the full larger output and retain the owner lease");
        Check(observation->Begin() == Status::Unsupported && observation->ReadState(state) == Status::Ok && state.base.base.base.base.requestedRoute == 2 &&
                  state.base.base.base.base.base.generation && state.base.base.base.base.base.failureReason && !state.base.base.base.base.base.bound &&
                  !state.base.base.base.base.base.active && !state.base.base.base.base.base.modulePinned && !state.base.base.base.base.base.pendingCleanup &&
                  !state.base.base.base.gpu.attempts && empty(state.base.boundary) && empty(state.nativeDispatch),
              "actual nongame module host rejects API6 before callsite patching and retains zero COM/native-dispatch tails");
        Check(observation->End() == Status::Ok && observation->End() == Status::Ok && observation->ReadState(state) == Status::Ok &&
                  empty(state.base.boundary) && empty(state.nativeDispatch) &&
                  ReadGameRenderEndpointDiagnosticV2(1, observation, &diagnostic) == Status::Unsupported && unchanged(),
              "safe owner End remains idempotent and readable without inventing unsupported API6 diagnostics");
        const auto released = reinterpret_cast<std::uintptr_t>(observation);
        observation->Release();
        Check(ReadGameRenderEndpointDiagnosticV2(1, reinterpret_cast<IGameRenderObservationV6*>(released), &diagnostic) == Status::InvalidArgument &&
                  unchanged() && ReadGameRenderEndpointDiagnosticV2(1, other, &diagnostic) == Status::Unsupported,
              "releasing a non-head V6 entry removes only its identity and preserves the independent live lease");
        const auto        racing = reinterpret_cast<std::uintptr_t>(other);
        std::atomic<bool> ready{false}, releaseStarted{false};
        bool              racePreserved = true;
        std::thread       reader(
            [&]
            {
                RenderEndpointDiagnosticV1 output = diagnosticSentinel;
                ready.store(true);
                while (!releaseStarted.load())
                    std::this_thread::yield();
                for (unsigned index = 0; index < 10000; ++index)
                {
                    const auto status = ReadGameRenderEndpointDiagnosticV2(1, reinterpret_cast<IGameRenderObservationV6*>(racing), &output);
                    if ((status != Status::WrongThread && status != Status::InvalidArgument && status != Status::Unsupported) ||
                        std::memcmp(&output, &diagnosticSentinel, sizeof(output)))
                        racePreserved = false;
                }
            });
        while (!ready.load())
            std::this_thread::yield();
        releaseStarted.store(true);
        other->Release();
        reader.join();
        Check(racePreserved &&
                  ReadGameRenderEndpointDiagnosticV2(1, reinterpret_cast<IGameRenderObservationV6*>(racing), &diagnostic) == Status::InvalidArgument &&
                  unchanged(),
              "API6 read versus Release race preserves rejected output and excludes the final destroyed identity");
        Check(ReadGameRenderEndpointDiagnosticV1(1, historical, &diagnostic) == Status::Unsupported && unchanged(),
              "API6 registration and cleanup cannot grant or destroy a historical V5 diagnostic capability");
        historical->Release();
    }
}

int main()
{
    static_assert(sizeof(RenderDeviceLeaseV1) == 48 && offsetof(RenderDeviceLeaseV1, device) == 8 && offsetof(RenderDeviceLeaseV1, generation) == 16);
    static_assert(std::is_nothrow_invocable_r_v<Status, decltype(RequestGameRenderDeviceLeaseV1), std::uint32_t, IGameRenderObservationV6*>);
    static_assert(std::is_nothrow_invocable_r_v<Status, decltype(TakeGameRenderDeviceLeaseV1), std::uint32_t, IGameRenderObservationV6*, RenderDeviceLeaseV1*>);
    static_assert(AdapterAbiVersion == 6 && AdapterControllerMask == 0xfffff);
    static_assert(static_cast<std::uint64_t>(AdapterCapability::NativeMovementWriteCandidate) == 256);
    static_assert(NativeDeviceStateCount == 127 && sizeof(NativeDeviceState) == 508);
    static_assert(static_cast<std::uint64_t>(AdapterCapability::NativeDeviceStateCandidateRead) == 128);
    static_assert(NativePadBindingCapabilities == 64, "A raw candidate read must not advertise the unverified EngineInputRead capability");
    static_assert((NativePadBindingCapabilities & static_cast<std::uint64_t>(AdapterCapability::EngineInputRead)) == 0);
    static_assert(std::is_nothrow_invocable_v<decltype(CreateGameAdapter), std::uint32_t>);
    static_assert(RenderObservationApiVersionV5 == 5);
    static_assert(RenderObservationApiVersionV6 == 6 && sizeof(FrameBoundaryObservationState) == 104);
    static_assert(std::is_standard_layout_v<RenderObservationStateV6> &&
                  offsetof(RenderObservationStateV6, nativeDispatch) == sizeof(RenderObservationStateV5));
    static_assert(sizeof(RenderObservationStateV6) == sizeof(RenderObservationStateV5) + sizeof(FrameBoundaryObservationState));
    static_assert(std::is_nothrow_invocable_v<decltype(CreateGameRenderObservationV6), std::uint32_t, CCoreInterface*, RenderObservationRoute>);
    static_assert(std::is_nothrow_invocable_v<decltype(CreateGameRenderObservationV5), std::uint32_t, CCoreInterface*, RenderObservationRoute>);
    static_assert(RenderEndpointDiagnosticVersionV1 == 1 && sizeof(RenderEndpointDiagnosticV1) == 80);
    static_assert(RenderRootsDiagnosticVersionV1 == 1 && sizeof(RenderRootsDiagnosticV1) == 56);
    static_assert(std::is_standard_layout_v<RenderRootsDiagnosticV1> && std::is_trivially_copyable_v<RenderRootsDiagnosticV1>);
    static_assert(
        std::is_nothrow_invocable_r_v<Status, decltype(ReadGameRenderRootsDiagnosticV1), std::uint32_t, IGameRenderObservationV6*, RenderRootsDiagnosticV1*>);
    static_assert(RenderSetupDiagnosticVersionV1 == 1 && sizeof(RenderSetupDiagnosticV1) == 72);
    static_assert(std::is_standard_layout_v<RenderSetupDiagnosticV1> && std::is_trivially_copyable_v<RenderSetupDiagnosticV1>);
    static_assert(
        std::is_nothrow_invocable_r_v<Status, decltype(ReadGameRenderSetupDiagnosticV1), std::uint32_t, IGameRenderObservationV6*, RenderSetupDiagnosticV1*>);

    static_assert(std::is_standard_layout_v<RenderEndpointDiagnosticV1> && std::is_trivially_copyable_v<RenderEndpointDiagnosticV1>);
    static_assert(std::is_nothrow_invocable_r_v<Status, decltype(ReadGameRenderEndpointDiagnosticV1), std::uint32_t, IGameRenderObservationV5*,
                                                RenderEndpointDiagnosticV1*>);
    static_assert(std::is_nothrow_invocable_r_v<Status, decltype(ReadGameRenderEndpointDiagnosticV2), std::uint32_t, IGameRenderObservationV6*,
                                                RenderEndpointDiagnosticV1*>);
    try
    {
        TestFactoryAndLifecycle();
        TestCoverageAndIsolation();
        TestInvalidObservations();
        TestThreadOwnership();
        TestMtaInput();
        TestNativeInputRejection();
        TestNativeDeviceRejection();
        TestNativeMovementRejection();
        TestNativeMovementEligibilityRejection();
        // Exercise historical factory layouts after the additive lease has
        // completed, so API5 cannot silently replace their module contract.
        TestRenderObservationV5FactoryRejection();
        TestEndpointDiagnosticLeaseRejection();
        TestRenderObservationV6Contract();
        TestDeviceLeaseContract();
        TestSetupDiagnosticLeaseRejection();
        TestRootsDiagnosticLeaseRejection();
        TestRenderObservationFactoryRejection();
        TestRenderObservationV2FactoryRejection();
        TestRenderObservationV3FactoryRejection();
        TestRenderObservationV4FactoryRejection();
        std::cout << "Passed " << checks << " MTA Game DE adapter checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
