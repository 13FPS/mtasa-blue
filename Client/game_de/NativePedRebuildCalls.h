/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One whole genuine native rebuild under the current local player owner
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include "NativePlayerInfoMemory.h"
namespace mta::game_de::NativePedRebuildCalls
{
    using Ped = NativePedPoolReader::Value;
    using CheckOwner = NativePedPoolReader::CheckOwner;
    using NativeFunction = void (*)(std::uintptr_t nativePed);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t nativePed, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, nativePed = 0, playerData = 0, clothes = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::uintptr_t beforeRender = 0, afterRender = 0, beforeContainer = 0, afterContainer = 0, preservedHead = 0, modelInfo = 0;
        std::uint32_t  handle = 0, exceptionCode = 0;
        bool           entered = false, returned = false;
    };
    // Existing local CJ state is borrowed inside one synchronous native scope.
    // DE's proved entry consumes only the ped; classic's false parameter must
    // not become an invented second DE argument. Native effects are accepted
    // after exactly one entry, with no replay or whole-object rollback.
    ClockResult Rebuild(std::uintptr_t image, const Ped&, const NativePlayerInfoMemory&, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
