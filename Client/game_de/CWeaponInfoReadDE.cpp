/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual SDK read semantics without cached native weapon data
 *****************************************************************************/
#include "CWeaponInfoReadDE.h"

#include <stdexcept>
#include <string>

namespace mta::game_de
{
    NativeWeaponInfoValues CWeaponInfoReadDE::Require() const
    {
        NativeWeaponInfoValues values;
        const auto             result = native.Read(slot, values);
        // The SDK getters have no status channel. Refuse unavailable native
        // rows explicitly, instead of returning a stale or invented value.
        if (result != Status::Ok)
            throw std::runtime_error("DE CWeaponInfo row unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return values;
    }

    eWeaponModel CWeaponInfoReadDE::GetModel()
    {
        return static_cast<eWeaponModel>(Require().model);
    }

    eWeaponModel CWeaponInfoReadDE::GetModel2()
    {
        return static_cast<eWeaponModel>(Require().model2);
    }

    float CWeaponInfoReadDE::GetWeaponRange()
    {
        return Require().weaponRange;
    }

    float CWeaponInfoReadDE::GetTargetRange()
    {
        return Require().targetRange;
    }

    short CWeaponInfoReadDE::GetDamagePerHit()
    {
        return Require().damage;
    }

    float CWeaponInfoReadDE::GetAccuracy()
    {
        return Require().accuracy;
    }

    short CWeaponInfoReadDE::GetMaximumClipAmmo()
    {
        return Require().clipAmmo;
    }

    float CWeaponInfoReadDE::GetMoveSpeed()
    {
        return Require().moveSpeed;
    }

    float CWeaponInfoReadDE::GetFiringSpeed()
    {
        return Require().speed;
    }

    float CWeaponInfoReadDE::GetRadius()
    {
        return Require().radius;
    }

    float CWeaponInfoReadDE::GetLifeSpan()
    {
        return Require().lifeSpan;
    }

    float CWeaponInfoReadDE::GetSpread()
    {
        return Require().spread;
    }

    float CWeaponInfoReadDE::GetAnimBreakoutTime()
    {
        return Require().animBreakout;
    }

    eWeaponSlot CWeaponInfoReadDE::GetSlot()
    {
        return static_cast<eWeaponSlot>(Require().slot);
    }

    eWeaponSkill CWeaponInfoReadDE::GetSkill()
    {
        return static_cast<eWeaponSkill>(Require().skill);
    }

    float CWeaponInfoReadDE::GetRequiredStatLevel()
    {
        return static_cast<float>(Require().requiredStat);
    }

    int CWeaponInfoReadDE::GetFlags()
    {
        return Require().flags;
    }

    DWORD CWeaponInfoReadDE::GetAnimGroup()
    {
        return Require().animGroup;
    }

    eFireType CWeaponInfoReadDE::GetFireType()
    {
        return static_cast<eFireType>(Require().fireType);
    }

    float CWeaponInfoReadDE::GetWeaponAnimLoopStart()
    {
        return Require().animLoopStart;
    }

    float CWeaponInfoReadDE::GetWeaponAnimLoopStop()
    {
        return Require().animLoopEnd;
    }

    float CWeaponInfoReadDE::GetWeaponAnimLoopFireTime()
    {
        return Require().animFireTime;
    }

    float CWeaponInfoReadDE::GetWeaponAnim2LoopStart()
    {
        return Require().anim2LoopStart;
    }

    float CWeaponInfoReadDE::GetWeaponAnim2LoopStop()
    {
        return Require().anim2LoopEnd;
    }

    float CWeaponInfoReadDE::GetWeaponAnim2LoopFireTime()
    {
        return Require().anim2FireTime;
    }

    float CWeaponInfoReadDE::GetWeaponAnimBreakoutTime()
    {
        return Require().animBreakout;
    }

    float CWeaponInfoReadDE::GetWeaponRadius()
    {
        return Require().radius;
    }

    short CWeaponInfoReadDE::GetAimOffsetIndex()
    {
        return Require().aimOffset;
    }

    BYTE CWeaponInfoReadDE::GetDefaultCombo()
    {
        return Require().defaultCombo;
    }

    BYTE CWeaponInfoReadDE::GetCombosAvailable()
    {
        return Require().combosAvailable;
    }

    bool CWeaponInfoReadDE::IsFlagSet(DWORD flag)
    {
        // DWORD promotes the classic signed flags to an unsigned bit mask;
        // high-bit queries and an empty mask retain the existing SDK result.
        return (static_cast<std::uint32_t>(Require().flags) & flag) != 0;
    }
}
