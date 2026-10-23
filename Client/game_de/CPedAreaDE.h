/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual raw CEntity area-code SDK pair on the registered ped chain
 ****************************************************************************/
#pragma once
#include "CPedOrientationDE.h"
#include "NativePedAreaAccess.h"
namespace mta::game_de
{
    class CPedAreaDE : public CPedOrientationDE
    {
    public:
        BYTE GetAreaCode() override;
        void SetAreaCode(BYTE) override;

    protected:
        CPedAreaDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                   NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen, NativePedDuckingAccess& ducking,
                   NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding, NativePedStayAccess& stay,
                   NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection, NativePedPlacementAccess& placement,
                   NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask, NativePedOrientationAccess& orientation, NativePedAreaAccess& area,
                   const NativePedReferenceToken& token, std::uint32_t wrapperType, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedOrientationDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection,
                                placement, water, waterTask, orientation, token, wrapperType, scalars),
              nativeAreaAccess(area)
        {
        }

    private:
        NativePedAreaAccess& nativeAreaAccess;
    };
    static_assert(std::is_abstract_v<CPedAreaDE> && std::is_base_of_v<CPedOrientationDE, CPedAreaDE>);
}
