/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Dynamic Init only inside the real synchronous temporary-clump borrow
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationClumpTypes.h"
#include "NativeAnimationAssociationAccess.h"
namespace mta::game_de
{
    class NativeAnimationAssociationController;
    class NativePrivateAnimationHierarchyController;
    class NativeTemporaryModelClumpBorrow;
    namespace NativeTemporaryDynamicAssociationInitializeCalls
    {
        ClockResult Apply(std::uintptr_t image, NativeAnimationAssociationController&, std::uint64_t generation,
                          const NativeAnimationAssociationReferenceToken&, NativeAnimationAssociationClumpOperation, const NativeTemporaryModelClumpBorrow&,
                          std::uintptr_t hierarchy, NativePrivateAnimationHierarchyController&, ClockResult (*)() noexcept, std::uintptr_t& returned,
                          NativeAnimationAssociationCalls::ClumpReceipt* = nullptr,
                          const NativeAnimationAssociationCalls::ClumpInvocations* = nullptr) noexcept;
    }
}
