/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the existing MTA custom weapon-stat data and policy
 ****************************************************************************/
#pragma once

#include <windows.h>
#include <cstdint>
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "CWeaponStat.h"

namespace MtaGame
{
    // This is the existing MTA value record. It is CPU storage rather than a
    // cast of either executable's native row layout.
    struct WeaponStatData
    {
        eFireType fire_type;  // type - instant hit (e.g. pistol), projectile (e.g. rocket launcher), area effect (e.g. flame thrower)

        float target_range;  // max targeting range
        float weapon_range;  // absolute gun range / default melee attack range
        int   model;         // modelinfo id
        int   model2;        // second modelinfo id

        eWeaponSlot weapon_slot;
        int         flags;  // flags defining characteristics

        // instead of storing pointers directly to anims, use anim association groups
        // NOTE: this is used for stealth kill anims for melee weapons
        DWORD anim_group;

        //////////////////////////////////
        // Gun Data
        /////////////////////////////////
        short   maximum_clip_ammo;  // ammo in one clip
        short   damage;             // damage inflicted per hit
        CVector fire_offset;        // offset from weapon origin to projectile starting point

        // skill settings
        eWeaponSkill skill_level;           // what's the skill level of this weapontype
        int          required_skill_level;  // what stat level is required for this skill level
        float        accuracy;              // modify accuracy of weapon
        float        move_speed;            // how fast can move with weapon

        // anim timings
        float anim_loop_start;        // start of animation loop
        float anim_loop_stop;         // end of animation loop
        float anim_loop_bullet_fire;  // time in animation when weapon should be fired

        float anim2_loop_start;        // start of animation2 loop
        float anim2_loop_stop;         // end of animation2 loop
        float anim2_loop_bullet_fire;  // time in animation2 when weapon should be fired

        float anim_breakout_time;  // time after which player can break out of attack and run off

        // projectile/area effect specific info
        float firing_speed;  // speed of projectile
        float radius;        // radius affected
        float life_span;     // time taken for shot to dissipate
        float spread;        // angle inside which shots are created

        short aim_offset;  // index into array of aiming offsets
        //////////////////////////////////
        // Melee Data
        /////////////////////////////////
        BYTE default_combo;     // base combo for this melee weapon
        BYTE combos_available;  // how many further combos are available
    };
}
