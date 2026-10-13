/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/CPad.h
 *  PURPOSE:     Controller pad interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <CControllerState.h>

class CPad
{
public:
    virtual CControllerState* GetCurrentControllerState(CControllerState* ControllerState) = 0;
    virtual CControllerState* GetLastControllerState(CControllerState* ControllerState) = 0;
    virtual void              SetCurrentControllerState(CControllerState* ControllerState) = 0;
    virtual void              SetLastControllerState(CControllerState* ControllerState) = 0;
    virtual void              Restore() = 0;
    virtual void              Store() = 0;
    virtual bool              IsEnabled() = 0;
    virtual void              Disable(bool bDisable) = 0;
    virtual void              Clear() = 0;
    virtual void              SetHornHistoryValue(bool value) = 0;
    virtual void              SetLastTimeTouched(DWORD dwTime) = 0;

    // PC never consumes these itself (CPad::ProcessPCSpecificStuff is a stub), so joystick
    // code reads/ticks them to drive XInput rumble.
    virtual short         GetShakeDur() = 0;
    virtual unsigned char GetShakeFreq() = 0;
    virtual void          SetShakeDur(short sShakeDur) = 0;
};
