/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's actual bullet-fire callback and muzzle itinerary
 ****************************************************************************/
#pragma once
#include "CWeaponInfo.h"
#include <CVector.h>
namespace MtaGame
{
    struct CWeaponFireBulletPolicy final
    {
        template <class Weapon, class Entity, class Ignore, class PlayerQuery, class Pre, class StatManagerQuery, class Transform, class Audio, class Post>
        static bool Fire(Weapon& weapon, Entity* firingEntity, const CVector& origin, const CVector& target, Ignore&& ignore, PlayerQuery&& asPlayer,
                         Pre&& preFire, StatManagerQuery&& statManager, Transform&& transformMuzzle, Audio&& fireAudio, Post&& postFire)
        {
            if (!firingEntity)
                return false;
            switch (weapon.GetType())
            {
                case WEAPONTYPE_PISTOL:
                case WEAPONTYPE_PISTOL_SILENCED:
                case WEAPONTYPE_DESERT_EAGLE:
                case WEAPONTYPE_SHOTGUN:
                case WEAPONTYPE_SAWNOFF_SHOTGUN:
                case WEAPONTYPE_SPAS12_SHOTGUN:
                case WEAPONTYPE_MICRO_UZI:
                case WEAPONTYPE_MP5:
                case WEAPONTYPE_AK47:
                case WEAPONTYPE_M4:
                case WEAPONTYPE_TEC9:
                case WEAPONTYPE_COUNTRYRIFLE:
                case WEAPONTYPE_SNIPERRIFLE:
                case WEAPONTYPE_MINIGUN:
                {
                    ignore(firingEntity);
                    auto* player = asPlayer(firingEntity);
                    // The gates retain fresh backend handler lookups, including
                    // the original handler-before-player condition evaluation.
                    // A pre handler may change this weapon's type or services;
                    // the second type and stat lookup must happen afterwards.
                    preFire(player);
                    // Keep manager receiver evaluation before the fresh type
                    // argument, as in the actual classic call expression.
                    auto*   info = statManager()->GetWeaponStatsFromSkillLevel(weapon.GetType(), 999.0f);
                    CVector muzzle = *info->GetFireOffset();
                    if (player)
                        transformMuzzle(player, &muzzle);
                    // Preserve the original aliases and native call's optional
                    // arguments. MTA deliberately ignores its returned bool.
                    // Exceptions keep the original prefix of accepted effects;
                    // adding cleanup/rollback here would change that contract.
                    weapon.FireInstantHit(firingEntity, const_cast<CVector*>(&origin), &muzzle, nullptr, const_cast<CVector*>(&target), nullptr, false, true);
                    if (player)
                        fireAudio(player);
                    postFire(player);
                    ignore(nullptr);
                    return true;
                }
                default:
                    break;
            }
            return false;
        }
    };
}
