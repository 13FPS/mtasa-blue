/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded raw names on current issued unpublished sequence rows
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceCopyCalls.h"
#include "NativeAnimationSequenceName.h"
namespace mta::game_de::NativeAnimationSequenceNameCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeAnimationSequenceRowCalls::CheckOwner;
    using Name = NativeAnimationSequenceName;
    using Record = NativeAnimationSequenceCopyCalls::Record;
    using Write = ClockResult (*)(std::uintptr_t row, const Name&, std::size_t terminatedBytes, bool projectBone, std::uint16_t boneTag,
                                  std::uint32_t& exception) noexcept;
    struct Receipt
    {
        Record         before{}, after{};
        std::uintptr_t row = 0;
        std::uint32_t  exceptionCode = 0;
        bool           entered = false, accepted = false, rawWritten = false, verified = false;
    };
    // Copy the entire name/hash intent before callbacks. A NUL must exist
    // inside name64; bytes including high bytes remain raw and are never
    // truncated or normalized. Only length+1 name bytes are written.
    ClockResult Set(std::uintptr_t image, Storage&, std::uint64_t generation, std::size_t index, const Name&, std::uint32_t logicalHash, CheckOwner,
                    Receipt* = nullptr, Write = nullptr) noexcept;
    // If physical selector10 is active, project the new logical hash LOW16
    // into bone40 while preserving the actual flags/tail/frame/name padding.
    // accepted/rawWritten acknowledge all requested stores before postflight;
    // entered failure keeps storage debt and never retries or rolls back.
}
