/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Report checked native metadata without manufacturing SDK defaults
 ****************************************************************************/
#include "CPedMetadataDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    NativePedMetadata CPedMetadataDE::Read(const char* operation) const
    {
        NativePedMetadata value;
        const auto        result = metadata.ReadPedMetadata(ReferenceToken(), value);
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped metadata owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
        return value;
    }
    RpClump* CPedMetadataDE::GetRpClump()
    {
        const auto     selected = ReferenceToken();
        std::uintptr_t value = 0;
        const auto     result = metadata.ReadPedRender(selected, value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped metadata owner refused GetRpClump: " + std::to_string(static_cast<unsigned>(result)));
        // Actual CEntitySA returns its render field directly. Preserve a
        // successful null and the complete opaque backend tag; a later clump
        // consumer must independently qualify its own schema and lifetime.
        return reinterpret_cast<RpClump*>(value);
    }
    WORD CPedMetadataDE::GetModelIndex()
    {
        // The native field is a raw WORD. A model-bank lookup or special FFFF
        // fallback would change the existing SDK's direct field semantics.
        return Read("GetModelIndex").modelIndex;
    }
    eEntityType CPedMetadataDE::GetEntityType()
    {
        return static_cast<eEntityType>(Read("GetEntityType").entityType);
    }
}
