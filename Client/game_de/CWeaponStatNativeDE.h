/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the genuine weapon bank behind MTA's native stat SDK slice
 ****************************************************************************/
#pragma once

#include "CWeaponInfoBankDE.h"
#include <game/CWeaponStatFlagPolicy.h>
#include <type_traits>

namespace mta::game_de
{
    // Native-backed stats share the runtime's existing SDK bank and writer.
    // They must not expose a copied CPU record as a live engine object. The
    // final combo-count method preserves the exact SDK byte-storage contract;
    // it does not claim that every stored byte is a valid gameplay combo range.
    class CWeaponStatNativeDE : public CWeaponStat
    {
    public:
        eWeaponType  GetWeaponType() override { return weaponType; }
        eWeaponSkill GetWeaponSkillLevel() override { return skillLevel; }
        void         SetWeaponType(eWeaponType value) override { weaponType = value; }
        void         SetWeaponSkillLevel(eWeaponSkill value) override { skillLevel = value; }

        void         ToggleFlagBits(DWORD bits) override;
        void         SetFlagBits(DWORD bits) override;
        void         ClearFlagBits(DWORD bits) override;
        void         SetFlags(int value) override { info.SetFlags(value); }
        bool         IsFlagSet(DWORD bits) override { return info.IsFlagSet(bits); }
        int          GetFlags() override { return info.GetFlags(); }
        eWeaponModel GetModel() override { return info.GetModel(); }
        void         SetModel(int value) override;
        eWeaponModel GetModel2() override { return info.GetModel2(); }
        void         SetModel2(int value) override;
        float        GetWeaponRange() override { return info.GetWeaponRange(); }
        void         SetWeaponRange(float value) override { info.SetWeaponRange(value); }
        float        GetTargetRange() override { return info.GetTargetRange(); }
        void         SetTargetRange(float value) override { info.SetTargetRange(value); }
        CVector*     GetFireOffset() override { return info.GetFireOffset(); }
        void         SetFireOffset(CVector* value) override { info.SetFireOffset(value); }
        short        GetDamagePerHit() override { return info.GetDamagePerHit(); }
        void         SetDamagePerHit(short value) override { info.SetDamagePerHit(value); }
        float        GetAccuracy() override { return info.GetAccuracy(); }
        void         SetAccuracy(float value) override { info.SetAccuracy(value); }
        short        GetMaximumClipAmmo() override { return info.GetMaximumClipAmmo(); }
        void         SetMaximumClipAmmo(short value) override { info.SetMaximumClipAmmo(value); }
        float        GetMoveSpeed() override { return info.GetMoveSpeed(); }
        void         SetMoveSpeed(float value) override { info.SetMoveSpeed(value); }
        float        GetFiringSpeed() override { return info.GetFiringSpeed(); }
        void         SetFiringSpeed(float value) override { info.SetFiringSpeed(value); }
        float        GetRadius() override { return info.GetRadius(); }
        void         SetRadius(float value) override { info.SetRadius(value); }
        float        GetLifeSpan() override { return info.GetLifeSpan(); }
        void         SetLifeSpan(float value) override { info.SetLifeSpan(value); }
        float        GetSpread() override { return info.GetSpread(); }
        void         SetSpread(float value) override { info.SetSpread(value); }
        float        GetAnimBreakoutTime() override { return info.GetAnimBreakoutTime(); }
        void         SetAnimBreakoutTime(float value) override { info.SetAnimBreakoutTime(value); }
        eWeaponSlot  GetSlot() override { return info.GetSlot(); }
        void         SetSlot(eWeaponSlot value) override { info.SetSlot(value); }
        eWeaponSkill GetSkill() override { return info.GetSkill(); }
        void         SetSkill(eWeaponSkill value) override { info.SetSkill(value); }
        float        GetRequiredStatLevel() override { return info.GetRequiredStatLevel(); }
        void         SetRequiredStatLevel(float value) override { info.SetRequiredStatLevel(value); }
        void         SetRequiredStatLevel(int value) override;
        DWORD        GetAnimGroup() override { return info.GetAnimGroup(); }
        void         SetAnimGroup(DWORD value) override { info.SetAnimGroup(value); }
        eFireType    GetFireType() override { return info.GetFireType(); }
        void         SetFireType(eFireType value) override;
        float        GetWeaponAnimLoopStart() override { return info.GetWeaponAnimLoopStart(); }
        void         SetWeaponAnimLoopStart(float value) override;
        float        GetWeaponAnimLoopStop() override { return info.GetWeaponAnimLoopStop(); }
        void         SetWeaponAnimLoopStop(float value) override;
        float        GetWeaponAnimLoopFireTime() override { return info.GetWeaponAnimLoopFireTime(); }
        void         SetWeaponAnimLoopFireTime(float value) override { info.SetWeaponAnimLoopFireTime(value); }
        float        GetWeaponAnim2LoopStart() override { return info.GetWeaponAnim2LoopStart(); }
        void         SetWeaponAnim2LoopStart(float value) override;
        float        GetWeaponAnim2LoopStop() override { return info.GetWeaponAnim2LoopStop(); }
        void         SetWeaponAnim2LoopStop(float value) override;
        float        GetWeaponAnim2LoopFireTime() override { return info.GetWeaponAnim2LoopFireTime(); }
        void         SetWeaponAnim2LoopFireTime(float value) override;
        float        GetWeaponAnimBreakoutTime() override { return info.GetWeaponAnimBreakoutTime(); }
        void         SetWeaponAnimBreakoutTime(float value) override { info.SetAnimBreakoutTime(value); }
        float        GetWeaponSpeed() override { return info.GetFiringSpeed(); }
        void         SetWeaponSpeed(float value) override { info.SetFiringSpeed(value); }
        float        GetWeaponRadius() override { return info.GetWeaponRadius(); }
        void         SetWeaponRadius(float value) override { info.SetRadius(value); }
        short        GetAimOffsetIndex() override { return info.GetAimOffsetIndex(); }
        void         SetAimOffsetIndex(short value) override;
        BYTE         GetDefaultCombo() override { return info.GetDefaultCombo(); }
        void         SetDefaultCombo(BYTE value) override;
        BYTE         GetCombosAvailable() override { return info.GetCombosAvailable(); }
        void         SetCombosAvailable(BYTE value) override;

    protected:
        // Like the classic live wrapper, later metadata edits change original
        // lookups without silently rebinding the already selected native row.
        // The bank, writer and mutable original provider must outlive the stat.
        CWeaponStatNativeDE(CWeaponInfoBankDE& bank, NativeWeaponInfoWriteAccess& writes, std::size_t row, eWeaponType weapon, eWeaponSkill skill,
                            MtaGame::WeaponStatOriginalLookup lookup, void* context);

    private:
        static CWeaponInfo&                     RequireSlot(CWeaponInfoBankDE& bank, std::size_t row);
        static void                             RequireWrite(ClockResult result, const char* operation);
        void                                    Flags(DWORD desired);
        CWeaponInfo&                            info;
        NativeWeaponInfoWriteAccess&            native;
        const std::size_t                       slot;
        eWeaponType                             weaponType;
        eWeaponSkill                            skillLevel;
        const MtaGame::WeaponStatOriginalLookup originalLookup;
        void* const                             originalContext;
    };
    static_assert(!std::is_abstract_v<CWeaponStatNativeDE> && std::is_base_of_v<CWeaponStat, CWeaponStatNativeDE>);
}
