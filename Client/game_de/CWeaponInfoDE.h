/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete actual weapon SDK object using the qualified native bank
 *****************************************************************************/
#pragma once

#include "CWeaponInfoWriteDE.h"
#include "NativeWeaponInfoVectorAccess.h"

namespace mta::game_de
{
    // Completing this SDK object does not manufacture a CGame factory or bypass
    // engine ownership. The existing MTA owner supplies both access interfaces.
    class CWeaponInfoDE final : public CWeaponInfoWriteDE
    {
    public:
        CWeaponInfoDE(const NativeWeaponInfoReader& reader, NativeWeaponInfoWriteAccess& writes, NativeWeaponInfoVectorAccess& vectors,
                      std::size_t slot) noexcept
            : CWeaponInfoWriteDE(reader, slot, writes), native(vectors), sdkSlot(slot)
        {
        }
        CVector* GetFireOffset() override;
        void     SetFireOffset(CVector* value) override;
        void     SetSlot(eWeaponSlot value) override;

    private:
        static void                   Require(ClockResult result, const char* operation);
        NativeWeaponInfoVectorAccess& native;
        std::size_t                   sdkSlot;
    };
    static_assert(!std::is_abstract_v<CWeaponInfoDE> && std::is_base_of_v<CWeaponInfo, CWeaponInfoDE>);
}
