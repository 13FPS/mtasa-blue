/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep current camera observations and Restore on the existing owner
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include "NativeCameraRowBorrow.h"
#include "NativePedReferenceToken.h"
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
        // Configured camera mode is distinct from pending-off and border
        // animation. Older providers cannot synthesize these actual effects.
        virtual ClockResult ReadCameraWidescreen(bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult WriteCameraWidescreen(bool) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadCameraFov(std::uint32_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        // Invalid SDK floats are filtered before this channel. A real owner
        // must acknowledge one bounded store or retain its uncertainty; an
        // older provider cannot turn absence into a successful FOV setter.
        virtual ClockResult WriteCameraFov(std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        // Raw pair reads publish no repair. The actual SDK computes its
        // float policy once, then conditionally repairs this exact original
        // pair before publishing outputs; an older owner must refuse it.
        virtual ClockResult ReadCameraMode(std::uint32_t, std::uint16_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadCameraDirection(std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ConditionalRepairCameraDirection(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult WriteCameraDirection(std::uint32_t, std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        // A current native row is borrowed by a unique retained client. The
        // lease grants retention, not operation authority or copied vectors.
        virtual ClockResult AcquireCameraRowBorrow(std::uint32_t, NativeCameraRowBorrow::Token&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReleaseCameraRowBorrow(NativeCameraRowBorrow::Token&) noexcept { return ClockResult::NotInitialized; }
        // A cached row product still needs a fresh current native admission.
        // Older providers cannot publish a stale pointer or grant CPU drain.
        virtual ClockResult ValidateCameraRowBorrow(const NativeCameraRowBorrow::Token&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult CheckCameraRowBorrowDrain(const NativeCameraRowBorrow::Token&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadCameraRowVector(const NativeCameraRowBorrow::Token&, CameraRowVectorKind, CVector*&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Raw target observation is tied to this same row client. A zero
        // or unknown value grants no entity adoption or reference writes.
        virtual ClockResult ReadCameraRowTarget(const NativeCameraRowBorrow::Token&, std::uintptr_t&) noexcept { return ClockResult::NotInitialized; }
        // DE retargeting owns an acknowledged engine-cell itinerary. A null
        // token pointer explicitly clears; older providers must refuse it.
        virtual ClockResult SetCameraRowTarget(const NativeCameraRowBorrow::Token&, const NativePedReferenceToken*) noexcept
        {
            return ClockResult::NotInitialized;
        }
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
