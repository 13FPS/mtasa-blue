/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep actual MTA task-based query distinct from physical water intent
 ****************************************************************************/
#include "CPedWaterDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    bool CPedWaterDE::IsInWater() const
    {
        bool       value = false;
        const auto result = nativeWaterTaskAccess.ReadPedInWater(ReferenceToken(), value);
        // Unknown task classes and expired owners are refused observations,
        // not proof of false or permission to consult physical water flags.
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped task owner refused IsInWater: " + std::to_string(static_cast<unsigned>(result)));
        return value;
    }
    void CPedWaterDE::SetInWaterFlags(bool inWater)
    {
        const auto result = nativeWaterFlagsAccess.SetPedInWaterFlags(ReferenceToken(), inWater);
        // No cached bool, script water predicate or alternate ped can turn an
        // owner refusal/uncertain native prefix into successful SDK storage.
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped physical water owner refused SetInWaterFlags: " + std::to_string(static_cast<unsigned>(result)));
    }
}
