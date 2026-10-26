/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-ordered collision-only completion in the existing loading owner
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <array>
#include <cstdint>

namespace mta::game_de::NativeStreamingCollisionSceneCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using StoreFunction = void (*)(CVector*, bool);
    using LoadAllFunction = void (*)();
    using InvokeStore = ClockResult (*)(StoreFunction, CVector*, bool, std::uint32_t& exception) noexcept;
    using InvokeLoadAll = ClockResult (*)(LoadAllFunction, std::uint32_t& exception) noexcept;

    struct StageReceipt
    {
        std::uintptr_t entryRva = 0;
        std::uint32_t  exceptionCode = 0;
        bool           entered = false, returned = false;
    };
    struct Receipt
    {
        std::uintptr_t       root = 0, gameMode = 0, level = 0, world = 0;
        std::array<float, 3> position{};
        StageReceipt         collision{}, ipl{}, loadAll{};
        unsigned char        modeBefore = 0, modeAfter = 0;
        bool                 entered = false, returned = false, verified = false;
    };

    // The real MTA collision-only operation runs COL, IPL, then blocking-all
    // loading. One shared completion gate keeps that sequence in the existing
    // owner; genuine engine stages retain their own tree/pool traversal.
    // entered means a stage may have effects, returned means all three calls
    // returned, and only verified/Ok acknowledges current owner bookends.
    // An entered failure requires Stop without replay or restoring old flags.
    ClockResult Load(std::uintptr_t image, const CVector* position, CheckOwner, Receipt* = nullptr, InvokeStore = nullptr, InvokeLoadAll = nullptr) noexcept;
}
