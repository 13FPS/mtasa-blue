/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw MTA vector setter intent and checked owner failures
 ****************************************************************************/
#include "CPedMutationDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CPedMutationDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped mutation owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }

    ClockResult CPedMutationDE::TrySetMoveSpeed(const CVector& velocity) noexcept
    {
        // Classic peds copy vector bits without a finite-value filter. Only
        // objects add moving-list/static side effects; qualified ped type3
        // excludes that branch. Native admission and partial-write accounting
        // stay in the existing owner, with no retry or arithmetic here.
        const auto result = mutation.SetReferencedPedVelocity(ReferenceToken(), NativePedVelocityKind::Linear, velocity);
        // The inherited void noexcept method cannot throw on native refusal.
        // Match CClockDE's explicit Try/result path instead of hiding failure
        // behind cached velocity or a fabricated successful SDK operation.
        moveSpeedResult.store(result);
        return result;
    }

    void CPedMutationDE::SetMoveSpeed(const CVector& velocity) noexcept
    {
        TrySetMoveSpeed(velocity);
    }

    void CPedMutationDE::SetTurnSpeed(CVector* velocity)
    {
        if (!velocity)
        {
            Require(ClockResult::InvalidArgument, "SetTurnSpeed");
            return;
        }
        // The legacy pointer is an input vector, not a native lifetime lease.
        // Preserve its exact bits through the angular intent, while the owner
        // freshly resolves the same immutable token used by inherited slices.
        Require(mutation.SetReferencedPedVelocity(ReferenceToken(), NativePedVelocityKind::Angular, *velocity), "SetTurnSpeed");
    }
}
