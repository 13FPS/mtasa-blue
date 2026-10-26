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
        // The source GetRpClump returns the current opaque render tag, even
        // when null. This separate channel must not add renderer bindings to
        // model/type reads or grant pointee, node, initialization or lease
        // authority merely because an entity's field was observed.
        virtual ClockResult ReadPedRender(const NativePedReferenceToken&, std::uintptr_t&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedMetadataAccess() = default;
    };
}
