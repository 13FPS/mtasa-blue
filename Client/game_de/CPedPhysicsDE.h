/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared MTA getter policy with a mandatory genuine native repair
 ****************************************************************************/
#pragma once
#include "CPedMutationDE.h"
#include "NativePedObservationAccess.h"
#include <game/CEntityPhysicsState.h>

namespace mta::game_de
{
    // This layer cannot be a usable SDK ped: a future complete owner must
    // implement all native effects of the pure repair hook as well as the
    // remaining SDK methods. Raw observations alone never satisfy that hook.
    class CPedPhysicsDE : public CPedMutationDE
    {
    public:
        CVector* GetPosition() override;
        CMatrix* GetMatrix(CMatrix* matrix) override;
        CVector* GetMoveSpeed(CVector* velocity) override;
        CVector* GetTurnSpeed(CVector* velocity) override;

    protected:
        CPedPhysicsDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                      NativePedObservationAccess& observation, const NativePedReferenceToken& token) noexcept
            : CPedMutationDE(vitals, metadata, mutation, token), observation(observation)
        {
        }
        // Required order: genuine SetMatrix/SetPosition placement + world/RW
        // effects, linear/angular zeroing, then the actual physical collision
        // reset fields. Native teleport is not a substitute for this contract.
        // Any entered failure must preserve effects and stop without replay.
        virtual ClockResult RepairLastGoodPhysicsState(const CMatrix& repairMatrix) noexcept = 0;
        // Future qualified position/matrix setters must remember at the same
        // classic commit points; ordinary velocity setters do not call this.
        void RememberPhysicsPosition(const CVector& position) noexcept { physics.RememberPosition(position); }

    private:
        static void                  Require(ClockResult, const char* operation);
        void                         Repair();
        CVector*                     ReadPosition();
        CMatrix*                     ReadMatrix(CMatrix* output, bool& requireMatrix);
        void                         ReadVelocity(NativePedVelocityKind, CVector* output);
        NativePedObservationAccess&  observation;
        MtaGame::CEntityPhysicsState physics;
    };
    static_assert(std::is_abstract_v<CPedPhysicsDE> && std::is_base_of_v<CPed, CPedPhysicsDE>);
}
