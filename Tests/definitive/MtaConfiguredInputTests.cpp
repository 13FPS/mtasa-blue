/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual Core configured controls in a test-only Core DLL
 *****************************************************************************/

// Link this translation unit only into the disposable fixture DLL, alongside
// the normal Core objects. No test exports are added to the installed client.
#include "../../Client/core/StdInc.h"
#include "../../Client/core/CGameAdapterModule.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
    std::uint32_t         checks = 0;
    std::array<char, 512> failure{};
    CKeyBinds*            callbackKeys = nullptr;
    MtaInput::Context*    callerContext = nullptr;
    int                   presses = 0, releases = 0, keyFunctions = 0;
    bool                  mutateOnRelease = false, removeOnPress = false, throwOnPress = false;

    void Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }

    void ControlCallback(CControlFunctionBind* bind)
    {
        if (bind->triggerState)
        {
            ++presses;
            if (removeOnPress)
                Check(callbackKeys->RemoveGTAControl("i", "left"), "callback removes a later same-key control bind");
            if (throwOnPress)
                throw std::runtime_error("intentional configured-control callback exception");
        }
        else
        {
            ++releases;
            if (mutateOnRelease)
            {
                Check(callbackKeys->ProcessConfiguredControlKey("v", true), "transition callback changes held controls before capture");
                *callerContext = {false, true, true, true, true};
            }
        }
    }

    void KeyFunction(CKeyFunctionBind*)
    {
        ++keyFunctions;
    }

    class Scenario
    {
    public:
        explicit Scenario(CGameAdapterModule& module) : adapter(module), keys(nullptr)
        {
            callbackKeys = &keys;
            callerContext = nullptr;
            presses = releases = keyFunctions = 0;
            mutateOnRelease = removeOnPress = throwOnPress = false;
            keys.SetAllControlsEnabled(true, false, true);
            Check(adapter.Reset(), "actual adapter resets its per-owner synthesis history");
        }

        ~Scenario()
        {
            // Core tables are global. One fixture owner releases every key it
            // used and restores enabled controls before the next owner exists.
            keys.RemoveAllControlFunctions();
            keys.RemoveAllFunctions();
            keys.RemoveAllCommands();
            keys.CallAllGTAControlBinds(CONTROL_BOTH, false);
            for (const auto* key : {"i", "k", "j", "u", "v", "w", "arrow_u", "mouse_wheel_up", "mouse_wheel_down"})
                keys.ProcessConfiguredControlKey(key, false, false);
            keys.FinishConfiguredControlFrame();
            keys.SetAllControlsEnabled(true, false, true);
            callbackKeys = nullptr;
            callerContext = nullptr;
        }

        void Bind(const char* key, const char* control)
        {
            Check(keys.AddGTAControl(key, control), "actual CKeyBinds accepts the configured key/control names");
        }

        CKeyBinds::ConfiguredControlFrame Pulse(const MtaInput::Context& context = {}, bool finish = true)
        {
            const auto frame = keys.BuildConfiguredControlFrame(context);
            Check(adapter.SubmitControlActions({frame.heldActions, frame.context}), "actual Core consumer submits the configured action frame");
            Check(adapter.Pulse(), "actual Game DE module accepts its owning-thread pulse");
            mta::game_de::MtaInputState state;
            Check(adapter.GetMtaInputState(state), "actual adapter exposes its canonical current/previous result");
            Check(std::memcmp(&state.current, &frame.controller, sizeof(state.current)) == 0,
                  "actual Core digital controller matches the actual Game DE synthesis");
            Check(std::memcmp(&state.previous, &previous, sizeof(previous)) == 0, "actual adapter preserves the preceding configured frame");
            previous = frame.controller;
            if (finish)
                keys.FinishConfiguredControlFrame();
            return frame;
        }

        CGameAdapterModule& adapter;
        CKeyBinds           keys;
        CControllerState    previous;
    };

    void RemappingAndAliases(CGameAdapterModule& module)
    {
        Scenario test(module);
        test.Bind("I", "FORWARDS");
        test.Bind("arrow_u", "forwards");
        test.Bind("u", "sprint");
        Check(!test.keys.ProcessConfiguredControlKey(nullptr, true) && !test.keys.ProcessConfiguredControlKey("unknown_key", true),
              "invalid key names cannot access bind storage");
        Check(!test.keys.ProcessConfiguredControlKey("w", true), "a classic default key is not silently substituted for the configured key");
        Check(test.Pulse().controller.LeftStickY == 0, "an unbound key leaves movement neutral");
        Check(test.keys.ProcessConfiguredControlKey("i", true), "configured remapped key activates its actual control bind");
        Check(test.Pulse().controller.LeftStickY == -128, "configured forwards uses canonical MTA movement");
        Check(test.keys.ProcessConfiguredControlKey("arrow_u", true), "a second configured alias can be held independently");
        Check(test.keys.ProcessConfiguredControlKey("i", false), "first alias releases");
        Check(test.Pulse().controller.LeftStickY == -128, "releasing one alias preserves the other held alias");
        Check(test.keys.ProcessConfiguredControlKey("u", true), "configured sprint key activates sprint");
        const auto sprint = test.Pulse();
        Check(sprint.controller.ButtonCross == 255 && sprint.controller.LeftStickY == -128, "configured sprint retains the canonical 255 button value");
        Check(test.keys.ProcessConfiguredControlKey("arrow_u", false), "last movement alias releases");
        Check(test.keys.ProcessConfiguredControlKey("u", false), "sprint releases");
        Check(test.Pulse().heldActions == 0, "last releases publish a neutral canonical frame");
        bool held = true;
        Check(test.keys.GetKeyStateByName("i", held) && !held, "actual named-key state records configured release");
    }

    void PressGatingAndCallbacks(CGameAdapterModule& module)
    {
        Scenario test(module);
        test.Bind("i", "forwards");
        Check(test.keys.AddControlFunction("forwards", ControlCallback, true), "press callback uses actual control-function binding");
        Check(test.keys.AddControlFunction("forwards", ControlCallback, false), "release callback uses actual control-function binding");
        Check(test.keys.AddFunction("i", KeyFunction), "fixture includes a key function that controls-only dispatch must ignore");
        Check(test.keys.AddCommand("i", "fixture_command", nullptr, true), "fixture includes a command that controls-only dispatch must ignore");
        Check(!test.keys.ProcessConfiguredControlKey("i", true, false), "caller can suppress a control press while input belongs to a cursor");
        Check(test.Pulse().heldActions == 0 && presses == 0 && keyFunctions == 0, "blocked control press invokes no command or key function");
        Check(test.keys.ProcessConfiguredControlKey("i", true), "permitted press reaches configured controls");
        Check(test.keys.ProcessConfiguredControlKey("i", true), "repeated held key can be reported without replaying a press callback");
        Check(presses == 1 && keyFunctions == 0, "actual control-function state suppresses repeated down callbacks");
        Check(test.keys.ProcessConfiguredControlKey("i", false, false), "cursor press suppression never strands an already-held control release");
        Check(test.Pulse().heldActions == 0 && releases == 1, "release reaches configured callbacks and neutralizes movement");
        Check(test.keys.SetControlEnabled("forwards", false), "actual configured control can be disabled");
        test.keys.ProcessConfiguredControlKey("i", true);
        Check(test.Pulse().heldActions == 0 && presses == 2, "disabled controls retain classic callback dispatch without assigning their control");
        Check(test.keys.SetControlEnabled("forwards", true), "re-enabling resets aggregate control from the actual held binding");
        Check(test.Pulse().controller.LeftStickY == -128, "re-enabled control reflects the still-held configured binding");
        test.keys.CallAllGTAControlBinds(CONTROL_BOTH, false);
        Check(test.Pulse().heldActions == 0, "existing MTA all-control release neutralizes the configured path");
    }

    void CallbackMutationAndExceptions(CGameAdapterModule& module)
    {
        for (const bool throws : {false, true})
        {
            Scenario test(module);
            test.Bind("i", "forwards");
            test.Bind("i", "left");
            Check(test.keys.AddControlFunction("forwards", ControlCallback, true), "mutation callback is registered through actual MTA binds");
            removeOnPress = true;
            throwOnPress = throws;
            bool caught = false;
            try
            {
                test.keys.ProcessConfiguredControlKey("i", true);
            }
            catch (const std::runtime_error&)
            {
                caught = true;
            }
            Check(caught == throws, "configured key dispatch preserves callback exception propagation");
            Check(test.keys.Count(KeyBindType::GTA_CONTROL) == 1, "callback-deleted later bind is reclaimed after dispatch, including exception exit");
            const auto frame = test.Pulse();
            Check(frame.controller.LeftStickY == -128 && frame.controller.LeftStickX == 0, "a callback-deleted later same-key bind is never dispatched");
            test.keys.RemoveAllControlFunctions();
            Check(test.keys.RemoveGTAControl("i", "forwards") && test.keys.Count(KeyBindType::GTA_CONTROL) == 0,
                  "processing ownership is restored so subsequent removal erases immediately");
        }
    }

    void ContextTransition(CGameAdapterModule& module)
    {
        Scenario test(module);
        test.Bind("i", "forwards");
        test.Bind("v", "accelerate");
        Check(test.keys.AddControlFunction("forwards", ControlCallback, false), "transition release callback is registered");
        test.keys.ProcessConfiguredControlKey("i", true);
        Check(test.Pulse().controller.LeftStickY == -128, "on-foot movement starts before context transition");
        MtaInput::Context context{true, false, false, false, false};
        callerContext = &context;
        mutateOnRelease = true;
        const auto vehicle = test.Pulse(context);
        Check(releases == 1 && !context.inVehicle && context.isDead, "transition callback really changes the caller's context object");
        Check(vehicle.context.inVehicle && !vehicle.context.isDead && !vehicle.context.hasDetonator,
              "frame context is the snapshot taken before transition callbacks");
        Check((vehicle.heldActions & MtaInput::ActionBit(FORWARDS)) == 0 && (vehicle.heldActions & MtaInput::ActionBit(ACCELERATE)) != 0 &&
                  vehicle.controller.ButtonCross == 255,
              "held actions are captured after callback mutations and departing on-foot releases");
        mutateOnRelease = false;
        Check(test.Pulse().heldActions == 0, "returning on foot releases vehicle controls before frame capture");
    }

    void MouseWheelBoundary(CGameAdapterModule& module)
    {
        Scenario test(module);
        test.Bind("mouse_wheel_up", "forwards");
        test.Bind("mouse_wheel_up", "zoom_in");
        Check(test.keys.AddControlFunction("zoom_in", ControlCallback, true), "wheel zoom callback uses actual MTA policy");
        Check(test.keys.ProcessConfiguredControlKey("mouse_wheel_up", true), "configured wheel press dispatches actual binds");
        const auto wheel = test.Pulse({}, false);
        Check(wheel.controller.LeftStickY == -128 && (wheel.heldActions & MtaInput::ActionBit(ZOOM_IN)) == 0 && presses == 1,
              "wheel movement is published once while wheel zoom keeps classic callback-only behavior");
        bool down = false;
        Check(test.keys.GetKeyStateByName("mouse_wheel_up", down) && down, "wheel key remains held until explicit post-publish finish");
        test.keys.FinishConfiguredControlFrame();
        Check(test.keys.GetKeyStateByName("mouse_wheel_up", down) && !down, "finish releases actual named wheel state");
        Check(test.Pulse().heldActions == 0, "next actual adapter frame is neutral after wheel finish");
    }

    void DirectionAndDeadContext(CGameAdapterModule& module)
    {
        Scenario test(module);
        test.Bind("i", "forwards");
        test.Bind("k", "backwards");
        test.keys.ProcessConfiguredControlKey("k", true);
        Check(test.Pulse().controller.LeftStickY == 128, "backwards establishes actual direction history");
        test.keys.ProcessConfiguredControlKey("k", false);
        test.keys.ProcessConfiguredControlKey("i", true);
        const auto dead = test.Pulse({false, false, true, false, false});
        Check(dead.controller.LeftStickY == 0 && (dead.heldActions & MtaInput::ActionBit(FORWARDS)),
              "dead context neutralizes the controller while retaining configured held actions");
        test.keys.ProcessConfiguredControlKey("k", true);
        Check(test.Pulse().controller.LeftStickY == -128, "dead frame does not advance classic opposing-direction history");
    }
}

extern "C" __declspec(dllexport) int MTA_RunConfiguredInputTests(std::uint32_t* count) noexcept
{
    checks = 0;
    failure = {};
    try
    {
        Check(count != nullptr && g_pCore == nullptr, "fixture has a count sink and no partial or fake Core facade");
        Check(GetModuleHandleW(L"game_de.dll") == nullptr, "actual adapter starts unloaded");
        int                ownerIdentity = 0;
        CGameAdapterModule module;
        Check(module.StartAdjacent(reinterpret_cast<CCoreInterface*>(&ownerIdentity)), "actual Core consumer owns the adjacent real Game DE module");
        Check(module.GetCapabilities() == 33 && !module.IsGameplayReady(), "configured synthesis does not advertise full game or native-write readiness");
        Check(!module.BindNativeInput(), "owned non-game host remains rejected by exact native image guard");
        RemappingAndAliases(module);
        PressGatingAndCallbacks(module);
        CallbackMutationAndExceptions(module);
        ContextTransition(module);
        MouseWheelBoundary(module);
        DirectionAndDeadContext(module);
        module.Stop();
        Check(!module.IsStarted() && GetModuleHandleW(L"game_de.dll") == nullptr, "actual consumer releases the real backend after configured input checks");
        *count = checks;
        return 0;
    }
    catch (const std::exception& error)
    {
        strncpy_s(failure.data(), failure.size(), error.what(), _TRUNCATE);
    }
    catch (...)
    {
        strncpy_s(failure.data(), failure.size(), "unexpected configured-input fixture exception", _TRUNCATE);
    }
    if (count)
        *count = checks;
    return 1;
}

extern "C" __declspec(dllexport) const char* MTA_ConfiguredInputTestFailure() noexcept
{
    return failure.data();
}
