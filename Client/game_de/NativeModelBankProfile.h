/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified model bank bounds and raw entry observations
 ****************************************************************************/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativeModelBankProfile
{
    inline constexpr std::uintptr_t BankRva = 0x5242fe0;
    inline constexpr std::uint32_t  ModelCount = 20000;
    inline constexpr std::size_t    SlotBytes = 8, BankBytes = ModelCount * SlotBytes;
    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
    };
    // Native RemoveModel routes index 20000 and higher to separate TXD/IFP/etc
    // stores. The bank belongs only to model slots below that boundary. Raw
    // ped WORD ffff stays an out-of-range value, never a masked model index.
    inline constexpr std::array<unsigned char, 25> NativeModelBankUpperBound = {0x81, 0xff, 0x20, 0x4e, 0x00, 0x00, 0x7d, 0x6b, 0x48, 0x8b, 0x9c, 0xfd, 0xe0,
                                                                                0x2f, 0x24, 0x05, 0x48, 0x8b, 0xcb, 0x48, 0x8b, 0x03, 0xff, 0x50, 0x50};
    inline constexpr std::array<unsigned char, 43> NativeModelBankAssignment = {
        0x48, 0x8b, 0x3d, 0xb7, 0x6b, 0x5f, 0x04, 0x4c, 0x8d, 0x35, 0x60, 0x14, 0xed, 0xfe, 0x48, 0x63, 0xc2, 0x48, 0x81, 0xc7, 0x10, 0x05,
        0x00, 0x00, 0x66, 0x89, 0x51, 0x3a, 0x48, 0x8b, 0xe9, 0x48, 0x0f, 0xbf, 0xda, 0x49, 0x8b, 0xb4, 0xc6, 0xe0, 0x2f, 0x24, 0x05};
    inline constexpr std::array<unsigned char, 18> NativeModelBankDestructorLookup = {0x48, 0x8d, 0x15, 0x38, 0x15, 0xed, 0xfe, 0x49, 0x8b,
                                                                                      0xc3, 0x4a, 0x8b, 0x8c, 0xda, 0xe0, 0x2f, 0x24, 0x05};
    inline constexpr std::array<Pin, 3>            Code = {{
        {0x1289051, NativeModelBankUpperBound.data(), NativeModelBankUpperBound.size()},
        {0x112eb92, NativeModelBankAssignment.data(), NativeModelBankAssignment.size()},
        {0x112eac1, NativeModelBankDestructorLookup.data(), NativeModelBankDestructorLookup.size()},
    }};
    inline constexpr std::size_t                   MaximumPinBytes = 43;
}
