/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-ordered engine group retirement and causal array publication
 ****************************************************************************/
#pragma once
#include "NativeAnimationManagerMemory.h"
#include "NativeAnimationAssociationGroupChildBorrow.h"
#include "NativeStaticAssociationArrayStorageCalls.h"
#include <atomic>
#include <memory>
namespace mta::game_de
{
    struct NativeAnimationAssociationGroupReplacementAccess;
    class NativeAnimationAssociationGroupReplacement final
    {
    public:
        enum class State : std::uint32_t
        {
            Empty,
            RetireEntered,
            OldChildrenRetired,
            BlockStoreEntered,
            BlockPublished,
            ChildStoreEntered,
            ChildrenPublished,
            CountStoreEntered,
            Published,
            FaultDebt,
            Finished
        };
        NativeAnimationAssociationGroupReplacement() noexcept;
        ~NativeAnimationAssociationGroupReplacement();
        NativeAnimationAssociationGroupReplacement(const NativeAnimationAssociationGroupReplacement&) = delete;
        NativeAnimationAssociationGroupReplacement& operator=(const NativeAnimationAssociationGroupReplacement&) = delete;
        State                                       GetState() const noexcept { return state.load(std::memory_order_acquire); }
        bool                                        CanDiscard() const noexcept { return GetState() == State::Empty || GetState() == State::Finished; }
        // This is continuation/entered uncertainty, not a new private owner of
        // current engine rows. No CPU destructor frees a published child.
        bool IsRetained() const noexcept { return busy.load() || !CanDiscard(); }

    private:
        friend struct NativeAnimationAssociationGroupReplacementAccess;
        friend struct NativeStaticAssociationInitializeOwnershipAccess;
        // The joint initializer can consume only this actual accepted record,
        // under its caller's already-held native scope. No raw lookup exists.
        ClockResult ResolvePublishedRowForInitialization(std::uintptr_t, std::uint64_t, const NativeCustomAnimSequenceStorage&, std::uint32_t,
                                                         NativeAnimationManagerMemory::CheckOwner,
                                                         NativeAnimationStaticAssociationReferenceToken&) const noexcept;
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<State>    state{State::Empty};
        std::atomic<bool>     busy{false};
    };
    namespace NativeAnimationAssociationGroupReplacementCalls
    {
        using Record = NativeAnimationAssociationGroupReplacement;
        using CheckOwner = NativeAnimationManagerMemory::CheckOwner;
        using RetireFunction = void (*)(std::uintptr_t);
        using InvokeRetire = ClockResult (*)(RetireFunction, std::uintptr_t, std::uint32_t&) noexcept;
        using InvokeQword = ClockResult (*)(std::uintptr_t, std::uint64_t, std::uint64_t, std::uint64_t&, bool&, std::uint32_t&) noexcept;
        using InvokeDword = NativeAnimationManagerMemory::AssociationGroupOffsetExchange;
        struct StoreReceipt
        {
            std::uintptr_t address = 0;
            std::uint64_t  before = 0, target = 0, observed = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, returned = false, accepted = false, verified = false;
        };
        struct Receipt
        {
            std::uint64_t                                                   generation = 0;
            NativeAnimationManagerMemory::AssociationGroupReplacementLayout before{}, observed{};
            std::uintptr_t                                                  block = 0;
            std::uint32_t                                                   fullCount = 0, exceptionCode = 0;
            bool          retireEntered = false, retireReturned = false, oldChildrenConsumed = false, retireVerified = false, childBorrowsInvalidated = false;
            StoreReceipt  blockStore{}, childStore{}, countStore{};
            Record::State state = Record::State::Empty;
            ClockResult   result = ClockResult::NotInitialized;
        };
        ClockResult RetireCurrentChildren(std::uintptr_t, Record&, std::uint64_t, const NativeAnimationAssociationGroupReferenceToken&, CheckOwner,
                                          NativeAnimationAssociationGroupChildBorrowInvalidator = {}, Receipt* = nullptr, InvokeRetire = nullptr) noexcept;
        ClockResult PublishBlock(std::uintptr_t, Record&, std::uint64_t, const NativeAnimationBlockIdentity&, CheckOwner, Receipt* = nullptr,
                                 InvokeQword = nullptr) noexcept;
        ClockResult PublishChildren(std::uintptr_t, Record&, std::uint64_t, NativeCustomAnimSequenceStorage&, CheckOwner, Receipt* = nullptr,
                                    InvokeQword = nullptr) noexcept;
        ClockResult PublishFullCount(std::uintptr_t, Record&, std::uint64_t, CheckOwner, Receipt* = nullptr, InvokeDword = nullptr) noexcept;
        // Both actual CPU records must outlive the synchronous per-H loop.
        // Published native ownership does not discard that causal witness.
        ClockResult FinishPublished(std::uintptr_t, Record&, std::uint64_t, CheckOwner, Receipt* = nullptr) noexcept;
    }
}
