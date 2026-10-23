/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve classic MTA firing callbacks, muzzle and native-call order
 *****************************************************************************/
#include "CWeaponFireBulletDE.h"
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <CVector2D.h>
// These shared SDK headers retain an unscoped enum forward declaration
// and anonymous surface fields. Localize their existing MSVC warning contract
// without weakening diagnostics for this firing implementation.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471 4201)
#endif
#include <game/CPlayerPed.h>
#include <game/CWorld.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <game/CWeaponStatManager.h>
#include <game/CWeaponFireBulletPolicy.h>

namespace mta::game_de
{
    bool CWeaponFireBulletDE::FireBullet(CEntity* firingEntity, const CVector& origin, const CVector& target)
    {
        // Reuse the exact common itinerary. Its fresh second weapon type,
        // service and post-handler reads preserve changes made by the pre
        // handler; accepted native effects are not retried or rolled back.
        return MtaGame::CWeaponFireBulletPolicy::Fire(
            *this, firingEntity, origin, target, [this](CEntity* entity) { weaponGame.GetWorld()->IgnoreEntity(entity); },
            [](CEntity* entity) { return dynamic_cast<CPlayerPed*>(entity); },
            [this](CPlayerPed* player)
            {
                auto* handler = fireHandlers.GetPreWeaponFireHandler();
                if (handler && player)
                    handler(player, false);
            },
            [this] { return weaponGame.GetWeaponStatManager(); },
            [](CPlayerPed* player, CVector* muzzle) { player->GetTransformedBonePosition(BONE_RIGHTWRIST, muzzle); },
            [](CPlayerPed* player) { player->AddWeaponAudioEvent(EPedWeaponAudioEventType::FIRE); },
            [this](CPlayerPed* player)
            {
                auto* handler = fireHandlers.GetPostWeaponFireHandler();
                if (handler && player)
                    handler();
            });
    }
}
