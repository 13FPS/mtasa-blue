/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw associated-goggles presence without creating effects
 ****************************************************************************/
#include "CPedGogglesDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    bool CPedGogglesDE::IsWearingGoggles() const
    {
        bool       present = false;
        const auto result = nativeGogglesAccess.ReadPedWearingGoggles(ReferenceToken(), present);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped goggles owner refused IsWearingGoggles: " + std::to_string(static_cast<unsigned>(result)));
        // Classic observes only its goggles-object association. Vision/FX
        // activity, object validity or a current-player fallback change that
        // meaning and cannot turn an owner refusal into a successful false.
        return present;
    }
}
