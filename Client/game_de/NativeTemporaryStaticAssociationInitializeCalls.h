/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One original static Init from closed published-row/temp-clump origins
 ****************************************************************************/
#pragma once
#include "NativeStaticAssociationInitializeCalls.h"
#include "NativeTemporaryModelClump.h"
#include "NativeAnimationAssociationGroupReplacement.h"
#include "NativeAnimationHierarchyAccess.h"
#include <atomic>
namespace mta::game_de
{
    struct NativeStaticAssociationInitializeOwnershipAccess;
    class NativeTemporaryStaticAssociationInitialization final
    {
    public:
        enum class State : std::uint32_t
        {
            Empty,
            Preparing,
            Entered,
            Verified,
            FaultDebt
        };
        NativeTemporaryStaticAssociationInitialization() noexcept = default;
        NativeTemporaryStaticAssociationInitialization(const NativeTemporaryStaticAssociationInitialization&) = delete;
        NativeTemporaryStaticAssociationInitialization& operator=(const NativeTemporaryStaticAssociationInitialization&) = delete;
        // Engine-group lifetime owns resulting static nodes. Unknown original
        // entry keeps this journal and the actual group/temp records alive;
        // destruction never frees nodes or fabricates rollback.
        ~NativeTemporaryStaticAssociationInitialization() = default;
        State GetState() const noexcept { return state.load(std::memory_order_acquire); }
        bool  CanDiscard() const noexcept { return GetState() == State::Empty || GetState() == State::Verified; }
        bool  HasOutstandingResource() const noexcept { return !CanDiscard(); }

    private:
        friend struct NativeStaticAssociationInitializeOwnershipAccess;
        std::atomic<State>                              state{State::Empty};
        NativeStaticAssociationInitializeCalls::Receipt receipt{};
    };
    namespace NativeTemporaryStaticAssociationInitializeCalls
    {
        using Receipt = NativeStaticAssociationInitializeCalls::Receipt;
        using Invoke = NativeStaticAssociationInitializeCalls::Invoke;
        // This is valid only inside WithTemporary's actual synchronous callback.
        // The nonconstructible borrow and both retained CPU origin records must
        // outlive the call. It acquires no second native gate or pointer lease.
        ClockResult Initialize(std::uintptr_t image, NativeTemporaryStaticAssociationInitialization&, std::uint64_t generation,
                               const NativeTemporaryModelClumpBorrow&, const NativeAnimationAssociationGroupReplacement&,
                               const NativeCustomAnimSequenceStorage&, std::uint32_t index, const NativeAnimationHierarchyReferenceToken&, Receipt* = nullptr,
                               Invoke = nullptr) noexcept;
    }
}
