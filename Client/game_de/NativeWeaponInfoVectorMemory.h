/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native borrowed-vector access and a conditional partial-write journal
 *****************************************************************************/
#pragma once

#include "NativeWeaponInfoReader.h"
#include "NativeWeaponInfoProfile.h"
#include "NativeWeaponInfoWriteMemory.h"
#include "NativeWeaponInfoVectorAccess.h"
#include <array>

namespace mta::game_de::NativeWeaponInfoVectorMemory
{
    struct FireOffsetJournal
    {
        std::array<std::uint32_t, 3> original{}, target{};
        std::uintptr_t               image = 0;
        std::size_t                  slot = NativeWeaponInfoProfile::RowCount;
        std::uint8_t                 appliedMask = 0, restoredMask = 0, foreignMask = 0, remainingMask = 0;
        ClockResult                  failure = ClockResult::Ok;
        bool                         committed = false;
    };

    // A successful borrow returns the actual row+0x24 alias, physically mapped
    // for the executable's lifetime. As in classic MTA, callers must respect
    // ordinary borrowed-pointer access/lifetime responsibility after return.
    // Callback admission is required by the real owner, even though owned image
    // fixtures may omit it. These helpers never create an engine write lease.
    ClockResult BorrowFireOffset(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, CVector*& output,
                                 NativeWeaponInfoWriteMemory::CheckOwner checkOwner = nullptr) noexcept;
    ClockResult SetSlot(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, eWeaponSlot value,
                        NativeWeaponInfoWriteMemory::CheckOwner checkOwner = nullptr) noexcept;
    // Success persists XYZ. Failure conditionally retires only this call's
    // still-owned components, with exact CAS preserving later foreign writes.
    // RestoreFailure reports remaining owned partial intent in the journal;
    // it never authorizes a blind row restore or writes after owner retirement.
    ClockResult SetFireOffset(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, const CVector* source,
                              NativeWeaponInfoWriteMemory::CheckOwner checkOwner = nullptr, FireOffsetJournal* journal = nullptr) noexcept;
    // The retained real owner must service remaining partial debt on later
    // admitted callbacks before detach. Success never rolls back a committed
    // vector; recovery requires the journal's exact original image/row identity.
    // OwnerConflict means foreign supersession retired all remaining debt;
    // unreadable/unrestored owned components continue to report RestoreFailure.
    ClockResult RecoverFireOffset(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, FireOffsetJournal& journal,
                                  NativeWeaponInfoWriteMemory::CheckOwner checkOwner = nullptr) noexcept;
}
