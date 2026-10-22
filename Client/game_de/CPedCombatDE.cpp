/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA fighting-style storage and its current native owner coherent
 ****************************************************************************/
#include "CPedCombatDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedCombatDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped combat owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    eFightingStyle CPedCombatDE::GetFightingStyle() const
    {
        std::uint8_t value;
        Require(combatAccess.ReadPedFightingStyle(ReferenceToken(), value), "GetFightingStyle");
        // The actual SDK enum has byte storage. Preserve raw native values
        // instead of turning an unrecognized style into a fabricated default.
        return static_cast<eFightingStyle>(value);
    }
    void CPedCombatDE::SetFightingStyle(eFightingStyle style, std::uint8_t styleExtra)
    {
        // Only the qualified native owner can compare the current style and
        // apply MTA's same-style early return and extra-bit OR policy. Forward
        // both raw bytes once; accepted partial effects cannot be replayed here.
        Require(combatAccess.SetPedFightingStyle(ReferenceToken(), static_cast<std::uint8_t>(style), styleExtra), "SetFightingStyle");
    }
}
