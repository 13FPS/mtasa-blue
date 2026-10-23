/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep native three-axis orientation on the sole registered ped owner
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedTransformReader.h"
namespace mta::game_de
{
    class NativePedOrientationAccess
    {
    public:
        // The exact original three floats are radians, not the named script's
        // degree inputs. This is not matrix storage, heading-only or repair.
        virtual ClockResult SetPedOrientation(const NativePedReferenceToken&, const CVector&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedOrientationAccess() = default;
    };
}
