/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Const ped fire association predicate through its registered owner
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>
namespace mta::game_de
{
    class NativePedFireAccess
    {
    public:
        virtual ~NativePedFireAccess() = default;
        virtual ClockResult ReadPedOnFire(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }
    };
}
