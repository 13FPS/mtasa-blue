/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the genuine current ducking flag through retained ped identity
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedDuckingAccess
    {
    public:
        virtual ClockResult ReadPedDucking(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedDuckingAccess() = default;
    };
}
