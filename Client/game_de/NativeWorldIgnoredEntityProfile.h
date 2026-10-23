/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact native pointer-equality filter consumed by supported LOS
 ****************************************************************************/
#pragma once
#include "NativeWorldGroundProfile.h"
namespace mta::game_de::NativeWorldIgnoredEntityProfile
{
    inline constexpr std::uintptr_t GlobalRva = NativeWorldGroundProfile::IgnoredEntityRva;
    inline constexpr std::size_t    Bytes = sizeof(std::uintptr_t), MaximumPinBytes = 13;
    static_assert(Bytes == 8 && GlobalRva % Bytes == 0);
    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
    };
    inline constexpr unsigned char Compare0[] = {0x48, 0x3b, 0x3d, 0x7e, 0xaa, 0x0a, 0x04, 0x66, 0x89, 0x47, 0x54, 0x74, 0x34};
    inline constexpr unsigned char Compare1[] = {0x48, 0x3b, 0x3d, 0x0d, 0xaa, 0x0a, 0x04, 0x66, 0x89, 0x47, 0x54, 0x74, 0x34};
    inline constexpr unsigned char Compare2[] = {0x48, 0x3b, 0x3d, 0x9e, 0xa9, 0x0a, 0x04, 0x66, 0x89, 0x47, 0x54, 0x74, 0x34};
    inline constexpr unsigned char Compare3[] = {0x48, 0x3b, 0x3d, 0xfe, 0xa8, 0x0a, 0x04, 0x66, 0x89, 0x47, 0x54, 0x74, 0x30};
    inline constexpr Pin           Pins[] = {
        {0x118fe53, Compare0, sizeof(Compare0)},
        {0x118fec4, Compare1, sizeof(Compare1)},
        {0x118ff33, Compare2, sizeof(Compare2)},
        {0x118ffd3, Compare3, sizeof(Compare3)},
    };
}
