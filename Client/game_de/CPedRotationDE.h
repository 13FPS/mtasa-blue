/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual CPed current-facing intent on one registered token
 ****************************************************************************/
#pragma once
#include "CPedPhysicsDE.h"
#include "NativePedRotationAccess.h"

namespace mta::game_de
{
    // Only the independently qualified current-facing field is implemented.
    // Target rotation and the genuine native physics repair remain pure, so
    // this layer cannot be published as a complete SDK ped or pool factory.
    class CPedRotationDE : public CPedPhysicsDE
    {
    public:
        float GetCurrentRotation() const override;
        void  SetCurrentRotation(float rotation) override;

    protected:
        CPedRotationDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                       NativePedObservationAccess& observation, NativePedRotationAccess& rotation, const NativePedReferenceToken& token,
                       NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedPhysicsDE(vitals, metadata, mutation, observation, token, scalars), rotationAccess(rotation)
        {
        }

    private:
        static void              Require(ClockResult, const char* operation);
        NativePedRotationAccess& rotationAccess;
    };
    static_assert(std::is_abstract_v<CPedRotationDE> && std::is_base_of_v<CPed, CPedRotationDE>);
}
