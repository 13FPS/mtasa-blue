/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe native camera scalar bits through the existing owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de::NativeCameraDataMemory
{
    using CheckOwner = ClockResult (*)() noexcept;
    // These owned scalar observations grant no camera row, writable vector,
    // native lifetime lease or setter. Refusal leaves the caller unchanged.
    ClockResult ReadActive(std::uintptr_t image, CheckOwner owner, std::uint8_t& output) noexcept;
    ClockResult ReadFov(std::uintptr_t image, std::uint32_t logicalRow, CheckOwner owner, std::uint32_t& rawBits) noexcept;
}
