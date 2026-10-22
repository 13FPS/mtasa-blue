/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward actual MTA bool properties without native replay or guesses
 ****************************************************************************/
#include "CPedBehaviorDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedBehaviorDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped behavior owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    bool CPedBehaviorDE::IsBleeding() const
    {
        bool value;
        Require(bleedingAccess.ReadPedBleeding(ReferenceToken(), value), "IsBleeding");
        return value;
    }
    void CPedBehaviorDE::SetBleeding(bool value)
    {
        // Classic writes one raw flag. The native owner preserves neighbors
        // and persistent partial effects; this SDK layer does not replay it
        // or replace the storage intent with a separate blood-effect call.
        Require(bleedingAccess.SetPedBleeding(ReferenceToken(), value), "SetBleeding");
    }
    bool CPedBehaviorDE::GetStayInSamePlace() const
    {
        bool value;
        Require(stayAccess.ReadPedStayInSamePlace(ReferenceToken(), value), "GetStayInSamePlace");
        return value;
    }
    void CPedBehaviorDE::SetStayInSamePlace(bool value)
    {
        Require(stayAccess.SetPedStayInSamePlace(ReferenceToken(), value), "SetStayInSamePlace");
    }
}
