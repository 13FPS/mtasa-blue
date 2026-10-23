/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual const CPed fire predicate, distinct from CEntity overload
 ****************************************************************************/
#pragma once
#include "CPedAreaDE.h"
#include "NativePedFireAccess.h"
namespace mta::game_de
{
    class CPedFireDE : public CPedAreaDE
    {
    public:
        bool IsOnFire() const override;

    protected:
        CPedFireDE(CGame& game, NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                   NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen, NativePedDuckingAccess& ducking,
                   NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding, NativePedStayAccess& stay,
                   NativePedFightingStyleAccess& combat, NativePedCurrentWeaponSlotAccess& selection, NativePedPlacementAccess& placement,
                   NativePedWaterFlagsAccess& water, NativePedWaterTaskAccess& waterTask, NativePedOrientationAccess& orientation, NativePedAreaAccess& area,
                   NativePedFireAccess& fire, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                   NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedAreaDE(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, placement,
                         water, waterTask, orientation, area, token, wrapperType, scalars),
              nativeFireAccess(fire)
        {
        }

    private:
        NativePedFireAccess& nativeFireAccess;
    };
    // The inherited nonconst CEntity fallback remains a separate CPU policy.
    // This const declaration hides it for direct class lookup, like CPedSA.
    // SetOnFire and the remaining factory/lifetime contracts stay mandatory.
    static_assert(std::is_abstract_v<CPedFireDE> && std::is_base_of_v<CPedAreaDE, CPedFireDE>);
}
