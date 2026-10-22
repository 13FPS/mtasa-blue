/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA's void secondary-removal intent and explicit owner failures
 ****************************************************************************/
#include "CTaskManagerDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CTaskManagerDE::RemoveTaskSecondary(const int priority)
    {
        // Classic forwards the index directly to SetTaskSecondary(nullptr,
        // index). Preserve that intent without a local clamp or silent no-op;
        // the qualified native owner reports unsupported/invalid operations.
        const auto result = access.RemoveReferencedPedSecondaryTask(token, priority);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE task-manager owner refused RemoveTaskSecondary: " + std::to_string(static_cast<unsigned>(result)));
    }
}
