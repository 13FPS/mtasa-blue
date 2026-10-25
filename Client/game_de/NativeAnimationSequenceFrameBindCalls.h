/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Restricted prepublication binding of an existing encoded frame buffer
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceCopyCalls.h"
namespace mta::game_de::NativeAnimationSequenceFrameBindCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeAnimationSequenceRowCalls::CheckOwner;
    using Value = NativeAnimationSequenceRowCalls::Value;
    using Record = NativeAnimationSequenceCopyCalls::Record;
    using Write = ClockResult (*)(std::uintptr_t row, const Value& target, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        Record         before{}, after{};
        std::uintptr_t row = 0, frames = 0;
        std::size_t    count = 0, bytes = 0;
        std::uint32_t  exceptionCode = 0;
        bool           entered = false, accepted = false, verified = false;
    };
    // This private builder is not the general SDK SetKeyFrames operation.
    // Require an issued fresh row and original nonnull frame allocation,
    // positive full count and a checked compressed10/16-byte buffer span.
    // Only after those checks does the stored count narrow to native WORD.
    ClockResult AttachCompressed(std::uintptr_t image, Storage& array, std::uint64_t generation, std::size_t rowIndex, const Storage& frames, std::size_t count,
                                 bool root, CheckOwner, Receipt* = nullptr, Write = nullptr) noexcept;
    // An explicit fresh native20/32-byte itinerary, independently proved by
    // the saved loader branches. This does not resolve general SetKeyFrames
    // replacement/false-compression policy or convert compressed payload.
    ClockResult AttachUncompressed(std::uintptr_t image, Storage& array, std::uint64_t generation, std::size_t rowIndex, const Storage& frames,
                                   std::size_t count, bool root, CheckOwner, Receipt* = nullptr, Write = nullptr) noexcept;
    // Preserve name/bone/padding and the optional selector10. No pointer-
    // implies-shared08 rule, payload initialization, allocation/free, journal
    // erasure, native publication, replay or generic unpin is introduced.
    // accepted acknowledges all requested CPU stores before postflight loss.
}
