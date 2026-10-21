/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the existing MTA custom weapon-stat data and policy
 ****************************************************************************/
#pragma once

#include "CWeaponStatDefaults.h"
#include "CWeaponStatFlagPolicy.h"
#include <type_traits>

namespace MtaGame
{
    // CGame::CreateWeaponStat creates unlisted CPU data in MTA. Owning the
    // record here preserves its mutable borrowed-vector contract without
    // exposing native banks or claiming that local edits change live weapons.
    class CWeaponStatOwned final : public CWeaponStat
    {
    public:
        CWeaponStatOwned(eWeaponType weapon, eWeaponSkill skill, WeaponStatOriginalLookup lookup = nullptr, void* context = nullptr)
            : weaponType(weapon), skillLevel(skill), originalLookup(lookup ? lookup : LookupDefaults), originalContext(context)
        {
            CWeaponStatDefaults::Load(this, weapon, skill);
        }
        eWeaponType  GetWeaponType() override;
        eWeaponSkill GetWeaponSkillLevel() override;

        void SetWeaponType(eWeaponType weaponType) override;
        void SetWeaponSkillLevel(eWeaponSkill skillLevel) override;

        //
        // Interface Sets and Gets
        //

        void ToggleFlagBits(DWORD flagBits) override;
        void SetFlagBits(DWORD flagBits) override;
        void ClearFlagBits(DWORD flagBits) override;
        // For initialization only
        void SetFlags(int iFlags) override { data.flags = iFlags; }
        bool IsFlagSet(DWORD flag) override { return ((data.flags & flag) > 0 ? true : false); }
        int  GetFlags() override { return data.flags; }

        eWeaponModel GetModel() override { return (eWeaponModel)data.model; }
        void         SetModel(int iModel) override { data.model = (int)iModel; }

        eWeaponModel GetModel2() override { return (eWeaponModel)data.model2; }
        void         SetModel2(int iModel) override { data.model2 = (int)iModel; }

        float GetWeaponRange() override { return data.weapon_range; };
        void  SetWeaponRange(float fRange) override { data.weapon_range = fRange; };

        float GetTargetRange() override { return data.target_range; };
        void  SetTargetRange(float fRange) override { data.target_range = fRange; };

        CVector* GetFireOffset() override { return &data.fire_offset; };
        void     SetFireOffset(CVector* vecFireOffset) override { data.fire_offset = *vecFireOffset; };

        short GetDamagePerHit() override { return data.damage; };
        void  SetDamagePerHit(short sDamagePerHit) override { data.damage = sDamagePerHit; };

        float GetAccuracy() override { return data.accuracy; };
        void  SetAccuracy(float fAccuracy) override { data.accuracy = fAccuracy; };

        short GetMaximumClipAmmo() override { return data.maximum_clip_ammo; };
        void  SetMaximumClipAmmo(short sAccuracy) override { data.maximum_clip_ammo = sAccuracy; };

        float GetMoveSpeed() override { return data.move_speed; };
        void  SetMoveSpeed(float fMoveSpeed) override { data.move_speed = fMoveSpeed; };

        // projectile/areaeffect only
        float GetFiringSpeed() override { return data.firing_speed; };
        void  SetFiringSpeed(float fFiringSpeed) override { data.firing_speed = fFiringSpeed; };

        // area effect only
        float GetRadius() override { return data.radius; };
        void  SetRadius(float fRadius) override { data.radius = fRadius; };

        float GetLifeSpan() override { return data.life_span; };
        void  SetLifeSpan(float fLifeSpan) override { data.life_span = fLifeSpan; };

        float GetSpread() override { return data.spread; };
        void  SetSpread(float fSpread) override { data.spread = fSpread; };

        float GetAnimBreakoutTime() override { return data.anim_breakout_time; };
        void  SetAnimBreakoutTime(float fBreakoutTime) override { data.anim_breakout_time = fBreakoutTime; };

        eWeaponSlot GetSlot() override { return (eWeaponSlot)data.weapon_slot; };
        void        SetSlot(eWeaponSlot dwSlot) override { data.weapon_slot = (eWeaponSlot)dwSlot; };

        eWeaponSkill GetSkill() override { return data.skill_level; }
        void         SetSkill(eWeaponSkill weaponSkill) override { data.skill_level = weaponSkill; }

        float GetRequiredStatLevel() override { return static_cast<float>(data.required_skill_level); }
        void  SetRequiredStatLevel(float fStatLevel) override { data.required_skill_level = static_cast<int>(fStatLevel); }
        void  SetRequiredStatLevel(int iStatLevel) override { data.required_skill_level = iStatLevel; }

        DWORD GetAnimGroup() override { return data.anim_group; }
        void  SetAnimGroup(DWORD dwAnimGroup) override { data.anim_group = dwAnimGroup; }

        eFireType GetFireType() override { return data.fire_type; }
        void      SetFireType(eFireType type) override { data.fire_type = type; }

        // Floats
        float GetWeaponAnimLoopStart() override { return data.anim_loop_start; }
        void  SetWeaponAnimLoopStart(float animLoopStart) override { data.anim_loop_start = animLoopStart; }

        float GetWeaponAnimLoopStop() override { return data.anim_loop_stop; }
        void  SetWeaponAnimLoopStop(float animLoopEnd) override { data.anim_loop_stop = animLoopEnd; }

        float GetWeaponAnimLoopFireTime() override { return data.anim_loop_bullet_fire; }
        void  SetWeaponAnimLoopFireTime(float animFireTime) override { data.anim_loop_bullet_fire = animFireTime; }

        float GetWeaponAnim2LoopStart() override { return data.anim2_loop_start; }
        void  SetWeaponAnim2LoopStart(float anim2LoopStart) override { data.anim2_loop_start = anim2LoopStart; }

        float GetWeaponAnim2LoopStop() override { return data.anim2_loop_stop; }
        void  SetWeaponAnim2LoopStop(float anim2LoopEnd) override { data.anim2_loop_stop = anim2LoopEnd; }

        float GetWeaponAnim2LoopFireTime() override { return data.anim2_loop_bullet_fire; }
        void  SetWeaponAnim2LoopFireTime(float anim2FireTime) override { data.anim2_loop_bullet_fire = anim2FireTime; }

        float GetWeaponAnimBreakoutTime() override { return data.anim_breakout_time; }
        void  SetWeaponAnimBreakoutTime(float animBreakoutTime) override { data.anim_breakout_time = animBreakoutTime; }

        float GetWeaponSpeed() override { return data.firing_speed; }
        void  SetWeaponSpeed(float fSpeed) override { data.firing_speed = fSpeed; }

        float GetWeaponRadius() override { return data.radius; }
        void  SetWeaponRadius(float fRadius) override { data.radius = fRadius; }

        // Ints
        short GetAimOffsetIndex() override { return data.aim_offset; }
        void  SetAimOffsetIndex(short sIndex) override { data.aim_offset = sIndex; }

        BYTE GetDefaultCombo() override { return data.default_combo; }
        void SetDefaultCombo(BYTE defaultCombo) override { data.default_combo = defaultCombo; }

        BYTE GetCombosAvailable() override { return data.combos_available; }
        void SetCombosAvailable(BYTE nCombosAvailable) override { data.combos_available = nCombosAvailable; }

    private:
        static WeaponStatOriginal LookupDefaults(void*, eWeaponType weapon, eWeaponSkill skill)
        {
            const auto* original = CWeaponStatDefaults::Select(weapon, skill);
            return original ? WeaponStatOriginal{static_cast<DWORD>(original->flags), original->anim_group} : WeaponStatOriginal{};
        }
        void HandleFlagsValueChange(DWORD desired) { ApplyWeaponStatFlags(*this, desired, originalLookup, originalContext); }

        eWeaponType              weaponType;
        eWeaponSkill             skillLevel;
        WeaponStatData           data{};
        WeaponStatOriginalLookup originalLookup;
        void*                    originalContext;
    };
    static_assert(!std::is_abstract_v<CWeaponStatOwned> && std::is_base_of_v<CWeaponStat, CWeaponStatOwned>);

    inline eWeaponType CWeaponStatOwned::GetWeaponType()
    {
        return weaponType;
    }
    inline eWeaponSkill CWeaponStatOwned::GetWeaponSkillLevel()
    {
        return skillLevel;
    }
    inline void CWeaponStatOwned::SetWeaponType(eWeaponType value)
    {
        weaponType = value;
    }
    inline void CWeaponStatOwned::SetWeaponSkillLevel(eWeaponSkill value)
    {
        skillLevel = value;
    }
    inline void CWeaponStatOwned::ToggleFlagBits(DWORD bits)
    {
        HandleFlagsValueChange(static_cast<DWORD>(data.flags) ^ bits);
    }
    inline void CWeaponStatOwned::SetFlagBits(DWORD bits)
    {
        HandleFlagsValueChange(static_cast<DWORD>(data.flags) | bits);
    }
    inline void CWeaponStatOwned::ClearFlagBits(DWORD bits)
    {
        HandleFlagsValueChange(static_cast<DWORD>(data.flags) & ~bits);
    }
}
