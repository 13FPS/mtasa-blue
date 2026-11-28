/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe transition activity and FOV in the same camera update
 ****************************************************************************/
#pragma once

// Optional camera interface. Older CCamera providers keep their existing vtable.
class CCameraTransitionFOV
{
public:
    // False means the transition is inactive and leaves fov untouched.
    virtual bool TryGetActiveTransitionFOV(float& fov) const = 0;

protected:
    ~CCameraTransitionFOV() = default;
};
