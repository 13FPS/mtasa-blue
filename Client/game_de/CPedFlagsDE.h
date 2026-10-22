/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA ped flag intent on one registered lifetime
 ****************************************************************************/
#pragma once
#include "CPedStateDE.h"
#include "NativePedBikeFlagsAccess.h"
#include "NativePedShotFlagsAccess.h"
namespace mta::game_de
{
    // These are independently qualified flag operations. The neighboring
    // TestForShotInVehicle property, SetDucking, target rotation and genuine
    // repair remain pure; this is not a complete ped or native pool factory.
    class CPedFlagsDE : public CPedStateDE
    {
    public:
        std::uint32_t GetCantBeKnockedOffBike() const override;
        void          SetCantBeKnockedOffBike(std::uint32_t value) override;
        bool          GetCanBeShotInVehicle() const override;
        void          SetCanBeShotInVehicle(bool value) override;

    protected:
        CPedFlagsDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                    NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen, NativePedDuckingAccess& ducking,
                    NativePedBikeFlagsAccess& bike, NativePedShotFlagsAccess& shot, const NativePedReferenceToken& token, std::uint32_t wrapperType,
                    NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedStateDE(vitals, metadata, mutation, observation, rotation, oxygen, ducking, token, wrapperType, scalars),
              bikeFlagsAccess(bike),
              shotFlagsAccess(shot)
        {
        }

    private:
        static void               Require(ClockResult, const char* operation);
        NativePedBikeFlagsAccess& bikeFlagsAccess;
        NativePedShotFlagsAccess& shotFlagsAccess;
    };
    static_assert(std::is_abstract_v<CPedFlagsDE> && std::is_base_of_v<CPed, CPedFlagsDE>);
}
