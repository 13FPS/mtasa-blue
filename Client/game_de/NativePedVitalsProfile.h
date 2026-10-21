/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse qualified native current-player vitals producer evidence
 ****************************************************************************/
#pragma once
#include "NativePlayerInfoExtraProfile.h"

namespace mta::game_de::NativePedVitalsProfile
{
    inline constexpr std::size_t    HealthOffset = 0x76c, MaxHealthOffset = 0x770, ArmorOffset = 0x774;
    inline constexpr std::size_t    TypeOffset = 0x6a, ScalarBytes = 4;
    inline constexpr std::uintptr_t PlayerVtableRva = NativePlayerInfoProfile::PlayerVtableRva;
    using Pin = NativePlayerInfoProfile::Pin;

    constexpr Pin SavedPin(std::uintptr_t rva) noexcept
    {
        for (const auto& pin : NativePlayerInfoExtraProfile::Code)
            if (pin.rva == rva)
                return pin;
        return {};
    }
    // Reuse the already-qualified player ability producers rather than copy
    // classic offsets or rescan an executable for a second equivalent profile.
    // Current health/armour remain distinct from max-health and ability bytes.
    inline constexpr std::array<Pin, 4> Code = {{SavedPin(0x107dfc2), SavedPin(0x107e191), SavedPin(0x116d993), SavedPin(0x10de382)}};
    static_assert(Code[0].rva == 0x107dfc2 && Code[0].size == 48 && Code[1].rva == 0x107e191 && Code[1].size == 33);
    static_assert(Code[2].rva == 0x116d993 && Code[2].size == 44 && Code[3].rva == 0x10de382 && Code[3].size == 56);
    static_assert(HealthOffset + ScalarBytes == MaxHealthOffset && MaxHealthOffset + ScalarBytes == ArmorOffset);
    static_assert(HealthOffset % ScalarBytes == 0 && ArmorOffset % ScalarBytes == 0);
}
