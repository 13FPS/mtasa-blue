/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's actual player rebuild SDK contract on DE
 *****************************************************************************/
#pragma once
#include "CPedClothesDE.h"
#include "NativePedRebuildAccess.h"
namespace mta::game_de
{
    class CPedRebuildDE : public CPedClothesDE
    {
    public:
        void RebuildPlayer() override;

    protected:
        CPedRebuildDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                      NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                      NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                      NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                      NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                      NativePedOrientationAccess& orientation, NativePedAreaAccess& area, NativePedFireAccess& fire, NativePedCollisionAccess& collision,
                      NativePedGogglesAccess& goggles, NativePedVisibilityAccess& visibility, NativePedOnScreenAccess& onScreen,
                      NativePedWeaponGrantAccess& grant, NativePedWeaponClearAccess& clear, NativePedClothesAccess& clothes, NativePedRebuildAccess& rebuild,
                      const NativePedReferenceToken& token, std::uint32_t wrapperType, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedClothesDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, placement,
                            water, waterTask, orientation, area, fire, collision, goggles, visibility, onScreen, grant, clear, clothes, token, wrapperType,
                            scalars),
              nativeRebuildAccess(rebuild)
        {
        }

    private:
        NativePedRebuildAccess& nativeRebuildAccess;
    };
    // Rebuilding acts on the same registered ped and its engine-owned data.
    // This borrowed layer does not create a player, publish a factory, or
    // claim the remaining abstract SDK methods are ready.
    static_assert(std::is_abstract_v<CPedRebuildDE> && std::is_base_of_v<CPedClothesDE, CPedRebuildDE>);
}
