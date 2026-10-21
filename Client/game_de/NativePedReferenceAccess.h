/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native registered-cell lifetime through the existing ped owner
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"

namespace mta::game_de
{
    class NativePedReferenceCell;
    // This optional internal family creates neither a ped wrapper nor a pool.
    // Callers retain their stable cell metadata until release is acknowledged.
    class NativePedReferenceAccess
    {
    public:
        virtual ClockResult RegisterPedReference(std::uint32_t, NativePedReferenceCell&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadPedReference(const NativePedReferenceCell&, NativePedPoolReader::Value&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReleasePedReference(NativePedReferenceCell&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedReferenceAccess() = default;
    };
}
