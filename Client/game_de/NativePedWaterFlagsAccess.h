/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual physical water-flag intent on one registered native ped
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedWaterFlagsAccess
    {
    public:
        // This writes storage only. SDK IsInWater is a distinct primary-task
        // predicate and must never be fabricated from these physical flags.
        virtual ClockResult SetPedInWaterFlags(const NativePedReferenceToken&, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedWaterFlagsAccess() = default;
    };
}
