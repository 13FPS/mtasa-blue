/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse completed SDK services in the future complete DE game object
 *****************************************************************************/
#include "CGameDE.h"

namespace mta::game_de
{
    CGameDE::CGameDE(const NativeGameTimingReader& reader, CClockDE& nativeClock, NativeGameTimingAccess& timingWriter) noexcept
        : CGameTimingDE(reader, timingWriter), clock(nativeClock)
    {
    }

    CClock* CGameDE::GetClock() noexcept
    {
        return &clock;
    }

    CKeyGen* CGameDE::GetKeyGen() noexcept
    {
        return &keyGen;
    }
}
