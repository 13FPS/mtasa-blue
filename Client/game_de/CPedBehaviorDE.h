/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA bleeding/stay storage through one registered ped token
 ****************************************************************************/
#pragma once
#include "CPedFlagsDE.h"
#include "NativePedBleedingAccess.h"
#include "NativePedStayAccess.h"
namespace mta::game_de
{
    // Only independently named native storage is implemented. No blood-render
    // call, task-presence guess or neighboring KindaStayInSamePlace bit is a
    // substitute for these actual SDK properties. Remaining methods stay pure.
    class CPedBehaviorDE : public CPedFlagsDE
    {
    public:
        bool IsBleeding() const override;
        void SetBleeding(bool value) override;
        bool GetStayInSamePlace() const override;
        void SetStayInSamePlace(bool value) override;

    protected:
        CPedBehaviorDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                       NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen,
                       NativePedDuckingAccess& ducking, NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, NativePedBleedingAccess& bleeding,
                       NativePedStayAccess& stay, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                       NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedFlagsDE(vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, token, wrapperType, scalars),
              bleedingAccess(bleeding),
              stayAccess(stay)
        {
        }

    private:
        static void              Require(ClockResult, const char* operation);
        NativePedBleedingAccess& bleedingAccess;
        NativePedStayAccess&     stayAccess;
    };
    static_assert(std::is_abstract_v<CPedBehaviorDE> && std::is_base_of_v<CPed, CPedBehaviorDE>);
}
