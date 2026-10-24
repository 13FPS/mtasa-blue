/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the existing native owner for MTA's genuine player rebuild
 *****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>
namespace mta::game_de
{
    class NativePedRebuildAccess
    {
    public:
        // The synchronous owner resolves the current registered player and
        // admits its actual local CJ/model/clothes/render state. The genuine
        // engine operation owns retirement and recreation; this interface
        // grants no remote player-data or complete factory ownership.
        virtual ClockResult RebuildPed(const NativePedReferenceToken&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedRebuildAccess() = default;
    };
}
