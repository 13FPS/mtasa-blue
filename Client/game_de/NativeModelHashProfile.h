/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact saved native model hash field consumer, independent of policy
 ****************************************************************************/
#pragma once
#include "NativeModelBankProfile.h"
namespace mta::game_de::NativeModelHashProfile
{
    inline constexpr std::size_t HashOffset = 8, PrefixBytes = HashOffset + sizeof(std::uint32_t), MaximumPinBytes = 17;
    // Whole FEB030 proves both nonnull-model DWORD+8 comparisons. Its name
    // cursor policy is not the addressed 4C59B0 hash-overload policy and is
    // never executed or copied by this read-only ascending bank lookup.
    inline constexpr std::array<unsigned char, 16> ForwardHashConsumer{0x48, 0x8b, 0x01, 0x48, 0x85, 0xc0, 0x74, 0x06,
                                                                       0x44, 0x39, 0x48, 0x08, 0x74, 0x51, 0xff, 0xc2};
    inline constexpr std::array<unsigned char, 17> ReverseHashConsumer{0x4a, 0x8b, 0x04, 0xc1, 0x4a, 0x8d, 0x0c, 0xc1, 0x48,
                                                                       0x85, 0xc0, 0x74, 0x06, 0x44, 0x39, 0x48, 0x08};
    using Pin = NativeModelBankProfile::Pin;
    inline constexpr std::array<Pin, 2> Code{
        {{0xfeb1d8, ForwardHashConsumer.data(), ForwardHashConsumer.size()}, {0xfeb217, ReverseHashConsumer.data(), ReverseHashConsumer.size()}}};
}
