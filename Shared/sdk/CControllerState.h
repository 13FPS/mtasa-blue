/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/CControllerState.h
 *  PURPOSE:     Shared MTA controller-state contract
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <cstring>

// Existing keysync and another game backend must share the same MTA control
// contract. Keep the class independent of the classic pad's Windows interface
// so reuse preserves its field layout and zero initialization without a copy.
// Set values to 128 unless otherwise specified
class CControllerState
{
public:
    signed short LeftStickX;   // move/steer left (-128?)/right (+128)
    signed short LeftStickY;   // move back(+128)/forwards(-128?)
    signed short RightStickX;  // numpad 6(+128)/numpad 4(-128?)
    signed short RightStickY;

    signed short LeftShoulder1;
    signed short LeftShoulder2;
    signed short RightShoulder1;  // target / hand brake
    signed short RightShoulder2;

    signed short DPadUp;    // radio change up
    signed short DPadDown;  // radio change down
    signed short DPadLeft;
    signed short DPadRight;

    signed short Start;
    signed short Select;

    signed short ButtonSquare;    // jump / reverse
    signed short ButtonTriangle;  // get in/out
    signed short ButtonCross;     // sprint / accelerate
    signed short ButtonCircle;    // fire

    signed short ShockButtonL;
    signed short ShockButtonR;  // look behind

    signed short m_bChatIndicated;
    signed short m_bPedWalk;
    signed short m_bVehicleMouseLook;
    signed short m_bRadioTrackSkip;

    CControllerState() { memset(this, 0, sizeof(CControllerState)); }
};
