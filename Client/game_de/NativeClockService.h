/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Opt-in native owner for MTA's existing clock service
 *****************************************************************************/
#pragma once

#include "../sdk/game/CGameClockService.h"

namespace mta::game_de
{
    // The producer owns requests, while the natural clock continuation owns
    // every SDK call and native restoration. Pending cleanup keeps this owner
    // alive; no partial CGame interface is advertised by this service.
    class NativeClockService final
    {
    public:
        // A generation denotes one cleanup owner, even while End is pending.
        // Copying it would let a second object retire the first object's hooks.
        NativeClockService() = default;
        NativeClockService(const NativeClockService&) = delete;
        NativeClockService& operator=(const NativeClockService&) = delete;
        bool                Begin(const ClockServiceRequestV1& request, ClockServiceStateV1& state) noexcept;
        bool                Submit(const ClockCommandV1& command) noexcept;
        bool                Renew(std::uint64_t generation) noexcept;
        bool                Read(ClockServiceStateV1& state) noexcept;
        bool                End(std::uint64_t generation) noexcept;
        bool                End() noexcept;
        bool                IsRetained() const noexcept;

        static bool ValidateRequest(const ClockServiceRequestV1& request) noexcept;
        static bool ValidateCommand(const ClockCommandV1& command) noexcept;

    private:
        std::uint64_t       generation = 0;
        ClockServiceStateV1 finalState;
        bool                hasFinalState = false;
    };
}
