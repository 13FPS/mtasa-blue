/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual Core configured controls in a test-only Core DLL
 *****************************************************************************/

// Link this translation unit only into the disposable fixture DLL, alongside
// the normal Core objects. No test exports are added to the installed client.
#include "../../Client/core/StdInc.h"
#include "../../Client/core/CGameAdapterModule.h"
#include <xml/CXML.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
            for (const auto* key : {"i", "k", "j", "u", "v", "w", "h", "lshift", "rshift", "num_8", "arrow_u", "mouse_wheel_up", "mouse_wheel_down"})
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

    struct PhysicalKeys
    {
        std::array<bool, 256>     down{};
        std::vector<unsigned int> sampled;
        static bool               Sample(unsigned int code, void* context)
        {
            auto& keys = *static_cast<PhysicalKeys*>(context);
            Check(code < keys.down.size(), "table-derived sampler receives a bounded physical virtual key");
            keys.sampled.push_back(code);
            return keys.down[code];
        }
    };

    class ActualXml
    {
    public:
        ~ActualXml()
        {
            if (file)
                xml->DeleteXML(file);
            if (release)
                release();
            if (module)
                FreeLibrary(module);
        }
        void Open()
        {
            Check(GetModuleHandleW(L"xmll.dll") == nullptr, "fixture owns the real XML singleton exclusively");
            static int anchor = 0;
            HMODULE    self = nullptr;
            Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&anchor),
                                     &self) != FALSE,
                  "fixture module identity is available");
            std::array<wchar_t, 32768> filename{};
            const auto                 length = GetModuleFileNameW(self, filename.data(), static_cast<DWORD>(filename.size()));
            Check(length && length < filename.size(), "owned fixture module path is bounded");
            const auto directory = std::filesystem::path(std::wstring(filename.data(), length)).parent_path();
            module = LoadLibraryExW((directory / L"xmll.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            Check(module != nullptr, "actual x64 MTA XML module loads beside the fixture");
            using Initialize = CXML* (*)(const char*);
            const auto initialize = reinterpret_cast<Initialize>(GetProcAddress(module, "InitXMLInterface"));
            release = reinterpret_cast<void (*)()>(GetProcAddress(module, "ReleaseXMLInterface"));
            Check(initialize && release, "actual XML ABI factory and release exports are present");
            // A read-only observation must not run the XML module's optional
            // save-file recovery, which can rename unrelated saved files.
            xml = initialize(nullptr);
            Check(xml != nullptr, "actual XML interface initializes without recovery writes");
            const auto savedPath = ToUTF8((directory / L"configured-input.xml").wstring());
            file = xml->CreateXML(savedPath.c_str(), false, true);
            Check(file && file->Parse(), "actual XML module parses the preserved saved configuration file");
            Check(!file->Write(), "configuration handle enforces real read-only XML behavior");
        }
        HMODULE   module = nullptr;
        CXML*     xml = nullptr;
        CXMLFile* file = nullptr;
        void (*release)() = nullptr;
    };

    void SavedXmlAndPhysicalPolling(CGameAdapterModule& module)
    {
        ActualXml xml;
        xml.Open();
        auto* node = xml.file->GetRootNode()->FindSubNode("binds");
        Check(node && node->GetSubNodeCount() > 0, "saved nonempty MTA binds node is available");
        bool legacyResourceActive = false;
        {
            Scenario legacy(module);
            Check(legacy.keys.LoadFromXML(node), "actual classic loader accepts nonempty configured XML without CGame");
            Check(legacy.keys.Count(KeyBindType::GTA_CONTROL) == 8 && legacy.keys.Count(KeyBindType::COMMAND) == 3,
                  "classic parser retains configured deduplication and command precedence");
            auto* resource = legacy.keys.GetBindFromCommand("fixture_resource");
            Check(resource != nullptr, "classic parser retains the saved resource command");
            legacyResourceActive = resource->isActive;
        }
        {
            Scenario test(module);
            Check(!test.keys.LoadConfiguredBindsFromXML(nullptr), "configured-only loader rejects missing XML without importing game defaults");
            auto empty = xml.xml->ParseString("<binds/>");
            Check(empty && test.keys.LoadConfiguredBindsFromXML(empty->node) && test.keys.Count() == 0,
                  "configured-only empty XML remains empty without a CGame/default dependency");
            Check(test.keys.LoadConfiguredBindsFromXML(node), "actual configured-only parser ingests real saved XML");
            Check(test.keys.Count(KeyBindType::GTA_CONTROL) == 8 && test.keys.Count(KeyBindType::COMMAND) == 3,
                  "saved aliases, case-insensitive deduplication and ignored invalid records match classic loading");
            Check(!test.keys.GTAControlExists("k", "backwards") && !test.keys.GTAControlExists("v", "forwards"),
                  "present command attributes take precedence over control attributes, including empty commands");
            auto* resource = test.keys.GetBindFromCommand("fixture_resource");
            Check(resource && resource->isActive == legacyResourceActive && resource->originalScriptKey == "h" && resource->resource == "res",
                  "saved resource command metadata and activation match the unchanged classic parser");
            Check(test.keys.AddFunction("i", KeyFunction) && test.keys.AddControlFunction("jump", ControlCallback, true),
                  "physical polling fixture includes command/key-function and excluded-action callbacks");
            PhysicalKeys physical;
            const auto   movement = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(BACKWARDS) | MtaInput::ActionBit(LEFT) | MtaInput::ActionBit(RIGHT) |
                                  MtaInput::ActionBit(SPRINT) | MtaInput::ActionBit(WALK);
            auto poll = [&](bool focused, std::uint64_t mask)
            {
                physical.sampled.clear();
                return test.keys.PollConfiguredControlKeys(focused, PhysicalKeys::Sample, &physical, mask);
            };
            physical.down['I'] = physical.down[VK_LSHIFT] = true;
            auto result = poll(true, movement);
            Check(!result.armed && result.eligibleKeys == 4 && result.unsupportedKeys == 2 && result.supportedBindings == 4 && result.ignoredBindings == 4,
                  "poll reports actual unique supported keys and skipped route/action binding counts");
            Check(test.Pulse().heldActions == 0, "startup held keys cannot enter configured control state");
            Check(physical.sampled.size() == 4 && std::count(physical.sampled.begin(), physical.sampled.end(), VK_LSHIFT) == 1 &&
                      std::count(physical.sampled.begin(), physical.sampled.end(), VK_RSHIFT) == 1 &&
                      std::find(physical.sampled.begin(), physical.sampled.end(), 'J') == physical.sampled.end(),
                  "sampler uses actual Shift sides once and never samples excluded fire or ambiguous devices");
            physical.down = {};
            Check(poll(true, movement).armed, "all eligible keys released establishes the initial neutral baseline");
            test.Pulse();
            physical.down['I'] = true;
            physical.down[VK_LSHIFT] = true;
            physical.down['J'] = true;
            Check(poll(true, movement).changedKeys == 2, "new physical presses update only supported configured key states");
            auto frame = test.Pulse();
            Check(frame.controller.LeftStickY == -128 && frame.controller.ButtonCross == 255 && frame.controller.ButtonSquare == 0 &&
                      frame.controller.ButtonCircle == 0 && keyFunctions == 0 && presses == 0,
                  "real remapped walking/sprint publish while commands, key functions and excluded shared-key actions stay inert");
            physical.down[VK_LSHIFT] = false;
            physical.down[VK_RSHIFT] = true;
            poll(true, movement);
            frame = test.Pulse();
            Check(frame.controller.ButtonCross == 0 && frame.controller.m_bPedWalk == 255,
                  "left and right Shift remain independent configured sprint/walk inputs");
            physical.down['U'] = true;
            physical.down['I'] = false;
            poll(true, movement);
            Check(test.Pulse().controller.LeftStickY == -128, "physical alias release retains another held configured alias");
            result = poll(false, movement);
            Check(!result.armed && !result.focused && physical.sampled.empty() && test.Pulse().heldActions == 0,
                  "focus loss releases real binds and key caches without sampling background physical state");
            Check(!poll(true, movement).armed && test.Pulse().heldActions == 0, "refocus with held keys requires another neutral baseline");
            physical.down['U'] = false;
            Check(!poll(true, movement).armed, "a still-held configured Shift prevents rearming");
            physical.down = {};
            Check(poll(true, movement).armed, "refocus re-arms only after all eligible keys release");
            physical.down['I'] = physical.down[VK_LSHIFT] = true;
            poll(true, movement);
            Check(test.Pulse().controller.LeftStickY == -128, "new post-focus press reaches the actual adapter frame");
            result = poll(true, MtaInput::ActionBit(SPRINT));
            Check(!result.armed && result.eligibleKeys == 1 && result.supportedBindings == 1 && result.ignoredBindings == 7 &&
                      physical.sampled == std::vector<unsigned int>{VK_LSHIFT} && test.Pulse().heldActions == 0,
                  "changing allowed action scope releases excluded binds and requires a fresh restricted baseline");
            physical.down[VK_LSHIFT] = false;
            Check(poll(true, MtaInput::ActionBit(SPRINT)).armed, "restricted input baseline ignores physically held excluded keys");
            physical.down[VK_LSHIFT] = true;
            poll(true, MtaInput::ActionBit(SPRINT));
            frame = test.Pulse();
            Check(frame.controller.ButtonCross == 255 && frame.controller.LeftStickY == 0, "restricted polling publishes only its explicitly allowed action");
            test.keys.RemoveAllGTAControls();
            Check(!poll(true, MtaInput::ActionBit(SPRINT)).armed && physical.sampled.empty() && test.Pulse().heldActions == 0,
                  "removing the last eligible binding disarms polling and clears aggregate configured controls");
        }
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
        SavedXmlAndPhysicalPolling(module);
        Check(GetModuleHandleW(L"xmll.dll") == nullptr, "actual XML wrappers/interface/module are released before Core cleanup");
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
