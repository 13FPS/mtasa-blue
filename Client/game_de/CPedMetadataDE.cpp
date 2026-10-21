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
