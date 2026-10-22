/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One actual MTA task-manager mutation through the existing ped owner
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>

namespace mta::game_de
{
    class NativeTaskRemovalAccess
    {
    public:
        virtual ~NativeTaskRemovalAccess() = default;
        // The CPU token selects a retained ped cell; it grants no callback or
        // task lease. Older read-only owners must refuse this destructive call.
        virtual ClockResult RemoveReferencedPedSecondaryTask(const NativePedReferenceToken&, int) noexcept { return ClockResult::NotInitialized; }
    };
}
