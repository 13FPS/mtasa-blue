/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the same active temporary origin and single original claim
 ****************************************************************************/
#include "NativeTemporaryDynamicAssociationInitializeCalls.h"
#include "NativeAnimationAssociationClumpOrigin.h"
namespace mta::game_de::NativeTemporaryDynamicAssociationInitializeCalls
{
    ClockResult Apply(std::uintptr_t image, NativeAnimationAssociationController& controller, std::uint64_t generation,
                      const NativeAnimationAssociationReferenceToken& receiver, NativeAnimationAssociationClumpOperation operation,
                      const NativeTemporaryModelClumpBorrow& borrow, std::uintptr_t hierarchy, NativePrivateAnimationHierarchyController& hierarchies,
                      ClockResult (*owner)() noexcept, std::uintptr_t& returned, NativeAnimationAssociationCalls::ClumpReceipt* receipt,
                      const NativeAnimationAssociationCalls::ClumpInvocations* invocations) noexcept
    {
        return NativeAnimationAssociationClumpOwnershipAccess::ApplyTemporary(image, controller, generation, receiver, operation, borrow, hierarchy,
                                                                              hierarchies, owner, returned, receipt, invocations);
    }
}
