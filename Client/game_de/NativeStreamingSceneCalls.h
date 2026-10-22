/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine scene completion inside the existing native loading owner
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <array>
#include <cstdint>
namespace mta::game_de::NativeStreamingSceneCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using NativeFunction = void (*)(const CVector*);
    using Invoke = ClockResult (*)(NativeFunction, const CVector*, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t       entryRva = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::uintptr_t       persistentLevel = 0, settings = 0, settingsClass = 0, target = 0;
        std::array<float, 3> position{};
        std::uint32_t        exceptionCode = 0;
        unsigned char        modeBefore = 0, modeAfter = 0;
        bool                 entered = false, returned = false;
    };
    // Genuine named03CB entry owns its internal request/LoadAll/scene effects.
    // This shares the existing completion gate and borrowed current world;
    // neither a new loading host nor a collision-only substitute is created.
    // Any entered failure is retained-owner Stop/no replay; no scene, flag,
    // queue, vector or buffer snapshot is restored after native execution.
    ClockResult Load(std::uintptr_t image, const CVector* position, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
