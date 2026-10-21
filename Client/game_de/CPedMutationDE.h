/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual inherited ped vector setters through one registered token
 ****************************************************************************/
#pragma once
#include "CPedMetadataDE.h"
#include "NativePedMutationAccess.h"
#include "NativePedPhysicalScalarsAccess.h"
#include <atomic>

namespace mta::game_de
{
    // Classic vector/placement getters repair invalid physics through last-good wrapper
    // state. That policy is not supplied here, so getters and the remaining
    // entity/ped methods stay pure rather than returning unchecked raw fields.
    class CPedMutationDE : public CPedMetadataDE
    {
    public:
        void  SetMoveSpeed(const CVector& velocity) noexcept override;
        void  SetTurnSpeed(CVector* velocity) override;
        void* GetStoredPointer() override;
        void  SetStoredPointer(void* pointer) override;
        float GetMass() override;
        void  SetMass(float mass) override;
        float GetTurnMass() override;
        void  SetTurnMass(float mass) override;

        float GetAirResistance() override;
        void  SetAirResistance(float resistance) override;
        float GetElasticity() override;
        void  SetElasticity(float elasticity) override;
        void  GetCenterOfMass(CVector& center) override;
        void  SetCenterOfMass(CVector& center) override;

        ClockResult TrySetMoveSpeed(const CVector& velocity) noexcept;
        ClockResult LastMoveSpeedResult() const noexcept { return moveSpeedResult.load(); }

    protected:
        CPedMutationDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                       const NativePedReferenceToken& token, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedMetadataDE(vitals, metadata, token), mutation(mutation), physicalScalars(scalars)
        {
        }

    private:
        static void Require(ClockResult result, const char* operation);
        float       ReadPhysicalScalar(NativePedPhysicalScalarField field, const char* operation) const;
        void        WritePhysicalScalar(NativePedPhysicalScalarField field, float value, const char* operation);
        // Native ownership stays with the enclosing runtime. Construction and
        // destruction neither acquire a reference nor retire native storage.
        NativePedMutationAccess& mutation;
        // Optional only for compatibility with already-built partial layers.
        // Absence refuses physical fields; it supplies no cached dynamics or
        // fabricated native capability. The enclosing owner retains lifetime.
        NativePedPhysicalScalarsAccess* const physicalScalars;
        std::atomic<ClockResult>              moveSpeedResult{ClockResult::NotInitialized};
        // This is MTA's client association, not a native object pointer or
        // reference lease. The caller retains its pointee's lifetime exactly
        // as in CEntitySA; destruction never dereferences or deletes it.
        void* storedPointer = nullptr;
    };
    static_assert(std::is_abstract_v<CPedMutationDE> && std::is_base_of_v<CPed, CPedMutationDE>);
}
