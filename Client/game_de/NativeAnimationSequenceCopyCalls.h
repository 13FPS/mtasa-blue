/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Whole private sequence property copy without native child retirement
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceRowCalls.h"
#include <array>
namespace mta::game_de::NativeAnimationSequenceCopyCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeAnimationSequenceRowCalls::CheckOwner;
    struct Record
    {
        std::array<unsigned char, 80> bytes{};
    };
    using Write = ClockResult (*)(std::uintptr_t target, const Record& source, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t sourceRow = 0, targetRow = 0;
        // The actual raw-copy intent's qualified physical fields let the
        // controller transfer accepted format metadata without trusting a
        // separate pre-read that could differ before the copied snapshot.
        NativeAnimationSequenceRowCalls::Value sourceValue{};
        std::uint32_t                          exceptionCode = 0;
        bool                                   entered = false, rawCopied = false, verified = false, selfCopy = false;
    };
    // Both records must be genuine current LiveUnpublished arrays, with full
    // row/cookie/range/heap admission. No child allocation is freed, adopted
    // or unpinned. The controller copies logical state under its same lock.
    ClockResult Copy(std::uintptr_t image, Storage& target, std::size_t targetIndex, const Storage& source, std::size_t sourceIndex, std::uint64_t generation,
                     CheckOwner, Receipt* = nullptr, Write = nullptr) noexcept;
    // rawCopied means the full80-byte CPU write returned successfully; it
    // remains true on later owner/read failure. A qualified self-copy is a
    // verified no-store (selfCopy=true,entered=false,rawCopied=false).
}
