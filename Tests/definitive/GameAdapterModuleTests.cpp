/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the MTA-owned DE module contract without engine bindings
 *****************************************************************************/

#include <game/CGameAdapter.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

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
        Check(CreateGameAdapter(0) == nullptr && CreateGameAdapter(1) == nullptr && CreateGameAdapter(AdapterAbiVersion + 1) == nullptr,
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
        std::array<Status, 8> statuses{};
        LocalObservation      foreignOutput = Observation(999);
        MtaInputState         foreignInput;
        foreignInput.current.ButtonCross = 19;
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
                            adapter.value->GetMtaInputState(foreignInput)};
            });
        other.join();
        for (const auto status : statuses)
            Check(status == Status::WrongThread, "foreign threads cannot operate or tear down the adapter");
        Check(foreignOutput.sampleAtMs == 999, "foreign read leaves caller output untouched");
        Check(foreignInput.current.ButtonCross == 19, "foreign logical input read leaves caller output untouched");
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
}

int main()
{
    static_assert(AdapterAbiVersion == 2 && AdapterControllerMask == 0xfffff);
    static_assert(std::is_nothrow_invocable_v<decltype(CreateGameAdapter), std::uint32_t>);
    try
    {
        TestFactoryAndLifecycle();
        TestCoverageAndIsolation();
        TestInvalidObservations();
        TestThreadOwnership();
        TestMtaInput();
        std::cout << "Passed " << checks << " MTA Game DE adapter checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
