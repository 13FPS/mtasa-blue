/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Distinct native ped teleport intent under an existing engine owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
// This typed boundary is also included without the game precompiled header.
// Keep the genuine shared vector's epsilon and platform definitions available.
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <cstdint>

namespace mta::game_de
{
    // This is not an exported service, a SetPosition implementation, or a
    // partial CEntity/CPed object. A timing-only owner refuses native entry.
    class NativePedTeleportAccess
    {
    public:
        virtual ClockResult TeleportPed(std::uint32_t, const CVector&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedTeleportAccess() = default;
    };
}
