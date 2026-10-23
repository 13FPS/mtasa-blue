/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the existing native owner for a camera sphere observation
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>

namespace mta::game_de
{
    class NativeCameraSphereVisibleAccess
    {
    public:
        // Camera state and frustum interpretation remain with the admitted
        // native operation. An older owner cannot synthesize a visible or
        // invisible result from unsupported camera or ped state.
        virtual ClockResult ReadCameraSphereVisible(const CVector&, float, bool&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeCameraSphereVisibleAccess() = default;
    };
}
