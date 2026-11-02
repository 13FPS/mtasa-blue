/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the genuine original building capacity without pool effects
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de::NativeBuildingPoolDefaultMemory
{
    using CheckOwner = ClockResult (*)() noexcept;
    // The immutable constructor argument is distinct from the current pool's
    // mutable size; this reader grants no building allocation or resize.
    ClockResult Read(std::uintptr_t image, CheckOwner owner, CheckOwner afterApplicationOwner, std::uint32_t& output) noexcept;
}
