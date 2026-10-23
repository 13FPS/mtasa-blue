/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward actual two-flag MTA intent without a fake task predicate
 ****************************************************************************/
#include "CPedWaterDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedWaterDE::SetInWaterFlags(bool inWater)
    {
        const auto result = nativeWaterFlagsAccess.SetPedInWaterFlags(ReferenceToken(), inWater);
        // No cached bool, script water predicate or alternate ped can turn an
        // owner refusal/uncertain native prefix into successful SDK storage.
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped physical water owner refused SetInWaterFlags: " + std::to_string(static_cast<unsigned>(result)));
    }
}
