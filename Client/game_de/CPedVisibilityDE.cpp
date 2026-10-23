/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep actual SDK visibility reads and writes on their native owner
 *****************************************************************************/
#include "CPedVisibilityDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    bool CPedVisibilityDE::IsVisible()
    {
        bool       visible = false;
        const auto result = nativeVisibilityAccess.ReadPedVisible(ReferenceToken(), visible);
        // A refused read is not successful invisibility. The current owner
        // qualifies the same immutable registered ped, without a renderer or
        // current-player fallback changing this raw property contract.
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped visibility owner refused IsVisible: " + std::to_string(static_cast<unsigned>(result)));
        return visible;
    }

    void CPedVisibilityDE::SetVisible(bool visible)
    {
        const auto result = nativeVisibilityAccess.SetPedVisible(ReferenceToken(), visible);
        // An entered native effect belongs to that owner's failure protocol.
        // SDK forwarding must never retry, restore or infer script effects.
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped visibility owner refused SetVisible: " + std::to_string(static_cast<unsigned>(result)));
    }
}
