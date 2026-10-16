/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit local movement write trial inside the native control scope
 *****************************************************************************/

#pragma once

#include "../sdk/game/CGameAdapter.h"
#include "../sdk/game/CGameFrontendInputLease.h"
#include <cstdint>

namespace mta::game_de
{
    // This bounded experiment uses MTA's canonical controller state and context
    // lifetime. It does not establish a remote entity or a complete CGame port.
    // Begin/Update/End belong to the producer. The verified native callback
    // adopts its own simulation thread and consumes coherent published input.
    // End may return false while that thread finishes restoring its scope;
    // the producer must retain the adapter and retry cleanup on a later tick.
    class NativeMovementInput final
    {
    public:
        bool        Begin(const MtaInputState& state) noexcept;
        bool        Update(const MtaInputState& state) noexcept;
        bool        BeginFrontendLease(const FrontendInputLeaseRequestV1& request) noexcept;
        static bool ValidateFrontendLeaseRequest(const FrontendInputLeaseRequestV1& request) noexcept;
        bool        RenewFrontendLease(std::uint64_t requestedGeneration, const MtaInputState& state) noexcept;
        bool        ReadFrontendLease(FrontendInputLeaseStateV1& state) noexcept;
        bool        EndFrontendLease(std::uint64_t requestedGeneration) noexcept;
        bool        IsFrontendLease() const noexcept;
        bool        End() noexcept;
        bool        IsBound() const noexcept;
        bool        IsActive() const noexcept;
        // The adapter first verifies its existing native-reader binding.
        // Before Begin this observes local on-foot/IDLE/controls-enabled
        // admission. During a trial it consumes fresh, generation/identity-bound
        // engine publications, since ambient pad globals may belong to another
        // ped. Missing/stale/inactive evidence preserves the caller's output;
        // weapon/task context and general gameplay readiness remain unknown.
        bool ReadEligibility(NativeMovementEligibility& eligibility) const noexcept;
        void ReadState(NativeMovementTrialState& state) const noexcept;

    private:
        bool                     BeginInternal(const MtaInputState& state, void* frontendWindow) noexcept;
        std::uint64_t            generation = 0;
        NativeMovementTrialState finalState;
        bool                     hasFinalState = false;
    };
}
