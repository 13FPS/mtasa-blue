/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's actual clothing descriptor SDK contract on DE
 *****************************************************************************/
#pragma once
#include "CPedClearWeaponDE.h"
#include "NativePedClothesAccess.h"
namespace mta::game_de
{
    class CPedClothesDE : public CPedClearWeaponDE
    {
    public:
        void SetClothesTextureAndModel(const char* texture, const char* model, int textureType) override;

    protected:
        CPedClothesDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                      NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                      NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                      NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                      NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                      NativePedOrientationAccess& orientation, NativePedAreaAccess& area, NativePedFireAccess& fire, NativePedCollisionAccess& collision,
                      NativePedGogglesAccess& goggles, NativePedVisibilityAccess& visibility, NativePedOnScreenAccess& onScreen,
                      NativePedWeaponGrantAccess& grant, NativePedWeaponClearAccess& clear, NativePedClothesAccess& clothes,
                      const NativePedReferenceToken& token, std::uint32_t wrapperType, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedClearWeaponDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection,
                                placement, water, waterTask, orientation, area, fire, collision, goggles, visibility, onScreen, grant, clear, token,
                                wrapperType, scalars),
              nativeClothesAccess(clothes)
        {
        }

    private:
        NativePedClothesAccess& nativeClothesAccess;
    };
    // Editing the existing descriptor is distinct from rebuilding a player.
    // The complete ped still owns RebuildPlayer and all remaining SDK methods;
    // this borrowed layer creates no clothes, player data or native factory.
    static_assert(std::is_abstract_v<CPedClothesDE> && std::is_base_of_v<CPedClearWeaponDE, CPedClothesDE>);
}
