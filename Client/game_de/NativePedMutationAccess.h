/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Ped vector intent through a retained registered-reference owner
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedVelocityAccess.h"

namespace mta::game_de
{
    // A CPU token identifies the owner's retained native reference cell, not a
    // script handle or a cached ped pointer. The actual owner resolves it both
    // before and after the existing raw vector transaction. Entered failures
    // preserve native effects and must not be retried by an SDK facade.
    class NativePedMutationAccess
    {
    public:
        virtual ClockResult SetReferencedPedVelocity(const NativePedReferenceToken&, NativePedVelocityKind, const CVector&) noexcept
        {
            return ClockResult::NotInitialized;
        }

    protected:
        ~NativePedMutationAccess() = default;
    };
}
