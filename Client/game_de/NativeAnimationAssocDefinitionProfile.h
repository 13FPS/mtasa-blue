/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual association-definition count and inline group-name provenance
 ****************************************************************************/
#pragma once
#include "NativePedMoveAnimProfile.h"
namespace mta::game_de::NativeAnimationAssocDefinitionProfile
{
    inline constexpr std::uintptr_t CountRva = 0x51afc64, BankRva = 0x502c180;
    inline constexpr std::size_t    RowBytes = 0x38, NameOffset = 0;
    // Named0245 actually reads the signed count and compares the inline name
    // at the start of each56-byte definition. Reuse those exact saved pins;
    // this is neither animation-block count nor a guessed SA structure cast.
    inline constexpr auto& Code = NativePedMoveAnimProfile::Code;
    inline constexpr auto  MaximumPinBytes = NativePedMoveAnimProfile::MaximumPinBytes;
    inline constexpr auto  GroupSlotRva = NativePedMoveAnimProfile::GroupSlotRva;
    inline constexpr auto  GroupRva = NativePedMoveAnimProfile::GroupRva;
}
