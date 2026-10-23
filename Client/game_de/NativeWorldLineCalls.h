/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's seven actual LOS-clear flags through genuine DE code
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <array>
struct SLineOfSightFlags;
namespace mta::game_de::NativeWorldLineCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    // Exact CWorldSA order: buildings, vehicles, peds, objects, dummies,
    // seeThrough, ignoreCamera. ShootThrough and CarTires have no argument.
    using Flags = std::array<bool, 7>;
    using NativeFunction = bool (*)(const CVector*, const CVector*, bool, bool, bool, bool, bool, bool, bool);
    using Invoke = ClockResult (*)(NativeFunction, const CVector&, const CVector&, const Flags&, bool&, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t       entryRva = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::array<float, 3> start{}, end{};
        Flags                flags{};
        std::uint32_t        exceptionCode = 0;
        std::uint16_t        scanBefore = 0, scanAfter = 0;
        bool                 nativeClear = false, entered = false, returned = false;
    };
    // Native scan/matrix effects make this a genuine one-shot operation. A
    // false clear result is successful native blocking; an owner refusal must
    // preserve caller output and retire the Runtime after any entered failure.
    ClockResult Clear(std::uintptr_t image, const CVector&, const CVector&, const SLineOfSightFlags&, bool&, CheckOwner, Receipt* = nullptr,
                      Invoke = nullptr) noexcept;
}
