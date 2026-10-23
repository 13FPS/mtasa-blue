/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit single-weapon clearing through the existing registered ped
 *****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>
#include <windows.h>
#include <game/CWeaponInfo.h>
namespace mta::game_de
{
    class NativePedWeaponClearAccess
    {
    public:
        // Clearing has native selection, attachment and model-reference effects.
        // Its current registered owner must qualify the whole genuine entry;
        // the SDK neither edits weapon storage nor treats refusal as completion.
        virtual ClockResult ClearPedWeapon(const NativePedReferenceToken&, eWeaponType) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedWeaponClearAccess() = default;
    };
}
