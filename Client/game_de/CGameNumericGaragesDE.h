/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the actual retained owner's qualified startup garage service
 ****************************************************************************/
#pragma once
#include "CGameNativeMenuDE.h"
#include <type_traits>

namespace mta::game_de
{
    // Supplying this real sixteen-ID receiver does not satisfy the remaining
    // full-game factories or the mandatory overall initialization preflight.
    class CGameNumericGaragesDE : public CGameNativeMenuDE
    {
    public:
        CGarages* GetGarages() override
        {
            RequirePreparedServiceBinding("get startup numeric garages");
            CGarages* selected = nullptr;
            auto      result = NativeWrites().GetStartupNumericGarages(selected);
            if (result == ClockResult::Ok && !selected)
                result = ClockResult::NotInitialized;
            RequireNativeWrite(result, "get startup numeric garages");
            // The provider may perform current native qualification before
            // returning. Publish its pointer only while this same prepared
            // game's constructor binding still passes the existing owner.
            RequirePreparedServiceBinding("get startup numeric garages");
            return selected;
        }

    protected:
        using CGameNativeMenuDE::CGameNativeMenuDE;
    };
    static_assert(std::is_abstract_v<CGameNumericGaragesDE> && std::is_base_of_v<CGame, CGameNumericGaragesDE>);
}
