/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same-owner access to native weapon slots and borrowed fire offsets
 *****************************************************************************/
#pragma once

// These SDK headers normally inherit Windows types and vector constants from
// the classic PCH. Native backend translation units supply them explicitly.
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <game/CGameClockService.h>
#include <game/CWeaponInfo.h>
#include <CVector.h>
#include <cstddef>

namespace mta::game_de
{
    // The SDK exposes a borrowed mutable native vector. Returning a cached copy
    // would silently change MTA's contract; only the engine owner can qualify
    // this actual image-backed alias. Refusing owners leave output unchanged.
    class NativeWeaponInfoVectorAccess
    {
    public:
        virtual ClockResult BorrowWeaponFireOffset(std::size_t, CVector*&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetWeaponFireOffset(std::size_t, const CVector*) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetWeaponSlot(std::size_t, eWeaponSlot) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWeaponInfoVectorAccess() = default;
    };
}
