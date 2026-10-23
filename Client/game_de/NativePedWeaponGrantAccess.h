/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit a genuine weapon grant through the registered ped owner
 *****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>
#include <windows.h>
#include <game/CWeaponInfo.h>
namespace mta::game_de
{
    class NativePedWeaponGrantAccess
    {
    public:
        // Native grant requires its own fresh preloaded-model/current-owner
        // admission after ordinary MTA preparation. This interface supplies
        // neither that preparation nor a complete native CWeapon wrapper bank.
        virtual ClockResult GrantPedWeapon(const NativePedReferenceToken&, eWeaponType, std::uint32_t, std::uint32_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }

    protected:
        ~NativePedWeaponGrantAccess() = default;
    };
}
