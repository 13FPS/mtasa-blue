/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep current camera observations and Restore on the existing owner
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <windows.h>
#include <cstdint>
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

        // Current scalar reads stay with this same owner. A missing provider
        // is not an absent camera, a clamped selector or the SDK's FOV default.
        virtual ClockResult ReadCameraActiveIndex(std::uint8_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadCameraFov(std::uint32_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        // Invalid SDK floats are filtered before this channel. A real owner
        // must acknowledge one bounded store or retain its uncertainty; an
        // older provider cannot turn absence into a successful FOV setter.
        virtual ClockResult WriteCameraFov(std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        // Ordinary Restore is one distinct native effect. Older owners must
        // refuse rather than substitute JumpCut, a field repair or a retry.
        virtual ClockResult RestoreCamera() noexcept { return ClockResult::NotInitialized; }
        // This is MTA's two-step JumpCut itinerary, not ordinary Restore.
        // Missing implementations may not synthesize its accepted prefix.
        virtual ClockResult RestoreCameraWithJumpCut() noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeCameraSphereVisibleAccess() = default;
    };
}
