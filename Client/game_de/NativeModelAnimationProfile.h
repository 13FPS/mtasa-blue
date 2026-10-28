/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact animation-file leaf and native EAX dependency consumer
 ****************************************************************************/
#pragma once
#include "NativeModelBankProfile.h"
namespace mta::game_de::NativeModelAnimationProfile
{
    inline constexpr std::uintptr_t GetterRva = 0x11f5680, ObservedVtableRva = 0x43170a0, GetterSlot = 0x78, ConsumerRva = 0x12888ac;
    inline constexpr std::size_t    ValueOffset = 0x6c0, ReceiverBytes = ValueOffset + sizeof(std::uint64_t);
    // The genuine ped table uses a different inherited clump getter. Keep
    // its fixed layout separate: a small ped must not require vehicle6c0.
    inline constexpr std::uintptr_t PedGetterRva = 0x11f5520, PedObservedVtableRva = 0x4317c18;
    inline constexpr std::uintptr_t PedTypeGetterRva = 0x11f5530, PedTypeSlot = 0x20;
    inline constexpr std::size_t    PedValueOffset = 0x68, PedReceiverBytes = 0x98;
    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
    };
    // The actual slot returns a QWORD. RequestModel tests/uses EAX, so the
    // observation must bookend the whole native sample before returning low32.
    inline constexpr std::array<unsigned char, 8> Getter = {
        0x48, 0x8b, 0x81, 0xc0, 0x06, 0x00, 0x00, 0xc3,
    };
    inline constexpr std::array<unsigned char, 46> Consumer = {
        0x4f, 0x8b, 0xa4, 0xfd, 0xe0, 0x2f, 0x24, 0x05, 0x4d, 0x85, 0xe4, 0x74, 0x21, 0x49, 0x8b, 0x04, 0x24, 0x49, 0x8b, 0xcc, 0xff, 0x50, 0x78,
        0x83, 0xf8, 0xff, 0x74, 0x12, 0x45, 0x33, 0xc0, 0x8d, 0x88, 0xe7, 0x63, 0x00, 0x00, 0x41, 0x8d, 0x50, 0x08, 0xe8, 0x86, 0xfc, 0xff, 0xff,
    };
    inline constexpr std::array<Pin, 2> Code = {{{GetterRva, Getter.data(), Getter.size()}, {ConsumerRva, Consumer.data(), Consumer.size()}}};
    inline constexpr std::size_t        MaximumPinBytes = Consumer.size();
    // Native RequestModel consumes this getter's EAX too. The raw type7
    // leaf corroborates the observed ped table, not a guessed SDK enum map.
    inline constexpr std::array<unsigned char, 5> PedGetter = {0x48, 0x8b, 0x41, 0x68, 0xc3};
    inline constexpr std::array<unsigned char, 3> PedTypeGetter = {0xb0, 0x07, 0xc3};
    inline constexpr std::array<Pin, 2>           PedCode = {
        {{PedGetterRva, PedGetter.data(), PedGetter.size()}, {PedTypeGetterRva, PedTypeGetter.data(), PedTypeGetter.size()}}};
}
