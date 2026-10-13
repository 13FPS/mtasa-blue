/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's configured-action controller synthesis across backends
 *****************************************************************************/

#pragma once

#include <CControllerActions.h>
#include <CControllerState.h>
#include <cstdint>

namespace MtaInput
{
    constexpr std::uint64_t ActionBit(eControllerAction action) noexcept
    {
        const auto index = static_cast<unsigned int>(action);
        return index < 64 ? (std::uint64_t{1} << index) : 0;
    }

    // The existing enum is sparse. Its holes are not additional valid controls.
    inline constexpr std::uint64_t KnownActionMask =
        ActionBit(FIRE) | ActionBit(NEXT_WEAPON) | ActionBit(PREVIOUS_WEAPON) | ActionBit(FORWARDS) | ActionBit(BACKWARDS) | ActionBit(LEFT) |
        ActionBit(RIGHT) | ActionBit(ZOOM_IN) | ActionBit(ZOOM_OUT) | ActionBit(ENTER_EXIT) | ActionBit(CHANGE_CAMERA) | ActionBit(JUMP) | ActionBit(SPRINT) |
        ActionBit(LOOK_BEHIND) | ActionBit(CROUCH) | ActionBit(ACTION) | ActionBit(WALK) | ActionBit(VEHICLE_FIRE) | ActionBit(VEHICLE_SECONDARY_FIRE) |
        ActionBit(VEHICLE_LEFT) | ActionBit(VEHICLE_RIGHT) | ActionBit(STEER_FORWARDS_DOWN) | ActionBit(STEER_BACK_UP) | ActionBit(ACCELERATE) |
        ActionBit(BRAKE_REVERSE) | ActionBit(RADIO_NEXT) | ActionBit(RADIO_PREVIOUS) | ActionBit(RADIO_USER_TRACK_SKIP) | ActionBit(HORN) |
        ActionBit(SUB_MISSION) | ActionBit(HANDBRAKE) | ActionBit(VEHICLE_LOOK_LEFT) | ActionBit(VEHICLE_LOOK_RIGHT) | ActionBit(VEHICLE_LOOK_BEHIND) |
        ActionBit(VEHICLE_MOUSE_LOOK) | ActionBit(SPECIAL_CONTROL_LEFT) | ActionBit(SPECIAL_CONTROL_RIGHT) | ActionBit(SPECIAL_CONTROL_DOWN) |
        ActionBit(SPECIAL_CONTROL_UP) | ActionBit(AIM_WEAPON) | ActionBit(CONVERSATION_YES) | ActionBit(CONVERSATION_NO) | ActionBit(GROUP_CONTROL_FORWARDS) |
        ActionBit(GROUP_CONTROL_BACK);

    struct Context
    {
        bool inVehicle = false;
        bool hasDetonator = false;
        bool isDead = false;
        bool aimingWeapon = false;
        bool enteringVehicle = false;
    };

    struct DirectionHistory
    {
        bool lastForwards = false;
        bool lastBackwards = false;
        bool moveForwards = false;
        bool lastLeft = false;
        bool lastRight = false;
        bool moveLeft = false;
    };

    inline bool IsHeld(std::uint64_t heldActions, eControllerAction action) noexcept
    {
        return (heldActions & ActionBit(action)) != 0;
    }

    namespace detail
    {
        inline bool ControlForwardsBackWards(std::uint64_t heldActions, CControllerState& cs, DirectionHistory& history) noexcept
        {
            bool bCurrentStateForwards = IsHeld(heldActions, FORWARDS);
            bool bCurrentStateBackwards = IsHeld(heldActions, BACKWARDS);
            if (bCurrentStateForwards && !bCurrentStateBackwards)
            {
                history.lastForwards = true;
                history.lastBackwards = false;
            }
            else if (!bCurrentStateForwards && bCurrentStateBackwards)
            {
                history.lastForwards = false;
                history.lastBackwards = true;
            }

            bool bBothKeysPressed = false;
            if (bCurrentStateForwards && bCurrentStateBackwards)
            {
                bBothKeysPressed = true;
                if (!history.lastForwards && history.lastBackwards)
                {
                    history.moveForwards = true;
                }
                else if (history.lastForwards && !history.lastBackwards)
                {
                    history.moveForwards = false;
                }

                bool bForwardsState = history.moveForwards;
                bool bBackwardsState = !history.moveForwards;
                cs.LeftStickY = (short)((bForwardsState) ? bForwardsState * -128 : bBackwardsState * 128);
            }

            return bBothKeysPressed;
        }

        inline bool ControlLeftAndRight(std::uint64_t heldActions, CControllerState& cs, DirectionHistory& history) noexcept
        {
            bool bCurrentStateLeft = IsHeld(heldActions, LEFT);
            bool bCurrentStateRight = IsHeld(heldActions, RIGHT);
            if (bCurrentStateLeft && !bCurrentStateRight)
            {
                history.lastLeft = true;
                history.lastRight = false;
            }
            else if (!bCurrentStateLeft && bCurrentStateRight)
            {
                history.lastLeft = false;
                history.lastRight = true;
            }

            bool bBothKeysPressed = false;
            if (bCurrentStateLeft && bCurrentStateRight)
            {
                bBothKeysPressed = true;
                if (!history.lastLeft && history.lastRight)
                {
                    history.moveLeft = true;
                }
                else if (history.lastLeft && !history.lastRight)
                {
                    history.moveLeft = false;
                }

                bool bLeftState = history.moveLeft;
                bool bRightState = !history.moveLeft;
                cs.LeftStickX = (short)((bLeftState) ? bLeftState * -128 : bRightState * 128);
            }

            return bBothKeysPressed;
        }
    }

    // Keep the established MTA control policy in one place. This synthesizes
    // logical values, without selecting physical keys or claiming an engine pad
    // write. Dead/vehicle frames intentionally retain on-foot direction history.
    inline CControllerState BuildControllerState(std::uint64_t heldActions, const Context& context, DirectionHistory& history) noexcept
    {
        CControllerState cs;
        if (!context.isDead)
        {
            if (!context.inVehicle)
            {
                cs.ButtonCircle = (IsHeld(heldActions, FIRE) && !context.hasDetonator) ? 255 : 0;  // Fire
                if (context.aimingWeapon)
                {
                    cs.RightShoulder2 = IsHeld(heldActions, ZOOM_OUT) ? 255 : 0;  // Zoom Out
                    cs.LeftShoulder2 = IsHeld(heldActions, ZOOM_IN) ? 255 : 0;    // Zoom In
                }
                else
                {
                    bool bNextWeapon = IsHeld(heldActions, NEXT_WEAPON);
                    bool bPreviousWeapon = IsHeld(heldActions, PREVIOUS_WEAPON);

                    if (bNextWeapon && bPreviousWeapon)
                        bNextWeapon = bPreviousWeapon = false;

                    cs.RightShoulder2 = bNextWeapon ? 255 : 0;     // Next Weapon
                    cs.LeftShoulder2 = bPreviousWeapon ? 255 : 0;  // Previous Weapon
                }

                if (!detail::ControlForwardsBackWards(heldActions, cs, history))
                {
                    cs.LeftStickY = (!IsHeld(heldActions, FORWARDS) && !IsHeld(heldActions, BACKWARDS)) ? 0 : (IsHeld(heldActions, FORWARDS)) ? -128 : 128;
                }

                if (!detail::ControlLeftAndRight(heldActions, cs, history))
                {
                    cs.LeftStickX = (!IsHeld(heldActions, LEFT) && !IsHeld(heldActions, RIGHT)) ? 0 : (IsHeld(heldActions, LEFT)) ? -128 : 128;
                }

                // * Enter Exit
                // * Change View
                cs.ButtonSquare = (!context.enteringVehicle && !context.aimingWeapon && IsHeld(heldActions, JUMP)) ? 255 : 0;  // Jump
                cs.ButtonCross = (IsHeld(heldActions, SPRINT)) ? 255 : 0;                                                      // Sprint
                cs.ShockButtonR = (IsHeld(heldActions, LOOK_BEHIND)) ? 255 : 0;                                                // Look Behind
                cs.ShockButtonL = (IsHeld(heldActions, CROUCH)) ? 255 : 0;                                                     // Crouch
                cs.LeftShoulder1 = (IsHeld(heldActions, ACTION)) ? 255 : 0;                                                    // Action
                cs.m_bPedWalk = (IsHeld(heldActions, WALK)) ? 255 : 0;                                                         // Walk
                // * Vehicle Keys
                cs.RightShoulder1 = (IsHeld(heldActions, AIM_WEAPON)) ? 255 : 0;      // Aim Weapon
                cs.DPadRight = (IsHeld(heldActions, CONVERSATION_YES)) ? 255 : 0;     // Conversation Yes
                cs.DPadLeft = (IsHeld(heldActions, CONVERSATION_NO)) ? 255 : 0;       // Conversation No
                cs.DPadUp = (IsHeld(heldActions, GROUP_CONTROL_FORWARDS)) ? 255 : 0;  // Group Control Forwards
                cs.DPadDown = (IsHeld(heldActions, GROUP_CONTROL_BACK)) ? 255 : 0;    // Group Control Backwards
            }
            else
            {
                cs.ButtonCircle = (IsHeld(heldActions, VEHICLE_FIRE)) ? 255 : 0;             // Fire
                cs.LeftShoulder1 = (IsHeld(heldActions, VEHICLE_SECONDARY_FIRE)) ? 255 : 0;  // Secondary Fire
                cs.LeftStickX = ((IsHeld(heldActions, VEHICLE_LEFT) && IsHeld(heldActions, VEHICLE_RIGHT)) ||
                                 (!IsHeld(heldActions, VEHICLE_LEFT) && !IsHeld(heldActions, VEHICLE_RIGHT)))
                                    ? 0
                                : (IsHeld(heldActions, VEHICLE_LEFT)) ? -128
                                                                      : 128;
                cs.LeftStickY = ((IsHeld(heldActions, STEER_FORWARDS_DOWN) && IsHeld(heldActions, STEER_BACK_UP)) ||
                                 (!IsHeld(heldActions, STEER_FORWARDS_DOWN) && !IsHeld(heldActions, STEER_BACK_UP)))
                                    ? 0
                                : (IsHeld(heldActions, STEER_FORWARDS_DOWN)) ? -128
                                                                             : 128;
                cs.ButtonCross = (IsHeld(heldActions, ACCELERATE)) ? 255 : 0;                                                         // Accelerate
                cs.ButtonSquare = (IsHeld(heldActions, BRAKE_REVERSE)) ? 255 : 0;                                                     // Reverse
                cs.DPadUp = (IsHeld(heldActions, RADIO_NEXT)) ? 255 : 0;                                                              // Radio Next
                cs.DPadDown = (IsHeld(heldActions, RADIO_PREVIOUS)) ? 255 : 0;                                                        // Radio Previous
                cs.m_bRadioTrackSkip = (IsHeld(heldActions, RADIO_USER_TRACK_SKIP)) ? 255 : 0;                                        // Radio Skip
                cs.ShockButtonL = (IsHeld(heldActions, HORN)) ? 255 : 0;                                                              // Horn
                cs.ShockButtonR = (IsHeld(heldActions, SUB_MISSION)) ? 255 : 0;                                                       // Sub Mission
                cs.RightShoulder1 = (IsHeld(heldActions, HANDBRAKE)) ? 255 : 0;                                                       // Handbrake
                cs.LeftShoulder2 = (IsHeld(heldActions, VEHICLE_LOOK_LEFT) || IsHeld(heldActions, VEHICLE_LOOK_BEHIND)) ? 255 : 0;    // Look Left
                cs.RightShoulder2 = (IsHeld(heldActions, VEHICLE_LOOK_RIGHT) || IsHeld(heldActions, VEHICLE_LOOK_BEHIND)) ? 255 : 0;  // Look Right
                // * Look Behind - uses both keys above simultaneously
                // Mouse Look
                cs.RightStickX = ((IsHeld(heldActions, SPECIAL_CONTROL_LEFT) && IsHeld(heldActions, SPECIAL_CONTROL_RIGHT)) ||
                                  (!IsHeld(heldActions, SPECIAL_CONTROL_LEFT) && !IsHeld(heldActions, SPECIAL_CONTROL_RIGHT)))
                                     ? 0
                                 : (IsHeld(heldActions, SPECIAL_CONTROL_LEFT)) ? 128
                                                                               : -128;
                cs.RightStickY = ((IsHeld(heldActions, SPECIAL_CONTROL_DOWN) && IsHeld(heldActions, SPECIAL_CONTROL_UP)) ||
                                  (!IsHeld(heldActions, SPECIAL_CONTROL_DOWN) && !IsHeld(heldActions, SPECIAL_CONTROL_UP)))
                                     ? 0
                                 : (IsHeld(heldActions, SPECIAL_CONTROL_DOWN)) ? 128
                                                                               : -128;
            }
            cs.ButtonTriangle = (IsHeld(heldActions, ENTER_EXIT)) ? 255 : 0;  // Enter Exit
            cs.Select = (IsHeld(heldActions, CHANGE_CAMERA)) ? 255 : 0;       // Change View
        }
        return cs;
    }
}
