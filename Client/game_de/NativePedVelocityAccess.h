/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw physical vector intent through an existing native ped owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <cstdint>

namespace mta::game_de
{
    enum class NativePedVelocityKind : std::uint32_t
    {
        Linear,
        Angular
    };

    // The shared SDK getters copy into caller storage. This narrow boundary
    // observes raw native vectors; it does not fabricate the full physical
    // wrapper's last-good-state repair, an SDK entity or a retained alias.
    class NativePedVelocityAccess
    {
    public:
        virtual ClockResult ReadPedVelocity(std::uint32_t, NativePedVelocityKind, CVector&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedVelocity(std::uint32_t, NativePedVelocityKind, const CVector&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedVelocityAccess() = default;
    };
}
