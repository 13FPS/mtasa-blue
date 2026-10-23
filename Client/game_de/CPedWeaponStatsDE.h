/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual MTA weapon-stat orchestration on the borrowed game
 ****************************************************************************/
#pragma once
#include "CPedPoseDE.h"
class CGame;
namespace mta::game_de
{
    // The enclosing wrapper owns its ordinary CGame/ped lifetime. Weapon
    // lookup/allocation and every remaining ped effect stay pure; these two
    // methods neither create a native weapon nor publish a complete factory.
    class CPedWeaponStatsDE : public CPedPoseDE
    {
    public:
        CWeaponStat* GetCurrentWeaponStat() const override;
        float        GetCurrentWeaponRange() const override;

    protected:
        CPedWeaponStatsDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                          NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                          NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                          NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                          NativePedPlacementAccess& placement, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                          NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedPoseDE(vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, placement, token,
                         wrapperType, scalars),
              game(game)
        {
        }

    private:
        CGame& game;
    };
    static_assert(std::is_abstract_v<CPedWeaponStatsDE> && std::is_base_of_v<CPed, CPedWeaponStatsDE>);
}
