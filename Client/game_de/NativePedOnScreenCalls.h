/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One admitted genuine-native IsOnScreen operation for a registered ped
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>
namespace mta::game_de::NativePedOnScreenCalls
{
    using Ped = NativePedPoolReader::Value;
    using CheckOwner = NativePedPoolReader::CheckOwner;
    using RadiusFunction = float (*)(std::uintptr_t);
    using CenterFunction = CVector* (*)(std::uintptr_t, CVector*);
    using ScreenFunction = bool (*)(std::uintptr_t, const CVector*, float);
    using RadiusInvoke = ClockResult (*)(RadiusFunction, std::uintptr_t, float&, std::uint32_t&) noexcept;
    using CenterInvoke = ClockResult (*)(CenterFunction, std::uintptr_t, CVector&, CVector*&, std::uint32_t&) noexcept;
    using ScreenInvoke = ClockResult (*)(ScreenFunction, std::uintptr_t, const CVector&, float, bool&, std::uint32_t&) noexcept;
    struct Invoke
    {
        RadiusInvoke radius = nullptr;
        CenterInvoke center = nullptr;
        ScreenInvoke screen = nullptr;
    };
    struct Receipt
    {
        std::uintptr_t nativePed = 0, camera = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::uint32_t  handle = 0, exceptionCode = 0;
        std::uint16_t  modelIndex = 0;
        CVector        center;
        float          radius = 0;
        unsigned char  completedMask = 0;
        bool           value = false, entered = false, returned = false;
    };
    // The native radius helper may resolve assets. All three helpers execute
    // once in one shared scope. Any entered failure stops the existing owner
    // without replay or rollback; only a complete current result updates output.
    // Invoke substitutes final typed native entries, never admission or ordering.
    ClockResult Read(std::uintptr_t image, const Ped&, bool&, CheckOwner, Receipt* = nullptr, const Invoke* = nullptr) noexcept;
}
