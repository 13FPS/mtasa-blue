/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA line-clear semantics through its borrowed owner
 *****************************************************************************/
#include "CWorldLineDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    bool CWorldLineDE::IsLineOfSightClear(const CVector* start, const CVector* end, const SLineOfSightFlags flags)
    {
        if (!start || !end)
            throw std::invalid_argument("DE world IsLineOfSightClear requires both positions");
        // The owner's synchronous callbacks cannot borrow or mutate the caller's
        // vectors/flags. Preserve all common SDK values without coordinate,
        // epsilon or category rewrites; native admission maps its genuine flags.
        const CVector           startIntent = *start;
        const CVector           endIntent = *end;
        const SLineOfSightFlags flagIntent = flags;
        bool                    clear = false;
        const auto              result = lineOwner.IsWorldLineOfSightClear(startIntent, endIntent, flagIntent, clear);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE world owner refused IsLineOfSightClear: " + std::to_string(static_cast<unsigned>(result)));
        // Both native clear and blocked are successful observations. A refused
        // owner is neither boolean, and this SDK layer never retries a query.
        return clear;
    }
}
