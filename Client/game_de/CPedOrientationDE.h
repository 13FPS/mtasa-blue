/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine three-axis ped placement within the existing SDK chain
 ****************************************************************************/
#pragma once
#include "CPedWaterDE.h"
#include "NativePedOrientationAccess.h"
namespace mta::game_de
{
    // The native ped route owns world removal, Euler placement and the real
    // DE actor/component/world update. It does not fabricate classic RW data,
    // target rotation, a general SetMatrix implementation or physics repair.
    class CPedOrientationDE : public CPedWaterDE
    {
    public:
        void SetOrientation(float x, float y, float z) override;

    protected:
        CPedOrientationDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                          NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                          NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                          NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                          NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                          NativePedOrientationAccess& orientation, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                          NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedWaterDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, placement,
                          water, waterTask, token, wrapperType, scalars),
              nativeOrientationAccess(orientation)
        {
        }

    private:
        NativePedOrientationAccess& nativeOrientationAccess;
    };
    static_assert(std::is_abstract_v<CPedOrientationDE> && std::is_base_of_v<CPedWaterDE, CPedOrientationDE>);
}
