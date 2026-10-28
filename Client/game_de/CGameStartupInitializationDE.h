/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit the actual loaded-world Initialize path through its native owner
 ****************************************************************************/
#pragma once
#include "CGameStartupStreamingDE.h"
#include <type_traits>

namespace mta::game_de
{
    // This completes the later Initialize preflight, not CGame construction or
    // the missing full SDK factories. The inherited MTA itinerary still owns
    // its order, accepted prefix, and late per-model streaming lookups.
    class CGameStartupInitializationDE : public CGameStartupStreamingDE
    {
    protected:
        using CGameStartupStreamingDE::CGameStartupStreamingDE;
        void RequireInitializationReady() override { RequireNativeWrite(NativeWrites().RequireStartupInitializationReady(), "startup initialization"); }
    };
    static_assert(std::is_abstract_v<CGameStartupInitializationDE> && std::is_base_of_v<CGame, CGameStartupInitializationDE>);
}
