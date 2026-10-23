/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual physical water-flag setter in the single DE ped wrapper chain
 ****************************************************************************/
#pragma once
#include "CPedWeaponStatsDE.h"
#include "NativePedWaterFlagsAccess.h"
namespace mta::game_de
{
    // SDK IsInWater tests a primary task, not these physical flags. It remains
    // pure until that distinct native task/lifetime contract is qualified.
    class CPedWaterDE : public CPedWeaponStatsDE
    {
    public:
        void SetInWaterFlags(bool inWater) override;

    protected:
        CPedWaterDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                    NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen, NativePedDuckingAccess& ducking,
                    NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding, NativePedStayAccess& stay,
                    NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection, NativePedPlacementAccess& placement,
                    NativePedWaterFlagsAccess& water, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                    NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedWeaponStatsDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection,
                                placement, token, wrapperType, scalars),
              nativeWaterFlagsAccess(water)
        {
        }

    private:
        NativePedWaterFlagsAccess& nativeWaterFlagsAccess;
    };
    static_assert(std::is_abstract_v<CPedWaterDE> && std::is_base_of_v<CPedWeaponStatsDE, CPedWaterDE>);
}
