/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual task-based water query and physical flag intent in one ped chain
 ****************************************************************************/
#pragma once
#include "CPedWeaponStatsDE.h"
#include "NativePedWaterFlagsAccess.h"
#include "NativePedWaterTaskAccess.h"
namespace mta::game_de
{
    // The query observes primary2/type268; the setter changes two independent
    // physical flags. Neither operation substitutes for the other or publishes
    // a task pointer, complete ped implementation or native lifetime lease.
    class CPedWaterDE : public CPedWeaponStatsDE
    {
    public:
        bool IsInWater() const override;
        void SetInWaterFlags(bool inWater) override;

    protected:
        CPedWaterDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                    NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen, NativePedDuckingAccess& ducking,
                    NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding, NativePedStayAccess& stay,
                    NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection, NativePedPlacementAccess& placement,
                    NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                    NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedWeaponStatsDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection,
                                placement, token, wrapperType, scalars),
              nativeWaterFlagsAccess(water),
              nativeWaterTaskAccess(waterTask)
        {
        }

    private:
        NativePedWaterFlagsAccess& nativeWaterFlagsAccess;
        NativePedWaterTaskAccess&  nativeWaterTaskAccess;
    };
    static_assert(std::is_abstract_v<CPedWaterDE> && std::is_base_of_v<CPedWeaponStatsDE, CPedWaterDE>);
}
