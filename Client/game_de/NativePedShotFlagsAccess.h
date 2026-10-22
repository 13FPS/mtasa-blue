/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine named vehicle-shot flag intent through registered ped identity
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedShotFlagsAccess
    {
    public:
        virtual ClockResult ReadPedCanBeShotInVehicle(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedCanBeShotInVehicle(const NativePedReferenceToken&, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedShotFlagsAccess() = default;
    };
}
