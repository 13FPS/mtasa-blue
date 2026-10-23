/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe actual goggles association through retained ped identity
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedGogglesAccess
    {
    public:
        virtual ClockResult ReadPedWearingGoggles(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedGogglesAccess() = default;
    };
}
