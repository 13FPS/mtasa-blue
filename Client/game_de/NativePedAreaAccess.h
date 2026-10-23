/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw SDK area BYTE through the existing registered ped owner
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de
{
    class NativePedAreaAccess
    {
    public:
        virtual ~NativePedAreaAccess() = default;
        virtual ClockResult ReadPedAreaCode(const NativePedReferenceToken&, std::uint8_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedAreaCode(const NativePedReferenceToken&, std::uint8_t) noexcept { return ClockResult::NotInitialized; }
    };
}
