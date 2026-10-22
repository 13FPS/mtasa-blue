/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual current weapon-slot SDK observation on the registered ped
 ****************************************************************************/
#pragma once
#include "CPedCombatDE.h"
#include "NativePedCurrentWeaponSlotAccess.h"
namespace mta::game_de
{
    // Reading the selected byte grants no actor/model mutation authority.
    // SetCurrentWeaponSlot and all weapon allocation/lifecycle remain pure.
    class CPedWeaponSelectionDE : public CPedCombatDE
    {
    public:
        eWeaponSlot GetCurrentWeaponSlot() const override;

    protected:
        CPedWeaponSelectionDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                              NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                              NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot,
                              NativePedBleedingAccess& bleeding, NativePedStayAccess& stay, NativePedFightingStyleAccess& combat,
                              NativePedCurrentWeaponSlotAccess& selection, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                              NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedCombatDE(vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, token, wrapperType, scalars),
              currentWeaponSelectionAccess(selection)
        {
        }

    private:
        NativePedCurrentWeaponSlotAccess& currentWeaponSelectionAccess;
    };
    static_assert(std::is_abstract_v<CPedWeaponSelectionDE> && std::is_base_of_v<CPed, CPedWeaponSelectionDE>);
}
