/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA's on-screen query on the existing borrowed ped SDK chain
 *****************************************************************************/
#pragma once
#include "CPedVisibilityDE.h"
#include "NativePedOnScreenAccess.h"
namespace mta::game_de
{
    class CPedOnScreenDE : public CPedVisibilityDE
    {
    public:
        bool IsOnScreen() override;

    protected:
        CPedOnScreenDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                       NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                       NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                       NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                       NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                       NativePedOrientationAccess& orientation, NativePedAreaAccess& area, NativePedFireAccess& fire, NativePedCollisionAccess& collision,
                       NativePedGogglesAccess& goggles, NativePedVisibilityAccess& visibility, NativePedOnScreenAccess& onScreen,
                       const NativePedReferenceToken& token, std::uint32_t wrapperType, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedVisibilityDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection,
                               placement, water, waterTask, orientation, area, fire, collision, goggles, visibility, token, wrapperType, scalars),
              nativeOnScreenAccess(onScreen)
        {
        }

    private:
        NativePedOnScreenAccess& nativeOnScreenAccess;
    };
    // The actual native camera query is distinct from raw IsVisible and the
    // renderer's IsFullyVisible contract. This slice supplies only IsOnScreen;
    // all factory, intelligence and remaining entity obligations stay pure.
    static_assert(std::is_abstract_v<CPedOnScreenDE> && std::is_base_of_v<CPedVisibilityDE, CPedOnScreenDE>);
}
