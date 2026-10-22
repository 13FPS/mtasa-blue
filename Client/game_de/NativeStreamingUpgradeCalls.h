/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine paired-upgrade request through the existing callback owner
 ****************************************************************************/
#pragma once
#include "NativeStreamingUpgradeReader.h"
namespace mta::game_de::NativeStreamingUpgradeCalls
{
    using CheckOwner = NativeStreamingUpgradeReader::CheckOwner;
    using NativeFunction = void (*)(std::int32_t, std::uint32_t);
    using Invoke = ClockResult (*)(NativeFunction, std::uint32_t, std::uint32_t, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, root = 0, provider = 0;
        std::uint32_t  modelIndex = 0, flags = 0, exceptionCode = 0;
        std::int32_t   companionIndex = -1;
        unsigned char  selectedStateBefore = 0, selectedStateAfter = 0, companionStateBefore = 0, companionStateAfter = 0;
        bool           entered = false, returned = false;
    };
    // Preflight both the selected model and any actual companion. Invoke the
    // genuine native upgrade wrapper once; it owns the order of its requests.
    // Any entered failure must stop the enclosing owner, without retry, flag
    // restoration, companion replay or a promise that asynchronous loading ended.
    ClockResult Request(std::uintptr_t, std::uint32_t, std::uint32_t, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
