/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual task-based water query on one registered native ped
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedWaterTaskAccess
    {
    public:
        virtual ClockResult ReadPedInWater(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedWaterTaskAccess() = default;
    };
}
