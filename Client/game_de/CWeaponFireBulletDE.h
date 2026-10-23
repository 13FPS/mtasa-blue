/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's bullet-fire itinerary on the actual DE weapon SDK chain
 *****************************************************************************/
#pragma once
#include "CWeaponNativeDE.h"
#include "WeaponFireHandlerAccess.h"

namespace mta::game_de
{
    class CWeaponFireBulletDE : public CWeaponNativeDE
    {
    public:
        bool FireBullet(CEntity* firingEntity, const CVector& origin, const CVector& target) override;

    protected:
        // The enclosing complete backend supplies its actual CGame and that
        // same game's handler view. Borrowing them preserves existing wrapper
        // construction and does not register hooks or install native firing.
        CWeaponFireBulletDE(CGame& game, WeaponFireHandlerAccess& handlers, NativeGameTimingAccess& access, const NativePedReferenceToken& token,
                            CPed* borrowedOwner, eWeaponSlot slot) noexcept
            : CWeaponNativeDE(access, token, borrowedOwner, slot), weaponGame(game), fireHandlers(handlers)
        {
        }

    private:
        CGame&                   weaponGame;
        WeaponFireHandlerAccess& fireHandlers;
    };
    // FireInstantHit, world entity exclusion, transformed bones and audio are
    // still genuine backend dependencies. This itinerary publishes neither a
    // complete CWeapon factory nor a substitute native trace or sound effect.
    static_assert(std::is_abstract_v<CWeaponFireBulletDE> && std::is_base_of_v<CWeaponNativeDE, CWeaponFireBulletDE>);
}
