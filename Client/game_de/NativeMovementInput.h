/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit local movement write trial inside the native control scope
 *****************************************************************************/

#pragma once

#include "../sdk/game/CGameAdapter.h"
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
        bool Begin(const MtaInputState& state) noexcept;
        bool Update(const MtaInputState& state) noexcept;
        bool End() noexcept;
        bool IsBound() const noexcept;
        bool IsActive() const noexcept;
        void ReadState(NativeMovementTrialState& state) const noexcept;

    private:
        std::uint64_t            generation = 0;
        NativeMovementTrialState finalState;
        bool                     hasFinalState = false;
    };
}
