/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Cover-only cleanup through the current MTA engine owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
namespace mta::game_de
{
    class NativeWorldCoverAccess
    {
    public:
        virtual ~NativeWorldCoverAccess() = default;
        virtual ClockResult CheckWorldCoverCleanupReady() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RemoveAllWorldCovers() noexcept { return ClockResult::NotInitialized; }
    };
}
