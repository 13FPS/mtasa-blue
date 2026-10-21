/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Physical observations through MTA's retained native ped reference
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedTransformReader.h"
#include "NativePedVelocityAccess.h"

namespace mta::game_de
{
    // The same CPU token used by SDK mutations selects a retained native cell.
    // Every observation resolves that cell before and after the existing native
    // read. These raw facts do not implement the SDK's invalid-physics repair.
    class NativePedObservationAccess
    {
    public:
        virtual ClockResult ReadReferencedPedTransform(const NativePedReferenceToken&, NativePedTransformReader::Value&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult BorrowReferencedPedPosition(const NativePedReferenceToken&, CVector*&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadReferencedPedVelocity(const NativePedReferenceToken&, NativePedVelocityKind, CVector&) noexcept
        {
            return ClockResult::NotInitialized;
        }

    protected:
        ~NativePedObservationAccess() = default;
    };
}
