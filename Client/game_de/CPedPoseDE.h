/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual inherited position/teleport intent on the registered ped
 ****************************************************************************/
#pragma once
#include "CPedWeaponSelectionDE.h"
#include "NativePedPlacementAccess.h"
namespace mta::game_de
{
    // Matrix/orientation writes require additional native world/RW operations.
    // They and real last-good physics repair remain pure; this layer does not
    // complete or publish a ped, player, pool or native allocation factory.
    class CPedPoseDE : public CPedWeaponSelectionDE
    {
    public:
        void SetPosition(float x, float y, float z) override;
        void SetPosition(CVector* position) override;
        void Teleport(float x, float y, float z) override;

    protected:
        CPedPoseDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation, NativePedObservationAccess& observation,
                   NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen, NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike,
                   NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding, NativePedStayAccess& stay, NativePedFightingStyleAccess& combat,
                   NativePedCurrentWeaponSlotAccess& selection, NativePedPlacementAccess& placement, const NativePedReferenceToken& token,
                   std::uint32_t wrapperType, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedWeaponSelectionDE(vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection, token,
                                    wrapperType, scalars),
              nativePlacementAccess(placement),
              nativePoseObservationAccess(observation)
        {
        }

    private:
        static void                 Require(ClockResult, const char* operation);
        NativePedPlacementAccess&   nativePlacementAccess;
        NativePedObservationAccess& nativePoseObservationAccess;
    };
    static_assert(std::is_abstract_v<CPedPoseDE> && std::is_base_of_v<CPed, CPedPoseDE>);
}
