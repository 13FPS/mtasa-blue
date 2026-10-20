/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's independent current/previous controller SDK contract
 *****************************************************************************/
#include "CPadStateDE.h"
#include "NativePadStateAccess.h"

#include <stdexcept>

namespace mta::game_de
{
    CControllerState* CPadStateDE::GetCurrentControllerState(CControllerState* output)
    {
        if (!output)
            throw std::invalid_argument("DE CPad current state output is null");
        CControllerState value;
        if (!states.ReadCurrent(value))
            throw std::runtime_error("DE CPad current state unavailable");
        // A refused or partial native read must preserve all caller fields.
        *output = value;
        return output;
    }

    CControllerState* CPadStateDE::GetLastControllerState(CControllerState* output)
    {
        if (!output)
            throw std::invalid_argument("DE CPad previous state output is null");
        CControllerState value;
        if (!states.ReadPrevious(value))
            throw std::runtime_error("DE CPad previous state unavailable");
        *output = value;
        return output;
    }

    void CPadStateDE::SetCurrentControllerState(CControllerState* input)
    {
        if (!input)
            throw std::invalid_argument("DE CPad current state input is null");
        // MTA supplies previous input separately around engine processing.
        // Shifting current into previous here would erase its intended edges.
        if (!states.WriteCurrent(*input))
            throw std::runtime_error("DE CPad current state write unavailable");
    }

    void CPadStateDE::SetLastControllerState(CControllerState* input)
    {
        if (!input)
            throw std::invalid_argument("DE CPad previous state input is null");
        if (!states.WritePrevious(*input))
            throw std::runtime_error("DE CPad previous state write unavailable");
    }

    void CPadStateDE::Clear()
    {
        // The SDK clears current/previous input only. Calling DE's broader
        // native clear would also mutate physical queues and enclosing fields.
        if (!states.Clear())
            throw std::runtime_error("DE CPad controller clear unavailable");
    }
}
