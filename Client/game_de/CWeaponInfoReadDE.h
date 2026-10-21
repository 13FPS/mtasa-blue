/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA weapon-info scalar getters over qualified native DE rows
 *****************************************************************************/
#pragma once

#include "NativeWeaponInfoReader.h"
#include <windows.h>
#include <game/CWeaponInfo.h>

namespace mta::game_de
{
    // Keep mutation and pointer-returning SDK methods pure. A read-only row
    // snapshot cannot supply live writable fire-offset pointers or authorize
    // network/script writes, so this class remains an internal partial slice.
    class CWeaponInfoReadDE : public CWeaponInfo
    {
    public:
        eWeaponModel GetModel() override;
        eWeaponModel GetModel2() override;
        float        GetWeaponRange() override;
        float        GetTargetRange() override;
        short        GetDamagePerHit() override;
        float        GetAccuracy() override;
        short        GetMaximumClipAmmo() override;
        float        GetMoveSpeed() override;
        float        GetFiringSpeed() override;
        float        GetRadius() override;
        float        GetLifeSpan() override;
        float        GetSpread() override;
        float        GetAnimBreakoutTime() override;
        eWeaponSlot  GetSlot() override;
        eWeaponSkill GetSkill() override;
        float        GetRequiredStatLevel() override;
        int          GetFlags() override;
        DWORD        GetAnimGroup() override;
        eFireType    GetFireType() override;
        float        GetWeaponAnimLoopStart() override;
        float        GetWeaponAnimLoopStop() override;
        float        GetWeaponAnimLoopFireTime() override;
        float        GetWeaponAnim2LoopStart() override;
        float        GetWeaponAnim2LoopStop() override;
        float        GetWeaponAnim2LoopFireTime() override;
        float        GetWeaponAnimBreakoutTime() override;
        float        GetWeaponRadius() override;
        short        GetAimOffsetIndex() override;
        BYTE         GetDefaultCombo() override;
        BYTE         GetCombosAvailable() override;
        bool         IsFlagSet(DWORD flag) override;

    protected:
        CWeaponInfoReadDE(const NativeWeaponInfoReader& reader, std::size_t sdkSlot) noexcept : native(reader), slot(sdkSlot) {}

    private:
        NativeWeaponInfoValues        Require() const;
        const NativeWeaponInfoReader& native;
        std::size_t                   slot;
    };
    static_assert(std::is_abstract_v<CWeaponInfoReadDE> && std::is_base_of_v<CWeaponInfo, CWeaponInfoReadDE>);
}
