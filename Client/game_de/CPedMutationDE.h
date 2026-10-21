/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual inherited ped vector setters through one registered token
 ****************************************************************************/
#pragma once
#include "CPedMetadataDE.h"
#include "NativePedMutationAccess.h"
#include <atomic>

namespace mta::game_de
{
    // The classic getters repair invalid physics through last-good wrapper
    // state. That policy is not supplied here, so getters and the remaining
    // entity/ped methods stay pure rather than returning unchecked raw fields.
    class CPedMutationDE : public CPedMetadataDE
    {
    public:
        void SetMoveSpeed(const CVector& velocity) noexcept override;
        void SetTurnSpeed(CVector* velocity) override;

        ClockResult TrySetMoveSpeed(const CVector& velocity) noexcept;
        ClockResult LastMoveSpeedResult() const noexcept { return moveSpeedResult.load(); }

    protected:
        CPedMutationDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                       const NativePedReferenceToken& token) noexcept
            : CPedMetadataDE(vitals, metadata, token), mutation(mutation)
        {
        }

    private:
        static void Require(ClockResult result, const char* operation);
        // Native ownership stays with the enclosing runtime. Construction and
        // destruction neither acquire a reference nor retire native storage.
        NativePedMutationAccess& mutation;
        std::atomic<ClockResult> moveSpeedResult{ClockResult::NotInitialized};
    };
    static_assert(std::is_abstract_v<CPedMutationDE> && std::is_base_of_v<CPed, CPedMutationDE>);
}
