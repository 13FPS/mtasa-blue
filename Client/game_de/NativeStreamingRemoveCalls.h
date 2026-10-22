/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine immediate native streaming removal on the existing owner
 ****************************************************************************/
#pragma once
#include "NativeModelLoadedReader.h"
namespace mta::game_de::NativeStreamingRemoveCalls
{
    using CheckOwner = NativeModelLoadedReader::CheckOwner;
    using NativeFunction = void (*)(std::int32_t modelIndex);
    using Invoke = ClockResult (*)(NativeFunction, std::uint32_t modelIndex, std::uint32_t& exceptionCode) noexcept;
    using CheckBeforeEntry = ClockResult (*)(void*) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, root = 0, provider = 0, modelInfo = 0, bundle = 0, assetMap = 0;
        std::uint32_t  modelIndex = 0, exceptionCode = 0;
        unsigned char  rawFlagsBefore = 0, rawStateBefore = 0, rawFlagsAfter = 0, rawStateAfter = 0;
        bool           entered = false, returned = false;
    };
    // Direct CStreaming intent: do not decrement the separate model counter or
    // assume zero/custom-clump/custom-col policy on behalf of CModelInfo. Any
    // entered failure can retain partial native effects and must retire its
    // callback owner without a replay, rollback or synthetic unloaded state.
    ClockResult Remove(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
    // Synchronous internal continuation for an already accepted final model
    // decrement. The stack-owned guard reacquires its zero/key/current owner
    // after removal admission, immediately before the genuine native entry.
    ClockResult RemoveGuarded(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner, CheckBeforeEntry, void*, Receipt* = nullptr,
                              Invoke = nullptr) noexcept;
}
