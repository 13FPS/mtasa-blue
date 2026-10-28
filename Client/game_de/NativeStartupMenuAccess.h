/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional later-initialization native menu capability
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
namespace mta::game_de
{
    class NativeStartupMenuAccess
    {
    public:
        virtual ~NativeStartupMenuAccess() = default;
        // Shared MTA initialization must admit every backend prerequisite
        // before its first effect. Older owners cannot certify this gate.
        virtual ClockResult RequireStartupNativeMenuReady() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult HideStartupNativeMenu() noexcept { return ClockResult::NotInitialized; }
    };
}
