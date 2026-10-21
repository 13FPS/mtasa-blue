/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse real MTA stat calls with qualified DE mutation ownership
 *****************************************************************************/
#include "CStatsDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CStatsDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE CStats ") + operation + " unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) +
                                     ")");
    }
    void CStatsDE::SetStatValue(unsigned short index, float value)
    {
        Require(native.SetStatValue(index, value), "SetStatValue");
    }
    void CStatsDE::ModifyStat(unsigned short index, float value)
    {
        // Native dispatch preserves reactions and special cases; treating this
        // SDK delta as a plain read/add/store would bypass those transactions.
        Require(native.ModifyStat(index, value), "ModifyStat");
    }
}
