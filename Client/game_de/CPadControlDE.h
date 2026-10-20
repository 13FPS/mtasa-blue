/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve CPad's native producer-control semantics
 *****************************************************************************/
#pragma once

#include "CPadStateDE.h"

namespace mta::game_de
{
    class NativePadUpdateAccess;

    // State access does not imply authority to suppress the native producer.
    // Borrow that separately admitted owner, while whole-pad save/restore,
    // horn, time and rumble remain abstract until their own contracts qualify.
    class CPadControlDE : public CPadStateDE
    {
    public:
        bool IsEnabled() override;
        void Disable(bool disabled) override;

    protected:
        CPadControlDE(NativePadStateAccess& stateAccess, NativePadUpdateAccess& updateAccess) noexcept : CPadStateDE(stateAccess), updates(updateAccess) {}

    private:
        NativePadUpdateAccess& updates;
    };

    static_assert(std::is_abstract_v<CPadControlDE> && std::is_base_of_v<CPadStateDE, CPadControlDE>);
}
