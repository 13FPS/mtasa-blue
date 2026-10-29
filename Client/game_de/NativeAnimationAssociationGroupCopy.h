/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Causal original-copy receipt, never public raw-pointer adoption
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationStorage.h"
#include "NativeAnimationAssociationGroupAccess.h"
namespace mta::game_de
{
    namespace NativeAnimationAssociationCalls
    {
        struct GroupCopyReceipt
        {
            NativeAnimationAssociationReferenceToken       issued{};
            NativeAnimationAssociationGroupReferenceToken  group{};
            NativeAnimationStaticAssociationReferenceToken selected{};
            std::uint32_t                                  animationId = 0, rawIdOffset = 0;
            std::uintptr_t                                 entryRva = 0xff1120, returnedInterface = 0;
            bool    sourcePrepared = false, hierarchyWarm = false, entered = false, returned = false, returnedNull = false, verified = false;
            Receipt resources{};
        };
    }
    // Only the existing storage owner can issue this ticket after its exact
    // FF1120 call returns. A copied public receipt cannot adopt an allocation.
    class NativeAnimationAssociationCopyReturnTicket final
    {
        friend struct NativeAnimationAssociationStorageAccess;
        friend class NativeCustomAnimSequenceStorage;
        NativeAnimationAssociationCopyReturnTicket(NativeCustomAnimSequenceStorage& targetOuter, NativeAnimationAssociationReferenceToken request,
                                                   NativeModelReferenceHeap::Value capturedHeap, std::uintptr_t returned) noexcept
            : target(&targetOuter), issued(request), heap(capturedHeap), receiver(returned)
        {
        }
        NativeCustomAnimSequenceStorage*         target;
        NativeAnimationAssociationReferenceToken issued;
        NativeModelReferenceHeap::Value          heap;
        std::uintptr_t                           receiver;
    };
}
