/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Opt-in native owner for MTA's existing clock service
 *****************************************************************************/
#pragma once

#include "../sdk/game/CGameClockService.h"

namespace mta::game_de
{
    class NativeGameTimingAccess;
    using GameTimingHandler = void (*)(NativeGameTimingAccess&, void*);
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
        // Internal CGame timing ownership also installs the pause-safe native
        // timer hook. The existing exported V1 clock path stays clock-only.
        bool BeginGameTiming(const ClockServiceRequestV1& request, ClockServiceStateV1& state, GameTimingHandler handler = nullptr,
                             void* context = nullptr) noexcept;
        bool Submit(const ClockCommandV1& command) noexcept;
        bool Renew(std::uint64_t generation) noexcept;
        bool Read(ClockServiceStateV1& state) noexcept;
        bool End(std::uint64_t generation) noexcept;
        bool End() noexcept;
        bool IsRetained() const noexcept;

        static bool ValidateRequest(const ClockServiceRequestV1& request) noexcept;
        static bool ValidateCommand(const ClockCommandV1& command) noexcept;

    private:
        bool BeginOwner(const ClockServiceRequestV1& request, ClockServiceStateV1& state, bool timingMode, GameTimingHandler handler, void* context) noexcept;
        std::uint64_t       generation = 0;
        ClockServiceStateV1 finalState;
        bool                hasFinalState = false;
    };
}
