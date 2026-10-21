/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual named native StartGame action under its synchronous owner
 *****************************************************************************/
#pragma once
#include <windows.h>
#include <game/CGameClockService.h>

namespace mta::game_de::NativeStartGameCalls
{
    using NativeFunction = void (*)(void* receiver);
    using CheckOwner = ClockResult (*)(void* receiver) noexcept;
    using ClaimEntry = ClockResult (*)(void* receiver) noexcept;
    using Invoke = ClockResult (*)(NativeFunction, void*, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uint32_t nativeStateBefore = 0, nativeStateAfter = 0;
        std::uint32_t selectionBefore = 0, selectionAfter = 0, exceptionCode = 0;
        unsigned char requestBefore = 0, requestAfter = 0, userPauseAfter = 0;
        bool          entered = false, returned = false;
    };

    // This reuses the actual native initialized GameThread pair and atomic
    // scoped-GC zero-probe. It supplies no arbitrary object or frame authority.
    ClockResult CheckThread(std::uintptr_t image) noexcept;

    // CheckOwner must admit only the opted-in normal post-original native
    // GTAGameMode Tick, matching this exact receiver/generation/depth. Observer
    // V1 and timer/producer/renderer scopes do not qualify. It is scope-only:
    // legitimate native progress may retire the previous menu/provider graph.
    // ClaimEntry freshly qualifies the actual enclosing owner/world/providers,
    // configured SimpleFade class and UI prerequisites, checks that same scope,
    // then atomically claims one attempt per generation immediately before the
    // native entry. A pre-entry refusal must not consume an attempt.
    // No pointer is retained beyond this borrowed synchronous call. Once entered,
    // a failure or exception must never retry or restore a previous world.
    ClockResult Apply(std::uintptr_t image, void* receiver, CheckOwner checkOwner, ClaimEntry claimEntry, Receipt* receipt = nullptr,
                      Invoke invoke = nullptr) noexcept;
    // Invoke is a typed ordinary-host test seam; production omits it. The native
    // action returns a request, not a synchronous loaded-world success signal.
}
