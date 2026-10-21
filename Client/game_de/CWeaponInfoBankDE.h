/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Stable genuine SDK wrappers for the qualified native weapon bank
 *****************************************************************************/
#pragma once

#include "CWeaponInfoDE.h"
#include "NativeWeaponInfoProfile.h"
#include <game/CWeaponInfoIndexPolicy.h>
#include <array>
#include <utility>

namespace mta::game_de
{
    // MTA retains weapon-info pointers between calls. Own every actual SDK
    // object with the runtime rather than allocating temporary row wrappers or
    // treating a native data row as a polymorphic CWeaponInfo object.
    class CWeaponInfoBankDE final
    {
    public:
        CWeaponInfoBankDE(const NativeWeaponInfoReader& reader, NativeWeaponInfoWriteAccess& writes, NativeWeaponInfoVectorAccess& vectors) noexcept;
        CWeaponInfo* GetSlot(std::size_t slot) noexcept;

        CWeaponInfoBankDE(const CWeaponInfoBankDE&) = delete;
        CWeaponInfoBankDE& operator=(const CWeaponInfoBankDE&) = delete;
        CWeaponInfoBankDE(CWeaponInfoBankDE&&) = delete;
        CWeaponInfoBankDE& operator=(CWeaponInfoBankDE&&) = delete;

    private:
        template <std::size_t... Index>
        CWeaponInfoBankDE(const NativeWeaponInfoReader& reader, NativeWeaponInfoWriteAccess& writes, NativeWeaponInfoVectorAccess& vectors,
                          std::index_sequence<Index...>) noexcept
            : wrappers{{CWeaponInfoDE(reader, writes, vectors, Index)...}}
        {
        }

        std::array<CWeaponInfoDE, NativeWeaponInfoProfile::RowCount> wrappers;
    };
    static_assert(NativeWeaponInfoProfile::RowCount == MtaGame::CWeaponInfoIndexPolicy::SlotCount);
}
