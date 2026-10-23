/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the game owner's existing weapon-fire registrations
 *****************************************************************************/
#pragma once
#include <windows.h>
#include <game/CGame.h>

namespace mta::game_de
{
    // CGame's common SDK exposes registration but no handler reads. Keep this
    // backend bridge storage-free so firing consumes the same registrations
    // already owned by CGameDE rather than creating a second hook registry.
    class WeaponFireHandlerAccess
    {
    public:
        virtual PreWeaponFireHandler*  GetPreWeaponFireHandler() const noexcept = 0;
        virtual PostWeaponFireHandler* GetPostWeaponFireHandler() const noexcept = 0;

    protected:
        virtual ~WeaponFireHandlerAccess() = default;
    };
}
