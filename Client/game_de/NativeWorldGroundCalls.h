/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the actual MTA scalar ground query through the real DE entry
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <array>
namespace mta::game_de::NativeWorldGroundCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using NativeFunction = float (*)(float, float, float, bool*, std::uintptr_t*, CVector*);
    using Invoke = ClockResult (*)(NativeFunction, const CVector&, bool*, std::uintptr_t*, CVector*, float&, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t       entryRva = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::array<float, 3> position{};
        float                nativeZ = 0;
        std::uint32_t        exceptionCode = 0;
        std::uint16_t        scanBefore = 0, scanAfter = 0;
        bool                 found = false, entered = false, returned = false;
    };
    // This entry mutates native scan IDs and can recycle/initialize matrices.
    // It is a one-shot engine operation, never a readonly memory reader. Keep
    // caller output unchanged on refusal; the retained Runtime Stops after
    // any entered failure, preserving effects without rollback or replay.
    ClockResult Find(std::uintptr_t image, const CVector&, float&, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
