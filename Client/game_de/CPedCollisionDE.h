/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA collision-use intent on the one borrowed ped SDK chain
 ****************************************************************************/
#pragma once
#include "CPedFireDE.h"
#include "NativePedCollisionAccess.h"
namespace mta::game_de
{
    class CPedCollisionDE : public CPedFireDE
    {
    public:
        void SetUsesCollision(bool usesCollision) override;

    protected:
        CPedCollisionDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                        NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                        NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                        NativePedStayAccess& stay, NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection,
                        NativePedPlacementAccess& placement, NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask,
                        NativePedOrientationAccess& orientation, NativePedAreaAccess& area, NativePedFireAccess& fire, NativePedCollisionAccess& collision,
                        const NativePedReferenceToken& token, std::uint32_t wrapperType, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedFireDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, placement,
                         water, waterTask, orientation, area, fire, token, wrapperType, scalars),
              nativeCollisionAccess(collision)
        {
        }

    private:
        NativePedCollisionAccess& nativeCollisionAccess;
    };
    // CEntity has no collision-use getter. Every other unresolved effect keeps
    // its own pure SDK contract; this is not a complete ped/physics factory.
    static_assert(std::is_abstract_v<CPedCollisionDE> && std::is_base_of_v<CPedFireDE, CPedCollisionDE>);
}
