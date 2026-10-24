/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One genuine existing local-player clothes intent without rebuilding
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include "NativePlayerInfoMemory.h"
#include <cstdint>
namespace mta::game_de::NativePedClothesCalls
{
    using Ped = NativePedPoolReader::Value;
    using CheckOwner = NativePedPoolReader::CheckOwner;
    using NativeFunction = void (*)(std::uintptr_t, const char*, const char*, std::int32_t);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t, const char*, const char*, std::uint8_t, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, nativePed = 0, playerData = 0, clothes = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::uint32_t  handle = 0, part = 0, exceptionCode = 0;
        std::uint16_t  textureBytes = 0, modelBytes = 0;
        bool           nullClothes = false, entered = false, returned = false;
    };
    // Names are read/copied only after the current clothes pointer is known
    // nonnull, preserving the classic no-op before caller-string evaluation.
    // The supplied reader proves the existing local player, not remote data
    // allocation or a finished CPlayerPed factory. Entered failure is final.
    ClockResult Set(std::uintptr_t, const Ped&, const NativePlayerInfoMemory&, const char* texture, const char* model, std::uint8_t texturePart, CheckOwner,
                    Receipt* = nullptr, Invoke = nullptr) noexcept;
}
