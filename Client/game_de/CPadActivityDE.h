/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's native pad activity timestamp contract
 *****************************************************************************/
#pragma once
#include "CPadControlDE.h"

namespace mta::game_de
{
    class NativePadActivityAccess;

    // A timestamp write borrows its own qualified native field access. It
    // cannot grant whole-pad replay, horn history or native shake support.
    class CPadActivityDE : public CPadControlDE
    {
    public:
        void SetLastTimeTouched(DWORD time) override;

    protected:
        CPadActivityDE(NativePadStateAccess& stateAccess, NativePadUpdateAccess& updateAccess, NativePadActivityAccess& activityAccess) noexcept
            : CPadControlDE(stateAccess, updateAccess), activity(activityAccess)
        {
        }

    private:
        NativePadActivityAccess& activity;
    };

    static_assert(std::is_abstract_v<CPadActivityDE> && std::is_base_of_v<CPadControlDE, CPadActivityDE>);
}
