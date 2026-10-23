/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the current native owner for MTA's line-clear boolean query
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
class CVector;
struct SLineOfSightFlags;
namespace mta::game_de
{
    class NativeWorldLineAccess
    {
    public:
        // The genuine query advances native scans and may lazily initialize
        // matrices. An older owner must refuse rather than invent clear/blocked.
        virtual ClockResult IsWorldLineOfSightClear(const CVector&, const CVector&, const SLineOfSightFlags&, bool&) noexcept
        {
            return ClockResult::NotInitialized;
        }

    protected:
        ~NativeWorldLineAccess() = default;
    };
}
