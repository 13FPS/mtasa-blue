/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine selected unwind metadata for owned image decoder fixtures
 *****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeFunctionRange.h"
#include <cstddef>
#include <cstdint>

namespace NativePlayerMoveAnimHookRangeFixture
{
    // The full supported table was traversed before selecting these complete
    // three families. Copy only metadata, then let the production decoder
    // derive its own bounds; no native body or fabricated unwind rule is used.
    inline constexpr std::uint32_t                                  ExceptionDirectoryRva = 0x5876000u;
    inline constexpr mta::game_de::NativeFunctionRangeDetail::Entry Entries[] = {
        {0x103add0u, 0x103b65cu, 0x4c5ffa4u},  // Rebuild; original table index 47496
        {0x103b660u, 0x103b712u, 0x4c0a430u},  // ClothesConsumer; original table index 47497
        {0x1251be0u, 0x1251c03u, 0x4c77154u},  // Process; original table index 52775
        {0x1251c03u, 0x1251d06u, 0x4c77168u},  // Process; original table index 52776
        {0x1251d06u, 0x1251e43u, 0x4c77184u},  // Process; original table index 52777
        {0x1251e43u, 0x1251ff8u, 0x4c7719cu},  // Process; original table index 52778
        {0x1251ff8u, 0x1252061u, 0x4c771b0u},  // Process; original table index 52779
        {0x1252061u, 0x12520c0u, 0x4c771c0u},  // Process; original table index 52780
    };

    inline constexpr unsigned char Unwind_4c0a430[] = {0x01, 0x14, 0x08, 0x00, 0x14, 0x64, 0x08, 0x00, 0x14, 0x54,
                                                       0x07, 0x00, 0x14, 0x34, 0x06, 0x00, 0x14, 0x32, 0x10, 0x70};
    inline constexpr unsigned char Unwind_4c5ffa4[] = {0x19, 0x3f, 0x11, 0x00, 0x2e, 0x78, 0x1b, 0x00, 0x2a, 0x68, 0x1c, 0x00, 0x26, 0x74, 0x43,
                                                       0x00, 0x26, 0x64, 0x42, 0x00, 0x26, 0x34, 0x41, 0x00, 0x26, 0x01, 0x3a, 0x00, 0x18, 0xf0,
                                                       0x16, 0xe0, 0x14, 0xd0, 0x12, 0xc0, 0x10, 0x50, 0x00, 0x00, 0x9c, 0xfd, 0xe8, 0x03};
    inline constexpr unsigned char Unwind_4c77154[] = {0x19, 0x19, 0x03, 0x00, 0x0a, 0xa2, 0x06, 0xd0, 0x04, 0x30, 0x00, 0x00, 0x9c, 0xfd, 0xe8, 0x03};
    inline constexpr unsigned char Unwind_4c77168[] = {0x21, 0x0e, 0x06, 0x00, 0x0e, 0xe4, 0x09, 0x00, 0x08, 0x74, 0x11, 0x00, 0x04, 0x64,
                                                       0x10, 0x00, 0xe0, 0x1b, 0x25, 0x01, 0x03, 0x1c, 0x25, 0x01, 0x54, 0x71, 0xc7, 0x04};
    inline constexpr unsigned char Unwind_4c77184[] = {0x21, 0x0a, 0x04, 0x00, 0x0a, 0xc4, 0x0a, 0x00, 0x05, 0x54, 0x0f, 0x00,
                                                       0x03, 0x1c, 0x25, 0x01, 0x06, 0x1d, 0x25, 0x01, 0x68, 0x71, 0xc7, 0x04};
    inline constexpr unsigned char Unwind_4c7719c[] = {0x21, 0x05, 0x02, 0x00, 0x05, 0xf4, 0x08, 0x00, 0x06, 0x1d,
                                                       0x25, 0x01, 0x43, 0x1e, 0x25, 0x01, 0x84, 0x71, 0xc7, 0x04};
    inline constexpr unsigned char Unwind_4c771b0[] = {0x21, 0x00, 0x00, 0x00, 0x06, 0x1d, 0x25, 0x01, 0x43, 0x1e, 0x25, 0x01, 0x84, 0x71, 0xc7, 0x04};
    inline constexpr unsigned char Unwind_4c771c0[] = {0x21, 0x00, 0x02, 0x00, 0x00, 0xc4, 0x0a, 0x00, 0xe0, 0x1b,
                                                       0x25, 0x01, 0x03, 0x1c, 0x25, 0x01, 0x54, 0x71, 0xc7, 0x04};

    struct MetadataPin
    {
        std::uint32_t        rva;
        const unsigned char* bytes;
        std::size_t          size;
    };
    inline constexpr MetadataPin Pins[] = {
        {0x4c0a430u, Unwind_4c0a430, sizeof(Unwind_4c0a430)}, {0x4c5ffa4u, Unwind_4c5ffa4, sizeof(Unwind_4c5ffa4)},
        {0x4c77154u, Unwind_4c77154, sizeof(Unwind_4c77154)}, {0x4c77168u, Unwind_4c77168, sizeof(Unwind_4c77168)},
        {0x4c77184u, Unwind_4c77184, sizeof(Unwind_4c77184)}, {0x4c7719cu, Unwind_4c7719c, sizeof(Unwind_4c7719c)},
        {0x4c771b0u, Unwind_4c771b0, sizeof(Unwind_4c771b0)}, {0x4c771c0u, Unwind_4c771c0, sizeof(Unwind_4c771c0)},
    };

    struct RootExpectation
    {
        std::uint32_t                                  functionRva;
        mta::game_de::NativeFunctionRangeDetail::Entry containing;
        std::uint32_t                                  begin, end;
    };
    inline constexpr RootExpectation Expectations[] = {
        {0x1251be0u, {0x1251be0u, 0x1251c03u, 0x4c77154u}, 0x1251be0u, 0x12520c0u},  // Process
        {0x103add0u, {0x103add0u, 0x103b65cu, 0x4c5ffa4u}, 0x103add0u, 0x103b65cu},  // Rebuild
        {0x103b660u, {0x103b660u, 0x103b712u, 0x4c0a430u}, 0x103b660u, 0x103b712u},  // ClothesConsumer
    };

    // A family envelope includes genuine gaps, matching production's safe
    // quiescence rule rather than claiming that every address is code.
    inline constexpr bool Contains(const RootExpectation& expected, std::uint32_t rva) noexcept
    {
        return rva >= expected.begin && rva < expected.end;
    }
}
