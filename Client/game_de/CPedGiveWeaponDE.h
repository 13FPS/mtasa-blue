/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual MTA weapon preparation on the borrowed ped SDK chain
 *****************************************************************************/
#pragma once
#include "CPedOnScreenDE.h"
#include "NativePedWeaponGrantAccess.h"
namespace mta::game_de
{
    class CPedGiveWeaponDE : public CPedOnScreenDE
    {
    public:
        CWeapon* GiveWeapon(eWeaponType weaponType, std::uint32_t ammo, eWeaponSkill skill) override;

    protected:
        CPedGiveWeaponDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                         NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                         NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                         NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                         NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                         NativePedOrientationAccess& orientation, NativePedAreaAccess& area, NativePedFireAccess& fire, NativePedCollisionAccess& collision,
                         NativePedGogglesAccess& goggles, NativePedVisibilityAccess& visibility, NativePedOnScreenAccess& onScreen,
                         NativePedWeaponGrantAccess& grant, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                         NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedOnScreenDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, placement,
                             water, waterTask, orientation, area, fire, collision, goggles, visibility, onScreen, token, wrapperType, scalars),
              weaponGame(game),
              nativeGrantAccess(grant)
        {
        }

    private:
        CGame&                      weaponGame;
        NativePedWeaponGrantAccess& nativeGrantAccess;
    };
    // The genuine CGame/model services still own Request/MakeCustomModel, and
    // both GetWeapon overloads remain pure: only the enclosing complete ped can
    // supply its existing wrapper bank. This layer creates no native weapon,
    // model implementation, ready flag or complete factory on its own.
    static_assert(std::is_abstract_v<CPedGiveWeaponDE> && std::is_base_of_v<CPedOnScreenDE, CPedGiveWeaponDE>);
}
