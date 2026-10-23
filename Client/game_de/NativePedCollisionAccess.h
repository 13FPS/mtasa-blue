/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual collision-use setter through the existing registered ped owner
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedCollisionAccess
    {
    public:
        virtual ClockResult SetPedUsesCollision(const NativePedReferenceToken&, bool) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedCollisionAccess() = default;
    };
}
