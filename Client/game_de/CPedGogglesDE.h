/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual ped goggles-object presence on the borrowed SDK chain
 ****************************************************************************/
#pragma once
#include "CPedTasksDE.h"
#include "NativePedGogglesAccess.h"
namespace mta::game_de
{
    class CPedGogglesDE : public CPedTasksDE
    {
    public:
        bool IsWearingGoggles() const override;

    protected:
        CPedGogglesDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                      NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                      NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                      NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                      NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                      NativePedOrientationAccess& orientation, NativePedAreaAccess& area, NativePedFireAccess& fire, NativePedCollisionAccess& collision,
                      NativePedGogglesAccess& goggles, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                      NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedTasksDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, placement,
                          water, waterTask, orientation, area, fire, collision, token, wrapperType, scalars),
              nativeGogglesAccess(goggles)
        {
        }

    private:
        NativePedGogglesAccess& nativeGogglesAccess;
    };
    // Presence is the actual SDK contract, independent of vision mode or task
    // state. Native goggles mutation and the real intelligence/factory hooks
    // remain mandatory; this layer does not store another intelligence owner.
    static_assert(std::is_abstract_v<CPedGogglesDE> && std::is_base_of_v<CPedTasksDE, CPedGogglesDE>);
}
