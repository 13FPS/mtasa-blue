/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward raw oxygen and named ducking intent without substitutions
 ****************************************************************************/
#include "CPedStateDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    std::uint32_t CPedStateDE::GetType() const noexcept
    {
        return wrapperTypeValue;
    }
    int CPedStateDE::GetCustomMoveAnim() const noexcept
    {
        return m_iCustomMoveAnim;
    }
    void CPedStateDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped state owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    float CPedStateDE::GetOxygenLevel() const
    {
        float value;
        // Missing selected player data must refuse. Returning another player's
        // breath, stamina or a synthetic default would hide missing authority.
        Require(oxygenAccess.ReadPedOxygenLevel(ReferenceToken(), value), "GetOxygenLevel");
        return value;
    }
    void CPedStateDE::SetOxygenLevel(float oxygen)
    {
        // CPedSA copies raw breath bits without a domain clamp. Persistent
        // accepted effects and token revocation remain the native owner's job.
        Require(oxygenAccess.SetPedOxygenLevel(ReferenceToken(), oxygen), "SetOxygenLevel");
    }
    bool CPedStateDE::IsDucking() const
    {
        bool value;
        Require(duckingAccess.ReadPedDucking(ReferenceToken(), value), "IsDucking");
        return value;
    }
}
