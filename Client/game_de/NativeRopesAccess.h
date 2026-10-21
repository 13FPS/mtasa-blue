/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA rope intents borrowed from the existing native owner
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
class CVector;

namespace mta::game_de
{
    class NativeRopesAccess
    {
    public:
        virtual ClockResult CreateSwatRope(const CVector&, std::uint32_t, std::int32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RemoveEntityRope(std::uintptr_t) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeRopesAccess() = default;
    };
}
