/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw two-bit bike-knockoff intent through fresh registered ped identity
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedBikeFlagsAccess
    {
    public:
        virtual ClockResult ReadPedCantBeKnockedOffBike(const NativePedReferenceToken&, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedCantBeKnockedOffBike(const NativePedReferenceToken&, std::uint32_t) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedBikeFlagsAccess() = default;
    };
}
