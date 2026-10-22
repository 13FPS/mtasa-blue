/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current-facing raw rotation through a fresh registered-ped token
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedRotationAccess
    {
    public:
        virtual ClockResult ReadPedCurrentRotation(const NativePedReferenceToken&, float&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedCurrentRotation(const NativePedReferenceToken&, float) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedRotationAccess() = default;
    };
}
