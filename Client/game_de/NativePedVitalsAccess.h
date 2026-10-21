/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Internal token-based current-player health and armour access
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"

namespace mta::game_de
{
    enum class NativePedVitalField : std::uint32_t
    {
        Health,
        Armor
    };

    // This optional family borrows the actual registered-cell controller.
    // It creates no CPed/CPools wrapper and refuses before its real owner opts in.
    class NativePedVitalsAccess
    {
    public:
        virtual ClockResult ReadPedVital(const NativePedReferenceToken&, NativePedVitalField, float&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedVital(const NativePedReferenceToken&, NativePedVitalField, float) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedVitalsAccess() = default;
    };
}
