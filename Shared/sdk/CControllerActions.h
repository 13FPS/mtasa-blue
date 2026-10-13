/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA action identities without classic key/platform definitions
 *****************************************************************************/

#pragma once

// Preserve action values used by existing bindings and sync. Another backend
// needs these identities, not the classic engine's keyboard configuration API.
enum eControllerAction
{
    FIRE = 0,

    NEXT_WEAPON = 2,
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

    VEHICLE_LOOK_LEFT = 0x22,
    VEHICLE_LOOK_RIGHT,
    VEHICLE_LOOK_BEHIND,
    VEHICLE_MOUSE_LOOK,
    SPECIAL_CONTROL_LEFT,
    SPECIAL_CONTROL_RIGHT,
    SPECIAL_CONTROL_DOWN,
    SPECIAL_CONTROL_UP,

    AIM_WEAPON = 0x2D,

    CONVERSATION_YES = 0x2F,
    CONVERSATION_NO,
    GROUP_CONTROL_FORWARDS,
    GROUP_CONTROL_BACK,
};
