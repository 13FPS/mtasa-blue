/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve existing Core bind-state gating before callback dispatch
 *****************************************************************************/

#include "../../Shared/sdk/CControlBindState.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
    int  checks = 0;
    void Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }

    constexpr std::array<eControllerAction, 44> actions = {FIRE,
                                                           NEXT_WEAPON,
                                                           PREVIOUS_WEAPON,
                                                           FORWARDS,
                                                           BACKWARDS,
                                                           LEFT,
                                                           RIGHT,
                                                           ZOOM_IN,
                                                           ZOOM_OUT,
                                                           ENTER_EXIT,
                                                           CHANGE_CAMERA,
                                                           JUMP,
                                                           SPRINT,
                                                           LOOK_BEHIND,
                                                           CROUCH,
                                                           ACTION,
                                                           WALK,
                                                           VEHICLE_FIRE,
                                                           VEHICLE_SECONDARY_FIRE,
                                                           VEHICLE_LEFT,
                                                           VEHICLE_RIGHT,
                                                           STEER_FORWARDS_DOWN,
                                                           STEER_BACK_UP,
                                                           ACCELERATE,
                                                           BRAKE_REVERSE,
                                                           RADIO_NEXT,
                                                           RADIO_PREVIOUS,
                                                           RADIO_USER_TRACK_SKIP,
                                                           HORN,
                                                           SUB_MISSION,
                                                           HANDBRAKE,
                                                           VEHICLE_LOOK_LEFT,
                                                           VEHICLE_LOOK_RIGHT,
                                                           VEHICLE_LOOK_BEHIND,
                                                           VEHICLE_MOUSE_LOOK,
                                                           SPECIAL_CONTROL_LEFT,
                                                           SPECIAL_CONTROL_RIGHT,
                                                           SPECIAL_CONTROL_DOWN,
                                                           SPECIAL_CONTROL_UP,
                                                           AIM_WEAPON,
                                                           CONVERSATION_YES,
                                                           CONVERSATION_NO,
                                                           GROUP_CONTROL_FORWARDS,
                                                           GROUP_CONTROL_BACK};

    // Frozen policy from CKeyBinds.cpp blob4c8961bd338fa0ceec18f7c8e0dec8f26fc9de4e,
    // before extraction. The original outer branch enters the callback loop;
    // it does so even when a disabled or wheel-zoom control is not assigned.
    template <class Query>
    bool Original(eControllerAction action, bool down, bool enabled, bool mouseWheel, bool& bindState, bool& controlState, Query query)
    {
        if (action == RADIO_USER_TRACK_SKIP)
            return false;
        bindState = down;
        if (down || !query())
        {
            bool zoomWheel = mouseWheel && (action == ZOOM_IN || action == ZOOM_OUT);
            if (enabled && !zoomWheel)
                controlState = down;
            return true;
        }
        return false;
    }

    void ExhaustivePolicy()
    {
        for (auto action : actions)
        {
            for (unsigned int bits = 0; bits < 64; ++bits)
            {
                const bool down = (bits & 1) != 0;
                const bool enabled = (bits & 2) != 0;
                const bool wheel = (bits & 4) != 0;
                bool       actualBind = (bits & 8) != 0, expectedBind = actualBind;
                bool       actualControl = (bits & 16) != 0, expectedControl = actualControl;
                const bool otherHeld = (bits & 32) != 0;
                int        actualQueries = 0, expectedQueries = 0;
                bool       actualSeenBind = actualBind, expectedSeenBind = expectedBind;
                const bool expected = Original(action, down, enabled, wheel, expectedBind, expectedControl,
                                               [&]
                                               {
                                                   ++expectedQueries;
                                                   expectedSeenBind = expectedBind;
                                                   return otherHeld;
                                               });
                const bool actual = MtaInput::UpdateControlBindState(action, down, enabled, wheel, actualBind, actualControl,
                                                                     [&]
                                                                     {
                                                                         ++actualQueries;
                                                                         actualSeenBind = actualBind;
                                                                         return otherHeld;
                                                                     });
                Check(actual == expected, "callback dispatch gate differs from frozen original");
                Check(actualBind == expectedBind && actualControl == expectedControl, "bind/control mutation differs from original");
                Check(actualQueries == expectedQueries && actualSeenBind == expectedSeenBind, "multi-bind query order or laziness differs from original");
                Check(actualQueries == ((!down && action != RADIO_USER_TRACK_SKIP) ? 1 : 0), "only eligible key-up queries other held bindings");
                if (actualQueries)
                    Check(!actualSeenBind, "keyup stores released bind state before querying other binds");
            }
        }
    }

    void MultipleBindings()
    {
        bool first = false, second = false, control = false;
        Check(MtaInput::UpdateControlBindState(FORWARDS, true, true, false, first, control, [&] { return second; }) && first && control,
              "first configured binding activates its control");
        Check(MtaInput::UpdateControlBindState(FORWARDS, true, true, false, second, control, [&] { return first; }) && second && control,
              "a second configured binding can remain held independently");
        Check(!MtaInput::UpdateControlBindState(FORWARDS, false, true, false, first, control, [&] { return second; }) && !first && second && control,
              "releasing one alias cannot release a control still held by another binding");
        Check(MtaInput::UpdateControlBindState(FORWARDS, false, true, false, second, control, [&] { return first; }) && !second && !control,
              "last alias release reaches the original callback loop");
    }

    void QueryExceptionsAndCallbackPolicy()
    {
        bool          bind = false, control = true;
        volatile bool throwQuery = true;
        auto          forbidden = [&]() -> bool
        {
            if (throwQuery)
                throw std::runtime_error("query");
            return false;
        };
        Check(MtaInput::UpdateControlBindState(WALK, true, true, false, bind, control, forbidden), "keydown must not invoke the multi-bind query");
        bool caught = false;
        try
        {
            MtaInput::UpdateControlBindState(WALK, false, true, false, bind, control, forbidden);
        }
        catch (const std::runtime_error&)
        {
            caught = true;
        }
        Check(caught && !bind && control, "query failure propagates after bind storage and before control assignment");
        bind = true;
        control = true;
        Check(!MtaInput::UpdateControlBindState(RADIO_USER_TRACK_SKIP, false, true, false, bind, control, forbidden) && bind && control,
              "radio skip rejects before state changes or a lazy query");
        Check(MtaInput::UpdateControlBindState(ZOOM_IN, false, true, true, bind, control, [] { return false; }) && !bind && control,
              "wheel zoom suppresses assignment while still entering function callbacks");
        bind = false;
        control = false;
        Check(MtaInput::UpdateControlBindState(SPRINT, true, false, false, bind, control, forbidden) && bind && !control,
              "disabled control still permits original callback dispatch");
        Check(MtaInput::UpdateControlBindState(SPRINT, true, false, false, bind, control, forbidden),
              "repeated state still reaches callback-state filtering in Core");
    }
}

int main()
{
    try
    {
        ExhaustivePolicy();
        MultipleBindings();
        QueryExceptionsAndCallbackPolicy();
        std::cout << "Passed " << checks << " reused MTA control-bind-state checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Control bind state failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
