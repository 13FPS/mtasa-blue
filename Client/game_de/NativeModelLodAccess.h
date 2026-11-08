/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep real model distance defaults and writes on one native owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de
{
    class NativeModelLodAccess
    {
    public:
        virtual ~NativeModelLodAccess() = default;
        // Raw float bits preserve MTA's actual signed-zero/NaN/override policy.
        // Missing owner authority is not a current zero or absent default.
        virtual ClockResult ReadModelLodDistance(std::uint32_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadOriginalModelLodDistance(std::uint32_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        // The actual SDK applies its limit before this native channel. No
        // native clamp or duplicate float conversion changes the supplied bits.
        virtual ClockResult SetModelLodDistance(std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ResetAllModelLodDistances() noexcept { return ClockResult::NotInitialized; }
    };
}
