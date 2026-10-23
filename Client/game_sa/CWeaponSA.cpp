/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CWeaponSA.cpp
 *  PURPOSE:     Weapon class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CGameSA.h"
#include "CPlayerPedSA.h"
#include "CWeaponSA.h"
#include "CWeaponStatManagerSA.h"
#include "CWeaponStatSA.h"
#include "CWorldSA.h"
#include <game/CWeaponFireBulletPolicy.h>

extern CGameSA* pGame;

CWeaponInfo* CWeaponSA::GetInfo(eWeaponSkill skill) const
{
    return m_interface ? pGame->GetWeaponInfo(m_interface->m_eWeaponType, skill) : nullptr;
}

void CWeaponSA::SetAsCurrentWeapon()
{
    if (m_owner)
        m_owner->SetCurrentWeaponSlot(m_weaponSlot);
}

void CWeaponSA::Destroy()
{
    if (m_interface)
        delete m_interface;

    delete this;
}

void CWeaponSA::Remove()
{
    if (!m_interface)
        return;

    m_interface->Shutdown();

    // If the removed weapon was the currently active weapon, switch to empty-handed
    if (m_owner && m_owner->GetCurrentWeaponSlot() == m_weaponSlot)
    {
        if (CWeaponInfo* info = pGame->GetWeaponInfo(m_interface->m_eWeaponType))
            m_owner->RemoveWeaponModel(info->GetModel());

        m_owner->SetCurrentWeaponSlot(WEAPONSLOT_TYPE_UNARMED);
    }
}

void CWeaponSA::Initialize(eWeaponType type, std::uint32_t ammo, CPed* ped)
{
    if (m_interface)
        m_interface->Initialize(type, ammo, ped ? ped->GetPedInterface() : nullptr);
}

void CWeaponSA::AddGunshell(CEntity* firingEntity, const CVector& vecOrigin, const CVector2D& vecDirection, float size) const
{
    if (m_interface && firingEntity)
        m_interface->AddGunshell(firingEntity->GetInterface(), vecOrigin, vecDirection, size);
}

void CWeaponSA::DoBulletImpact(CEntity* firingEntity, CEntitySAInterface* hitEntityInterface, const CVector& vecOrigin, const CVector& vecTarget,
                               const CColPointSAInterface& colPoint, int incrementalHit) const
{
    if (m_interface)
        m_interface->DoBulletImpact(firingEntity ? firingEntity->GetInterface() : nullptr, hitEntityInterface, vecOrigin, vecTarget, colPoint, incrementalHit);
}

bool CWeaponSA::Fire(CEntity* firingEntity, CVector* vecOrigin, CVector* vecEffectPos, CEntity* targetEntity, CVector* vecTarget, CVector* vecAlt)
{
    if (!firingEntity || !m_interface)
        return false;

    return m_interface->Fire(firingEntity->GetInterface(), vecOrigin, vecEffectPos, targetEntity ? targetEntity->GetInterface() : nullptr, vecTarget, vecAlt);
}

bool CWeaponSA::FireInstantHit(CEntity* firingEntity, CVector* vecOrigin, CVector* vecMuzzle, CEntity* targetEntity, CVector* vecTarget, CVector* vecForDriveby,
                               bool crossHairGun, bool createGunFx)
{
    if (!m_interface)
        return false;

    return m_interface->FireInstantHit(firingEntity ? firingEntity->GetInterface() : nullptr, vecOrigin, vecMuzzle,
                                       targetEntity ? targetEntity->GetInterface() : nullptr, vecTarget, vecForDriveby, crossHairGun, createGunFx);
}

bool CWeaponSA::FireBullet(CEntity* firingEntity, const CVector& vecOrigin, const CVector& vecTarget)
{
    // Reuse MTA's existing sequence while leaving genuine world/stat/player
    // services and native FireInstantHit dispatch with the classic backend.
    // Keep handler lookups inside their original gates, after any callbacks.
    return MtaGame::CWeaponFireBulletPolicy::Fire(
        *this, firingEntity, vecOrigin, vecTarget, [](CEntity* entity) { pGame->GetWorld()->IgnoreEntity(entity); },
        [](CEntity* entity) { return dynamic_cast<CPlayerPed*>(entity); },
        [](CPlayerPed* player)
        {
            if (pGame->m_pPreWeaponFireHandler && player)
                pGame->m_pPreWeaponFireHandler(player, false);
        },
        [] { return pGame->GetWeaponStatManager(); }, [](CPlayerPed* player, CVector* muzzle) { player->GetTransformedBonePosition(BONE_RIGHTWRIST, muzzle); },
        [](CPlayerPed* player) { player->AddWeaponAudioEvent(EPedWeaponAudioEventType::FIRE); },
        [](CPlayerPed* player)
        {
            if (pGame->m_pPostWeaponFireHandler && player)
                pGame->m_pPostWeaponFireHandler();
        });
}

bool CWeaponSA::GenerateDamageEvent(CPed* ped, CEntity* responsible, eWeaponType weaponType, int damagePerHit, ePedPieceTypes hitZone, std::uint8_t dir) const
{
    if (!ped || !m_interface)
        return false;

    return m_interface->GenerateDamageEvent(ped ? ped->GetPedInterface() : nullptr, responsible ? responsible->GetInterface() : nullptr, weaponType,
                                            damagePerHit, hitZone, dir);
}

bool CWeaponSA::ProcessLineOfSight(const CVector& vecStart, const CVector& vecEnd, CColPoint** colCollision, CEntity*& collisionEntity,
                                   const SLineOfSightFlags& flags, SLineOfSightBuildingResult* buildingResult, eWeaponType weaponType,
                                   CEntitySAInterface** entity)
{
    // Call CBirds::HandleGunShot
    ((void(__cdecl*)(const CVector*, const CVector*))FUNC_CBirds_CheckForHit)(&vecStart, &vecEnd);

    // Call CShadows::GunShotSetsOilOnFire
    ((void(__cdecl*)(const CVector&, const CVector&))FUNC_CShadows_CheckForHit)(vecStart, vecEnd);

    bool hit = pGame->GetWorld()->ProcessLineOfSight(&vecStart, &vecEnd, colCollision, &collisionEntity, flags, buildingResult);

    if (hit)
    {
        if (collisionEntity)
            *entity = collisionEntity->GetInterface();
        else
        {
            if (buildingResult->bValid)
                *entity = buildingResult->pInterface;
        }
    }

    // Call CWeapon::CheckForShootingVehicleOccupant
    if (*entity && (*entity)->nType == ENTITY_TYPE_VEHICLE)
        ((bool(__cdecl*)(CEntitySAInterface**, CColPointSAInterface*, eWeaponType, const CVector&,
                         const CVector&))FUNC_CWeapon_CheckForShootingVehicleOccupant)(entity, (*colCollision)->GetInterface(), weaponType, vecStart, vecEnd);

    return hit;
}

int CWeaponSA::GetWeaponReloadTime(CWeaponStat* weaponStat) const
{
    auto* weaponStats = static_cast<CWeaponStatSA*>(weaponStat);
    if (CWeaponInfoSAInterface* statInterface = weaponStats->GetInterface())
        return statInterface->GetWeaponReloadTime();

    return 0;
}

int CWeaponSA::GetWeaponFireTime(CWeaponStat* weaponStat)
{
    std::uint32_t timer = pGame->GetSystemTime();
    float         weaponFireTime = (weaponStat->GetWeaponAnimLoopStop() - weaponStat->GetWeaponAnimLoopStart()) * 1000.0f;
    return static_cast<int>(weaponFireTime);
}
