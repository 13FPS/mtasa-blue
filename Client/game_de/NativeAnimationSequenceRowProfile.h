/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native evidence for the exact signed bone sentinel/physical selector
 ****************************************************************************/
#pragma once
#include "NativeCustomAnimSequenceStorageProfile.h"
#include <array>
namespace mta::game_de::NativeAnimationSequenceRowProfile
{
    // The actual loader compares full EAX with -1 before OR WORD42,10 and
    // storing low AX at40. This independently proves physical DE semantics;
    // the classic logical union helper alone supplies no field-layout proof.
    inline constexpr std::array<unsigned char, 16> BonePolicy{{0x83, 0xf8, 0xff, 0x74, 0x0b, 0x66, 0x41, 0x83, 0x4e, 0x42, 0x10, 0x66, 0x41, 0x89, 0x46, 0x40}};
    using Pin = NativeCustomAnimSequenceStorageProfile::Pin;
    inline constexpr std::array<Pin, 1> Code{{{0xff2f6a, BonePolicy.data(), BonePolicy.size()}}};
    inline constexpr std::size_t        MaximumPinBytes = BonePolicy.size();
}
