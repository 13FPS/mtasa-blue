/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw MTA fighting-style intent through a registered ped lifetime
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedFightingStyleAccess
    {
    public:
        virtual ClockResult ReadPedFightingStyle(const NativePedReferenceToken&, std::uint8_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedFightingStyle(const NativePedReferenceToken&, std::uint8_t, std::uint8_t) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedFightingStyleAccess() = default;
    };
}
