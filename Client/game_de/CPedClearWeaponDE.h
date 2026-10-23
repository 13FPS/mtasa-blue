/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's actual single-weapon ped SDK contract on DE
 *****************************************************************************/
#pragma once
#include "CPedGiveWeaponDE.h"
#include "NativePedWeaponClearAccess.h"
namespace mta::game_de
{
    class CPedClearWeaponDE : public CPedGiveWeaponDE
    {
    public:
        void ClearWeapon(eWeaponType weaponType) override;

    protected:
        CPedClearWeaponDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                          NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                          NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                          NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                          NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                          NativePedOrientationAccess& orientation, NativePedAreaAccess& area, NativePedFireAccess& fire, NativePedCollisionAccess& collision,
                          NativePedGogglesAccess& goggles, NativePedVisibilityAccess& visibility, NativePedOnScreenAccess& onScreen,
                          NativePedWeaponGrantAccess& grant, NativePedWeaponClearAccess& clear, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                          NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedGiveWeaponDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection,
                               placement, water, waterTask, orientation, area, fire, collision, goggles, visibility, onScreen, grant, token, wrapperType,
                               scalars),
              nativeClearAccess(clear)
        {
        }

    private:
        NativePedWeaponClearAccess& nativeClearAccess;
    };
    // The genuine single-weapon operation needs no returned wrapper lookup.
    // ClearWeapons and both GetWeapon overloads remain the complete ped's
    // obligations; this layer publishes no weapon bank or complete factory.
    static_assert(std::is_abstract_v<CPedClearWeaponDE> && std::is_base_of_v<CPedGiveWeaponDE, CPedClearWeaponDE>);
}
