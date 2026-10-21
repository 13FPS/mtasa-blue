/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read actual ped model/type metadata through a registered token
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"

namespace mta::game_de
{
    struct NativePedMetadata
    {
        std::uint16_t modelIndex = 0;
        std::uint8_t  entityType = 0;
    };

    class NativePedMetadataAccess
    {
    public:
        virtual ClockResult ReadPedMetadata(const NativePedReferenceToken&, NativePedMetadata&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedMetadataAccess() = default;
    };
}
