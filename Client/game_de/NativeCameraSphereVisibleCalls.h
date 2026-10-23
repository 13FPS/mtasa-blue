/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One genuine native camera sphere predicate under the existing owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <cstdint>
namespace mta::game_de::NativeCameraSphereVisibleCalls
{
    using CheckOwner = ClockResult (*)() noexcept;
    using NativeFunction = bool (*)(std::uintptr_t, const CVector*, float);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t, const CVector&, float, bool&, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t camera = 0, root = 0, gameMode = 0, level = 0, world = 0;
        CVector        center;
        float          radius = 0;
        std::uint32_t  exceptionCode = 0;
        bool           value = false, entered = false, returned = false;
    };
    // Snapshot caller inputs before owner callbacks. One shared native scope
    // admits the current engine camera; only a fully acknowledged local bool
    // is published. Runtime stops on entered failure without retry/rollback.
    ClockResult Read(std::uintptr_t image, const CVector&, float, bool&, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
