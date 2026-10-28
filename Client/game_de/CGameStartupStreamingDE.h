/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the retained owner's startup-only MTA streaming receiver
 ****************************************************************************/
#pragma once
#include "CGameNumericGaragesDE.h"
#include <type_traits>

namespace mta::game_de
{
    // This getter preserves MakePedModel's late service lookup after accepted
    // native metadata publication. It does not complete the game factories or
    // grant permission to skip the mandatory overall initialization preflight.
    class CGameStartupStreamingDE : public CGameNumericGaragesDE
    {
    public:
        CStreaming* GetStreaming() override
        {
            CStreaming* selected = nullptr;
            auto        result = NativeWrites().GetStartupStreaming(selected);
            if (result == ClockResult::Ok && !selected)
                result = ClockResult::NotInitialized;
            RequireNativeWrite(result, "get startup streaming");
            return selected;
        }

    protected:
        using CGameNumericGaragesDE::CGameNumericGaragesDE;
    };
    static_assert(std::is_abstract_v<CGameStartupStreamingDE> && std::is_base_of_v<CGame, CGameStartupStreamingDE>);
}
