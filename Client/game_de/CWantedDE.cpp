/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA wanted setter and NoFlash contracts
 *****************************************************************************/
#include "CWantedDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CWantedDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE CWanted ") + operation + " unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) +
                                     ")");
    }
    void CWantedDE::SetWantedLevel(DWORD level)
    {
        static_assert(sizeof(DWORD) == sizeof(std::uint32_t));
        Require(native.SetWantedLevel(level, false), "SetWantedLevel");
    }
    char CWantedDE::GetWantedLevel()
    {
        std::uint32_t level = 0;
        Require(native.ReadWantedLevel(level), "GetWantedLevel");
        // The classic SDK deliberately clamps the actual unsigned level to6.
        // A missing native service must throw instead of looking like level0.
        return static_cast<char>(level > 6 ? 6 : level);
    }
    void CWantedDE::SetWantedLevelNoFlash(DWORD level)
    {
        Require(native.SetWantedLevel(level, true), "SetWantedLevelNoFlash");
    }
}
