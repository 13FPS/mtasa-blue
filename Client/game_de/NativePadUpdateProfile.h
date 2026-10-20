/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read-only identity of the qualified native global pad updater
 *****************************************************************************/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativePadUpdateProfile
{
    inline constexpr std::uintptr_t ImageSize = 0x5ca4400, UpdaterRva = 0x1154c00, EndRva = 0x1155094;
    inline constexpr std::uintptr_t PadArrayRva = 0x53e8810;
    inline constexpr std::size_t    PadStride = 0x188, PadCount = 2;
    // Exact-image disk evidence: updater-qualification.json plus the prior pad
    // array/stride proof. These bytes grant no hook, call or mutable-pad scope.
    inline constexpr std::array<unsigned char, 32> EntryBytes = {0x48, 0x83, 0xec, 0x28, 0x65, 0x48, 0x8b, 0x04, 0x25, 0x58, 0x00,
                                                                 0x00, 0x00, 0xba, 0x68, 0x02, 0x00, 0x00, 0x48, 0x89, 0x5c, 0x24,
                                                                 0x30, 0x48, 0x89, 0x74, 0x24, 0x38, 0x48, 0x8b, 0x08, 0x8b};
    inline constexpr std::array<unsigned char, 4>  PinBytes0 = {0x48, 0x83, 0xec, 0x28};                          // global_updater_entry
    inline constexpr std::array<unsigned char, 7>  PinBytes1 = {0x48, 0x8d, 0x0d, 0xd7, 0x37, 0x29, 0x04};        // global_pad0_dispatch
    inline constexpr std::array<unsigned char, 5>  PinBytes2 = {0xe8, 0x17, 0x03, 0x00, 0x00};                    // global_pad0_call
    inline constexpr std::array<unsigned char, 5>  PinBytes3 = {0xe9, 0x02, 0x03, 0x00, 0x00};                    // global_pad1_tail
    inline constexpr std::array<unsigned char, 8>  PinBytes4 = {0x4c, 0x8b, 0xdc, 0x55, 0x41, 0x55, 0x41, 0x56};  // per_pad_entry
    inline constexpr std::array<unsigned char, 4>  PinBytes5 = {0x45, 0x89, 0x59, 0xd4};                          // constructor_no_vptr_dword_zero
    inline constexpr std::array<unsigned char, 6>  PinBytes6 = {0x33, 0xc0, 0x48, 0x89, 0x41, 0x38};              // native_clear_entry
    inline constexpr std::array<unsigned char, 5>  PinBytes7 = {0xe8, 0x49, 0x35, 0xac, 0xff};                    // prefix_mouse_update_call
    inline constexpr std::array<unsigned char, 8>  PinBytes8 = {0xf3, 0x0f, 0x10, 0x1d, 0x5b, 0x39, 0x29, 0x04};  // prefix_x_float_read
    inline constexpr std::array<unsigned char, 8>  PinBytes9 = {0xf3, 0x0f, 0x10, 0x15, 0x54, 0x39, 0x29, 0x04};  // prefix_y_float_read
    inline constexpr std::array<unsigned char, 5>  PinBytes10 = {0xe8, 0xad, 0xd3, 0x00, 0x00};                   // known_direct_call_1
    inline constexpr std::array<unsigned char, 5>  PinBytes11 = {0xe8, 0x17, 0x1f, 0x00, 0x00};                   // known_direct_call_2
    inline constexpr std::array<unsigned char, 5>  PinBytes12 = {0xe8, 0x36, 0xad, 0xf4, 0xff};                   // known_direct_call_3
    inline constexpr std::array<unsigned char, 5>  PinBytes13 = {0xe8, 0x06, 0xaa, 0xf4, 0xff};                   // known_direct_call_4
    inline constexpr std::array<unsigned char, 5>  PinBytes14 = {0xe9, 0xae, 0xba, 0xeb, 0xff};                   // known_tail_call_5
    inline constexpr std::array<unsigned char, 5>  PinBytes15 = {0xf3, 0x0f, 0x11, 0x73, 0x2c};                   // prefix_float_at_2c
    inline constexpr std::array<unsigned char, 5>  PinBytes16 = {0xf3, 0x0f, 0x11, 0x53, 0x28};                   // prefix_float_at_28
    inline constexpr std::array<unsigned char, 5>  PinBytes17 = {0xf2, 0x0f, 0x11, 0x43, 0x20};                   // prefix_float_pair_at_20
    inline constexpr std::array<unsigned char, 5>  PinBytes18 = {0xf3, 0x0f, 0x10, 0x43, 0x24};                   // prefix_float_at_24_read
    inline constexpr std::array<unsigned char, 6>  PinBytes19 = {0xf3, 0x44, 0x0f, 0x11, 0x43, 0x20};             // prefix_float_at_20_scaled
    inline constexpr std::array<unsigned char, 5>  PinBytes20 = {0xf3, 0x0f, 0x11, 0x43, 0x24};                   // prefix_float_at_24_scaled
    inline constexpr std::array<unsigned char, 4>  PinBytes21 = {0xf3, 0x0f, 0x11, 0x03};                         // prefix_float_at_zero_decay
    inline constexpr std::array<unsigned char, 4>  PinBytes22 = {0xf3, 0x0f, 0x11, 0x03};                         // prefix_float_at_zero_increment
    inline constexpr std::array<unsigned char, 3>  PinBytes23 = {0x89, 0x48, 0x5c};                               // native_pad_identity
    inline constexpr std::array<unsigned char, 7>  PinBytes24 = {0x48, 0x8d, 0x0d, 0x43, 0x39, 0x29, 0x04};       // second_pad_native_dispatch
    inline constexpr std::array<unsigned char, 7>  PinBytes25 = {0x48, 0x8d, 0x80, 0x88, 0x01, 0x00, 0x00};       // initializer_independent_pad_stride
    inline constexpr std::array<unsigned char, 3>  PinBytes26 = {0x83, 0xf9, 0x02};                               // initializer_two_pad_loop

    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
    };
    inline constexpr std::array<Pin, 27> Pins = {{{0x1154c00, PinBytes0.data(), PinBytes0.size()},   {0x1155032, PinBytes1.data(), PinBytes1.size()},
                                                  {0x1155044, PinBytes2.data(), PinBytes2.size()},   {0x1155059, PinBytes3.data(), PinBytes3.size()},
                                                  {0x1155360, PinBytes4.data(), PinBytes4.size()},   {0x648763, PinBytes5.data(), PinBytes5.size()},
                                                  {0x11545e0, PinBytes6.data(), PinBytes6.size()},   {0x1154ec2, PinBytes7.data(), PinBytes7.size()},
                                                  {0x1154ecd, PinBytes8.data(), PinBytes8.size()},   {0x1154ed8, PinBytes9.data(), PinBytes9.size()},
                                                  {0x114784e, PinBytes10.data(), PinBytes10.size()}, {0x1152ce4, PinBytes11.data(), PinBytes11.size()},
                                                  {0x1209ec5, PinBytes12.data(), PinBytes12.size()}, {0x120a1f5, PinBytes13.data(), PinBytes13.size()},
                                                  {0x129914d, PinBytes14.data(), PinBytes14.size()}, {0xc18748, PinBytes15.data(), PinBytes15.size()},
                                                  {0xc18761, PinBytes16.data(), PinBytes16.size()},  {0xc187b6, PinBytes17.data(), PinBytes17.size()},
                                                  {0xc187c8, PinBytes18.data(), PinBytes18.size()},  {0xc1884e, PinBytes19.data(), PinBytes19.size()},
                                                  {0xc18861, PinBytes20.data(), PinBytes20.size()},  {0xc18954, PinBytes21.data(), PinBytes21.size()},
                                                  {0xc18a24, PinBytes22.data(), PinBytes22.size()},  {0x11543d0, PinBytes23.data(), PinBytes23.size()},
                                                  {0x115504e, PinBytes24.data(), PinBytes24.size()}, {0x11543d3, PinBytes25.data(), PinBytes25.size()},
                                                  {0x1154415, PinBytes26.data(), PinBytes26.size()}}};

    struct RuntimeEntry
    {
        std::uint32_t rva, begin, end, unwind;
    };
    // This one function has five chained x64 unwind fragments. Its first
    // RUNTIME_FUNCTION ends after 18 bytes; EndRva spans the complete family.
    inline constexpr std::array<RuntimeEntry, 5> RuntimeEntries = {{{0x5907ae8, 0x1154c00, 0x1154c12, 0x4c090a0},
                                                                    {0x5907af4, 0x1154c12, 0x1154dfd, 0x4c6aed0},
                                                                    {0x5907b00, 0x1154dfd, 0x1154f12, 0x4c6aee8},
                                                                    {0x5907b0c, 0x1154f12, 0x115505e, 0x4c6af00},
                                                                    {0x5907b18, 0x115505e, 0x1155094, 0x4c6af10}}};

    struct Binding
    {
        std::uintptr_t image = 0, updater = 0, pads = 0;
    };

    // Qualify performs the existing exact-executable hash guard. Revalidation
    // checks the same live image, all profiles and unwind identities without
    // hashing 89 MB on a callback. Both preserve caller storage on refusal.
    bool Qualify(Binding& output) noexcept;
    // false omits ONLY the 32-byte entry-prefix comparison. An active hook owner
    // must independently compare its exact captured entry bytes before using
    // this mode; it is not permission to accept arbitrary entry modifications.
    bool Revalidate(const Binding& binding, bool requireUnhookedEntry = true) noexcept;
}
