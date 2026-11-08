/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep timed-model writes and first defaults with their native owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de
{
    class NativeModelTimeAccess
    {
    public:
        virtual ~NativeModelTimeAccess() = default;
        // A successful false is a freshly qualified non-timed model. Missing
        // callback/write authority must not be converted into that SDK result.
        virtual ClockResult SetModelTime(std::uint32_t, std::uint8_t, std::uint8_t, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ResetAllModelTimes() noexcept { return ClockResult::NotInitialized; }
    };
}
