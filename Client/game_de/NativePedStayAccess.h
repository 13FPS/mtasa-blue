/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine named vehicle-stay flag intent through registered ped identity
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedStayAccess
    {
    public:
        virtual ClockResult ReadPedStayInSamePlace(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedStayInSamePlace(const NativePedReferenceToken&, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedStayAccess() = default;
    };
}
