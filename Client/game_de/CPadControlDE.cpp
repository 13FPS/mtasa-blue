/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route CPad control operations through their qualified native owner
 *****************************************************************************/
#include "CPadControlDE.h"
#include "NativePadUpdateAccess.h"

#include <stdexcept>

namespace mta::game_de
{
    bool CPadControlDE::IsEnabled()
    {
        bool enabled = false;
        if (!updates.IsEnabled(enabled))
            throw std::runtime_error("DE CPad updater state unavailable");
        return enabled;
    }

    void CPadControlDE::Disable(bool disabled)
    {
        // Suppressing a native producer is distinct from clearing input or
        // setting a player's control-disable flags. Only its owner can accept
        // this global operation and carry the obligation to restore it.
        if (!updates.SetDisabled(disabled))
            throw std::runtime_error("DE CPad updater control unavailable");
    }
}
