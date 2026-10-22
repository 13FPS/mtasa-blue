/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine model requests through the current native streaming owner
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include "NativeModelLoadedReader.h"

namespace mta::game_de::NativeStreamingRequestCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    // The native0247 caller independently supplies false in R8D. This is an
    // additional DE argument, not a guessed reuse of MTA's priority flags.
    using NativeFunction = void (*)(std::int32_t model, std::uint32_t flags, std::uint32_t extraZero);
    using Invoke = ClockResult (*)(NativeFunction, std::uint32_t model, std::uint32_t flags, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, root = 0, provider = 0, modelInfo = 0;
        std::uint32_t  modelIndex = 0, flags = 0, exceptionCode = 0;
        unsigned char  rawFlagsBefore = 0, rawStateBefore = 0, rawFlagsAfter = 0, rawStateAfter = 0;
        bool           entered = false, returned = false;
    };

    // Ordinary DFF models only. Upgrade IDs require MTA's separate native
    // upgrade route, which is intentionally not implemented by this primitive.
    // Native request acceptance does not mean loading finished. Entered calls
    // may commit async jobs/flags before any failure; the enclosing owner must
    // stop on entered failure, without retry or streaming snapshot restoration.
    ClockResult Request(std::uintptr_t image, std::uint32_t modelIndex, std::uint32_t flags, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
