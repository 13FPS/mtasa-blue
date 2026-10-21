/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read-only qualification of the native pad activity timestamp
 *****************************************************************************/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativePadActivityProfile
{
    inline constexpr std::uintptr_t ImageSize = 0x5ca4400, PadArrayRva = 0x53e8810, NativeTimerRva = 0x523a458;
    inline constexpr std::size_t    PadStride = 0x188, PadCount = 2, FieldOffset = 0x164, FieldSize = 4, PadIdentityOffset = 0x98;
    // These exact native initializer and activity producer/consumer instructions
    // qualify one DWORD field. They grant no call, hook or write authority.
    inline constexpr std::array<unsigned char, 3>  IdentityBytes = {0x89, 0x48, 0x5c};
    inline constexpr std::array<unsigned char, 7>  StrideBytes = {0x48, 0x8d, 0x80, 0x88, 0x01, 0x00, 0x00};
    inline constexpr std::array<unsigned char, 3>  CountBytes = {0x83, 0xf9, 0x02};
    inline constexpr std::array<unsigned char, 7>  FirstPadBytes = {0x48, 0x8d, 0x0d, 0xd7, 0x37, 0x29, 0x04};
    inline constexpr std::array<unsigned char, 7>  SecondPadBytes = {0x48, 0x8d, 0x0d, 0x43, 0x39, 0x29, 0x04};
    inline constexpr std::array<unsigned char, 9>  ActivityBytes = {0xe8, 0xbb, 0xef, 0xff, 0xff, 0x84, 0xc0, 0x74, 0x0d};
    inline constexpr std::array<unsigned char, 6>  TimerBytes = {0x8b, 0x05, 0xd9, 0x4f, 0x0e, 0x04};
    inline constexpr std::array<unsigned char, 7>  StoreBytes = {0x41, 0x89, 0x86, 0x64, 0x01, 0x00, 0x00};
    inline constexpr std::array<unsigned char, 12> IndependentStoreBytes = {0x8b, 0x05, 0x4a, 0x54, 0x0e, 0x04, 0x89, 0x05, 0x60, 0x39, 0x29, 0x04};
    inline constexpr std::array<unsigned char, 6>  ElapsedBytes = {0x2b, 0x05, 0xa4, 0x10, 0x34, 0x04};
    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
    };
    inline constexpr std::array<Pin, 10> Pins = {{{0x11543d0, IdentityBytes.data(), IdentityBytes.size()},
                                                  {0x11543d3, StrideBytes.data(), StrideBytes.size()},
                                                  {0x1154415, CountBytes.data(), CountBytes.size()},
                                                  {0x1155032, FirstPadBytes.data(), FirstPadBytes.size()},
                                                  {0x115504e, SecondPadBytes.data(), SecondPadBytes.size()},
                                                  {0x1155470, ActivityBytes.data(), ActivityBytes.size()},
                                                  {0x1155479, TimerBytes.data(), TimerBytes.size()},
                                                  {0x115547f, StoreBytes.data(), StoreBytes.size()},
                                                  {0x1155008, IndependentStoreBytes.data(), IndependentStoreBytes.size()},
                                                  {0x10a7a52, ElapsedBytes.data(), ElapsedBytes.size()}}};
    struct Binding
    {
        std::uintptr_t image = 0, pads = 0;
    };
    // Qualify uses the established exact executable digest. Both functions
    // require constructed physical pad IDs0/1 and preserve output on refusal.
    // Revalidate checks the same live image/pins/pages without another disk hash.
    // Neither depends on updater entry bytes: an independently owned updater
    // hook must not accidentally invalidate unrelated timestamp qualification.
    bool Qualify(Binding& output) noexcept;
    bool Revalidate(const Binding& binding) noexcept;
}
