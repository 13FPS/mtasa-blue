/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse Core's configured-control bind-state policy across backends
 *****************************************************************************/

#pragma once

#include <CControllerActions.h>

namespace MtaInput
{
    // Keep alias release, disabled controls and wheel zoom consistent with
    // existing MTA binds. Core retains callback dispatch; this result only
    // decides whether its original callback loop is entered.
    template <class OtherHeld>
    inline bool UpdateControlBindState(eControllerAction action, bool down, bool enabled, bool mouseWheel, bool& bindState, bool& controlState,
                                       OtherHeld&& otherHeld)
    {
        // Original Core suppresses custom track skips before any mutation.
        if (action == RADIO_USER_TRACK_SKIP)
            return false;
        bindState = down;
        // Query lazily on release, after storing this bind's new state, so a
        // different held alias can keep the aggregate control active.
        if (!down && otherHeld())
            return false;
        const bool zoomWheel = mouseWheel && (action == ZOOM_IN || action == ZOOM_OUT);
        if (enabled && !zoomWheel)
            controlState = down;
        return true;
    }
}
