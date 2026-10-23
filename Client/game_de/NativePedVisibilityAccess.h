/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the genuine current visibility flag through retained ped identity
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedVisibilityAccess
    {
    public:
        virtual ClockResult ReadPedVisible(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedVisible(const NativePedReferenceToken&, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedVisibilityAccess() = default;
    };
}
