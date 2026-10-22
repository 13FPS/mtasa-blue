/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Named bleeding boolean intent through fresh registered ped identity
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedBleedingAccess
    {
    public:
        virtual ClockResult ReadPedBleeding(const NativePedReferenceToken&, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedBleeding(const NativePedReferenceToken&, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedBleedingAccess() = default;
    };
}
