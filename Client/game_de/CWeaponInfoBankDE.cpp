/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Stable genuine SDK wrappers for the qualified native weapon bank
 *****************************************************************************/
#include "CWeaponInfoBankDE.h"

namespace mta::game_de
{
    CWeaponInfoBankDE::CWeaponInfoBankDE(const NativeWeaponInfoReader& reader, NativeWeaponInfoWriteAccess& writes,
                                         NativeWeaponInfoVectorAccess& vectors) noexcept
        : CWeaponInfoBankDE(reader, writes, vectors, std::make_index_sequence<NativeWeaponInfoProfile::RowCount>{})
    {
    }

    CWeaponInfo* CWeaponInfoBankDE::GetSlot(std::size_t slot) noexcept
    {
        // Constructing wrappers grants no access to the engine. The existing
        // native owner qualifies the row and callback before publishing one.
        return slot < wrappers.size() ? &wrappers[slot] : nullptr;
    }
}
