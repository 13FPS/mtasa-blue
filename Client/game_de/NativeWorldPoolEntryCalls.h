/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Whole native entry invocation under the world-pool controller
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de::NativeWorldPoolEntryCalls
{
    enum class Operation : unsigned char
    {
        RemoveMatrix,
        AllocateStaticMatrix
    };
    using Entry = void (*)(void*);
    using Invoker = ClockResult (*)(Entry, void*, std::uint32_t&) noexcept;
    struct Receipt
    {
        bool          entered = false, returned = false;
        std::uint32_t exception = 0;
        ClockResult   result = ClockResult::NotInitialized;
    };
    // This is an internal final-entry thunk, not an independent owner/service.
    // The consuming controller must admit exact typed pool generation + UE /
    // matrix/sector inputs, run its last owner check, seal raw observations and
    // record retained accepted intent BEFORE calling Invoke. It must retain
    // unknown after an entered exception or a lost post-return acknowledgement.
    ClockResult Invoke(Operation, std::uintptr_t image, std::uintptr_t nativeEntity, Receipt&, Invoker = nullptr) noexcept;
}
