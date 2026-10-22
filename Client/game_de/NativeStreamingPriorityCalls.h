/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete qualified priority work through genuine native handles
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de::NativeStreamingPriorityCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using WaitFunction = std::int32_t (*)(std::uintptr_t handle);
    using DrainFunction = void (*)(std::uintptr_t root);
    using WaitInvoke = ClockResult (*)(WaitFunction, std::uintptr_t handle, std::int32_t& nativeResult, std::uint32_t& exception) noexcept;
    using DrainInvoke = ClockResult (*)(DrainFunction, std::uintptr_t root, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t root = 0, gameMode = 0, level = 0, world = 0, waitRva = 0, drainRva = 0;
        std::uint32_t  selectedBefore = 0, selectedAfter = 0, waits = 0, drains = 0, lastModel = 0xffffffffu, exceptionCode = 0;
        std::int32_t   nativeWaitResult = 0;
        bool           entered = false, returned = false;
    };
    // Every selected current flag10 row must have a genuine rooted native
    // strong handle before any effects. Missing IMG/state2 work refuses;
    // true never substitutes the engine's global blocking-all operation.
    // Native callbacks may retire handles: receipt addresses grant no lease.
    ClockResult Load(std::uintptr_t image, CheckOwner, Receipt* = nullptr, WaitInvoke = nullptr, DrainInvoke = nullptr) noexcept;
}
