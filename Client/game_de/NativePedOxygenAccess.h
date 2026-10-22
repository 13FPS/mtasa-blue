/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw breath storage through a fresh registered ped and its own player data
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedOxygenAccess
    {
    public:
        virtual ClockResult ReadPedOxygenLevel(const NativePedReferenceToken&, float&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedOxygenLevel(const NativePedReferenceToken&, float) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedOxygenAccess() = default;
    };
}
