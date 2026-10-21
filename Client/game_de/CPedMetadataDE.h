/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual inherited ped model/type methods through one registered token
 ****************************************************************************/
#pragma once
#include "CPedVitalsDE.h"
#include "NativePedMetadataAccess.h"

namespace mta::game_de
{
    // This adds only the two qualified inherited SDK readers. No model bank,
    // concrete ped factory or native lifetime is supplied by an abstract layer.
    class CPedMetadataDE : public CPedVitalsDE
    {
    public:
        WORD        GetModelIndex() override;
        eEntityType GetEntityType() override;

    protected:
        CPedMetadataDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, const NativePedReferenceToken& token) noexcept
            : CPedVitalsDE(vitals, token), metadata(metadata)
        {
        }

    private:
        NativePedMetadata        Read(const char* operation) const;
        NativePedMetadataAccess& metadata;
    };
    static_assert(std::is_abstract_v<CPedMetadataDE> && std::is_base_of_v<CPed, CPedMetadataDE>);
}
