/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the existing owner for MTA's scalar world-ground query
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
class CVector;
namespace mta::game_de
{
    class NativeWorldGroundAccess
    {
    public:
        // A read-looking query enters native scan/matrix state. Older owners
        // must refuse it rather than manufacture a zero-height hit or lease.
        virtual ClockResult FindWorldGroundZ(const CVector&, float&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWorldGroundAccess() = default;
    };
}
