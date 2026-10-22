/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact named vehicle-shot bool storage, distinct from unproved test flag
 ****************************************************************************/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mta::game_de::NativePedShotFlagsProfile
{
    inline constexpr std::size_t    FlagsOffset = 0x638, MaximumPinBytes = 78, MaximumAttempts = 8;
    inline constexpr std::uint32_t  CanBeShotMask = 0x04000000;
    inline constexpr std::uintptr_t PedVtableRva = 0x4318270, PlayerVtableRva = 0x4319730;
    // Named054A selects bit26 in DWORD638, then the actual common tail
    // stores true/false according to the supplied bool. No second flag is
    // inferred from native layout or classic declaration order.
    static_assert(FlagsOffset % sizeof(std::uint32_t) == 0);
    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
    };
    inline constexpr std::array<unsigned char, 78> NamedCanBeShotBranch = {
        0xba, 0x02, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xce, 0xe8, 0x64, 0x5c, 0xfc, 0xff, 0x48, 0x63, 0x15, 0xbd, 0x5c, 0x12, 0x04,
        0x85, 0xd2, 0x78, 0x21, 0x4c, 0x8b, 0x05, 0xc2, 0xe2, 0x63, 0x04, 0x48, 0x8b, 0xca, 0x48, 0xc1, 0xf9, 0x08, 0x49, 0x8b,
        0x40, 0x08, 0x38, 0x14, 0x01, 0x75, 0x0a, 0x48, 0x69, 0xf9, 0x10, 0x0b, 0x00, 0x00, 0x49, 0x03, 0x38, 0x8b, 0x8f, 0x38,
        0x06, 0x00, 0x00, 0x8b, 0xc1, 0x0f, 0xba, 0xe8, 0x1a, 0x0f, 0xba, 0xf1, 0x1a, 0xe9, 0x8c, 0xfa, 0xff, 0xff,
    };
    inline constexpr std::array<unsigned char, 23> NamedBooleanStoreTail = {
        0x83, 0x3d, 0xf4, 0x61, 0x12, 0x04, 0x00, 0x0f, 0x45, 0xc8, 0x32, 0xc0, 0x89, 0x8f, 0x38, 0x06, 0x00, 0x00, 0xe9, 0x3e, 0x0f, 0x00, 0x00,
    };
    inline constexpr std::array<unsigned char, 10> CommonPedFlagInitialization = {
        0xc7, 0x87, 0x38, 0x06, 0x00, 0x00, 0x00, 0x00, 0x10, 0x06,
    };
    inline constexpr std::array<Pin, 3> Code = {{
        {0x10ed2bf, NamedCanBeShotBranch.data(), NamedCanBeShotBranch.size()},
        {0x10ecd99, NamedBooleanStoreTail.data(), NamedBooleanStoreTail.size()},
        {0x1219d52, CommonPedFlagInitialization.data(), CommonPedFlagInitialization.size()},
    }};
}
