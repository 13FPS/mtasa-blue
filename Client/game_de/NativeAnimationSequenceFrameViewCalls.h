/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source byte arithmetic inside one current issued frame allocation
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceRowCalls.h"
namespace mta::game_de::NativeAnimationSequenceFrameViewCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeAnimationSequenceRowCalls::CheckOwner;
    // Full size_t index and uint32 stride retain source widths. Validate the
    // resulting point/span through this exact frame record/current heap,
    // without reading payload or inferring a native keyframe representation.
    // Zero stride preserves the current nonnull pointer for every index.
    ClockResult Read(std::uintptr_t image, const Storage& array, std::uint64_t generation, std::size_t rowIndex, const Storage& frames, std::size_t frameIndex,
                     std::uint32_t stride, CheckOwner, std::uintptr_t& output) noexcept;
}
