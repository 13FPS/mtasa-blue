/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA fighting-style intent through the registered ped owner
 ****************************************************************************/
#pragma once
#include "CPedBehaviorDE.h"
#include "NativePedFightingStyleAccess.h"
namespace mta::game_de
{
    // Native memory applies the shared classic same-style/extra-bit policy
    // after qualifying this token. The SDK never caches a style, calls the
    // different script reset policy, or publishes a complete ped factory.
    class CPedCombatDE : public CPedBehaviorDE
    {
    public:
        eFightingStyle GetFightingStyle() const override;
        void           SetFightingStyle(eFightingStyle style, std::uint8_t styleExtra = 6) override;

    protected:
        CPedCombatDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                     NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen, NativePedDuckingAccess& ducking,
                     NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding, NativePedStayAccess& stay,
                     NativePedFightingStyleAccess& combat, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                     NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedBehaviorDE(vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, token, wrapperType, scalars),
              combatAccess(combat)
        {
        }

    private:
        static void                   Require(ClockResult, const char* operation);
        NativePedFightingStyleAccess& combatAccess;
    };
    static_assert(std::is_abstract_v<CPedCombatDE> && std::is_base_of_v<CPed, CPedCombatDE>);
}
