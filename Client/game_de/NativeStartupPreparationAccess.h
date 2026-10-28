/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional fresh prerequisites for actual later startup operations
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>

namespace mta::game_de
{
    class NativeStartupPreparationAccess
    {
    public:
        virtual ~NativeStartupPreparationAccess() = default;
        // These qualify current operation prerequisites only. They create no
        // metadata, request, retained journal or lease; every effect still
        // performs its own original current-owner validation. Older owners
        // must refuse instead of claiming these genuine native capabilities.
        // Qualify the complete later Initialize itinerary on one current
        // owner generation, without running any effect or granting a lease
        // for its duration. Later operations still qualify themselves.
        virtual ClockResult RequireStartupInitializationReady() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RequireStartupPedModelCreationReady() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RequireStartupSpecialModelRequestReady() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RequireStartupCollisionCopyReady() noexcept { return ClockResult::NotInitialized; }
    };
}
