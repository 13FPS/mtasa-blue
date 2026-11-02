/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe raw main-camera target bits without adopting an entity
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de::NativeCameraMainTargetMemory
{
    using CheckOwner = ClockResult (*)() noexcept;
    // This is an immediate raw observation. The actual SDK registry must
    // map its own controlled ped or refuse; unknown bits grant no lease.
    ClockResult Read(std::uintptr_t image, CheckOwner owner, CheckOwner afterApplicationOwner, std::uintptr_t& output) noexcept;
}
