/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native ped physical scalar intent through a registered token
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"

namespace mta::game_de
{
    enum class NativePedPhysicalScalarField : std::uint32_t
    {
        Mass,
        TurnMass
    };
    // This narrow optional family grants no CPed/CPools publication. Its real
    // owner must resolve the registered token around every native observation
    // or persistent scalar mutation; a numeric handle is not a lifetime grant.
    class NativePedPhysicalScalarsAccess
    {
    public:
        virtual ClockResult ReadPedPhysicalScalar(const NativePedReferenceToken&, NativePedPhysicalScalarField, float&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPedPhysicalScalar(const NativePedReferenceToken&, NativePedPhysicalScalarField, float) noexcept
        {
            return ClockResult::NotInitialized;
        }

    protected:
        ~NativePedPhysicalScalarsAccess() = default;
    };
}
