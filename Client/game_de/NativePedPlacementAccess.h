/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit placement and teleport on the original registered ped
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>
namespace mta::game_de
{
    class NativePedPlacementAccess
    {
    public:
        virtual ClockResult SetReferencedPedPosition(const NativePedReferenceToken&, const CVector&) noexcept { return ClockResult::NotInitialized; }
        // Complete only in the same callback after acknowledged SDK placement
        // and last-good commit. Once this exact owner/token is admitted, even
        // a pre-native refusal must stop that already-started intent. This is
        // not a standalone replacement for the existing raw TeleportPed API.
        virtual ClockResult CompleteReferencedPedTeleport(const NativePedReferenceToken&, const CVector&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedPlacementAccess() = default;
    };
}
