/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK scalar setters routed through the qualified native owner
 *****************************************************************************/
#pragma once

#include "CWeaponInfoReadDE.h"
#include "NativeWeaponInfoWriteAccess.h"

namespace mta::game_de
{
    // Scalar writes are persistent SDK intent. Slot remains pure until its DE
    // consumer bounds are qualified; the SDK enum alone cannot establish them.
    // Fire-offset methods stay pure for the subsequent borrowed-vector owner.
    class CWeaponInfoWriteDE : public CWeaponInfoReadDE
    {
    public:
        void SetWeaponRange(float value) override;
        void SetTargetRange(float value) override;
        void SetAccuracy(float value) override;
        void SetMoveSpeed(float value) override;
        void SetFiringSpeed(float value) override;
        void SetRadius(float value) override;
        void SetLifeSpan(float value) override;
        void SetSpread(float value) override;
        void SetAnimBreakoutTime(float value) override;
        void SetWeaponAnimLoopFireTime(float value) override;
        void SetDamagePerHit(short value) override;
        void SetMaximumClipAmmo(short value) override;
        void SetSkill(eWeaponSkill value) override;
        void SetRequiredStatLevel(float value) override;
        void SetFlags(int value) override;
        void SetFlag(DWORD mask) override;
        void ClearFlag(DWORD mask) override;
        void SetAnimGroup(DWORD value) override;

    protected:
        CWeaponInfoWriteDE(const NativeWeaponInfoReader& reader, std::size_t slot, NativeWeaponInfoWriteAccess& writer) noexcept
            : CWeaponInfoReadDE(reader, slot), native(writer), sdkSlot(slot)
        {
        }

    private:
        static void                  RequireWrite(ClockResult result, const char* operation);
        NativeWeaponInfoWriteAccess& native;
        std::size_t                  sdkSlot;
    };
    static_assert(std::is_abstract_v<CWeaponInfoWriteDE> && std::is_base_of_v<CWeaponInfo, CWeaponInfoWriteDE>);
}
