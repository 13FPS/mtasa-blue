/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One genuine existing-slot special-model rename and requeue intent
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include <array>
namespace mta::game_de::NativeStreamingSpecialModelCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using NativeFunction = void (*)(std::int32_t, const char*, std::uint32_t);
    using Invoke = ClockResult (*)(NativeFunction, std::uint32_t, const char*, std::uint32_t, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t      entryRva = 0, root = 0, provider = 0, modelInfo = 0;
        std::uint32_t       modelIndex = 0, channel = 0, keyAfter = 0, exceptionCode = 0;
        std::array<char, 9> copiedName{};
        unsigned char       rawFlagsBefore = 0, rawStateBefore = 0, rawFlagsAfter = 0, rawStateAfter = 0;
        bool                entered = false, returned = false;
    };
    // Read only the current capability prerequisites under the same loading
    // exclusion as Request. This selects no model/name, invokes no engine
    // allocator/removal/request, and publishes no persistent readiness lease.
    // Request still freshly qualifies its complete selected-row prerequisites.
    ClockResult CheckReady(std::uintptr_t image, CheckOwner) noexcept;
    // Owns the shared loading scope itself. Runtime must Stop after an entered
    // failure and must never restore names, cache entries, flags or removed data.
    ClockResult Request(std::uintptr_t image, std::uint32_t model, const char* name, std::uint32_t channel, CheckOwner, Receipt* = nullptr,
                        Invoke = nullptr) noexcept;
}
