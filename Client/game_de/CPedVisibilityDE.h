/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual entity visibility intent on the borrowed ped SDK chain
 ****************************************************************************/
#pragma once
#include "CPedGogglesDE.h"
#include "NativePedVisibilityAccess.h"
namespace mta::game_de
{
    class CPedVisibilityDE : public CPedGogglesDE
    {
    public:
        bool IsVisible() override;
        void SetVisible(bool visible) override;

    protected:
        CPedVisibilityDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                         NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                         NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                         NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                         NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                         NativePedOrientationAccess& orientation, NativePedAreaAccess& area, NativePedFireAccess& fire, NativePedCollisionAccess& collision,
                         NativePedGogglesAccess& goggles, NativePedVisibilityAccess& visibility, const NativePedReferenceToken& token,
                         std::uint32_t wrapperType, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedGogglesDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, placement,
                            water, waterTask, orientation, area, fire, collision, goggles, token, wrapperType, scalars),
              nativeVisibilityAccess(visibility)
        {
        }

    private:
        NativePedVisibilityAccess& nativeVisibilityAccess;
    };
    // Only the qualified raw entity visibility property is delegated. This
    // layer creates no renderer, native entity, script current-player effect
    // or intelligence/factory implementation; those hooks remain mandatory.
    static_assert(std::is_abstract_v<CPedVisibilityDE> && std::is_base_of_v<CPedGogglesDE, CPedVisibilityDE>);
}
