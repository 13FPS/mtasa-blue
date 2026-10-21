/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the SDK's borrowed vector and native slot contracts
 *****************************************************************************/
#include "CWeaponInfoDE.h"
#include "NativeWeaponInfoVectorProfile.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CWeaponInfoDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE CWeaponInfo ") + operation + " unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) +
                                     ")");
    }
    CVector* CWeaponInfoDE::GetFireOffset()
    {
        CVector* output = nullptr;
        Require(native.BorrowWeaponFireOffset(sdkSlot, output), "GetFireOffset");
        if (!output)
            Require(ClockResult::ReadFailure, "GetFireOffset");
        return output;
    }
    void CWeaponInfoDE::SetFireOffset(CVector* value)
    {
        if (!value)
            Require(ClockResult::InvalidArgument, "SetFireOffset");
        Require(native.SetWeaponFireOffset(sdkSlot, value), "SetFireOffset");
    }
    void CWeaponInfoDE::SetSlot(eWeaponSlot value)
    {
        // Native consumers, not just the classic enum, establish thirteen slots.
        // The actual memory owner also enforces its independently pinned bound.
        if (static_cast<std::uint32_t>(value) >= NativeWeaponInfoVectorProfile::SlotCount)
            Require(ClockResult::InvalidArgument, "SetSlot");
        Require(native.SetWeaponSlot(sdkSlot, value), "SetSlot");
    }
}
