/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit world exclusion through the existing registered ped owner
 *****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>

namespace mta::game_de
{
    class NativeWorldIgnoreEntityAccess
    {
    public:
        // A token selects an existing controlled ped; it is not a raw native
        // entity pointer or permission to leave one behind after retirement.
        // The actual owner must acknowledge its single-global cleanup before
        // releasing that relation. Older owners explicitly refuse both paths.
        virtual ClockResult SetWorldIgnoredPed(const NativePedReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ClearWorldIgnoredEntity() noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWorldIgnoreEntityAccess() = default;
    };
}
