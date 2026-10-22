/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward raw MTA current rotation without heading conversion/replay
 ****************************************************************************/
#include "CPedRotationDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CPedRotationDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped rotation owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    float CPedRotationDE::GetCurrentRotation() const
    {
        float value;
        Require(rotationAccess.ReadPedCurrentRotation(ReferenceToken(), value), "GetCurrentRotation");
        return value;
    }
    void CPedRotationDE::SetCurrentRotation(float value)
    {
        // CPed stores raw native radians. Degree wrapping and placement belong
        // to a different native script operation, not this direct SDK setter.
        Require(rotationAccess.SetPedCurrentRotation(ReferenceToken(), value), "SetCurrentRotation");
    }
}
