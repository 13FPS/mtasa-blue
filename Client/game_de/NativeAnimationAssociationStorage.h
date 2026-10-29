/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exactly-once private bank-backed dynamic animation ownership
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationAccess.h"
#include "NativeAnimationHierarchyAccess.h"
#include "NativeAnimationAssociationGroupAccess.h"
#include "NativeCustomAnimSequenceStorage.h"
#include <atomic>
#include <memory>
namespace mta::game_de
{
    struct NativeAnimationAssociationStorageAccess;
    class NativeAnimationAssociationStorage final
    {
    public:
        enum class State : std::uint32_t
        {
            Empty,
            Unconstructed,
            ConstructEntered,
            LiveUnpublished,
            RetireEntered,
            Released,
            FaultDebt,
            PublicationPinned,
            NodeFreeEntered,
            NodesReleased,
            NodeClearEntered,
            CopyEntered
        };
        NativeAnimationAssociationStorage() noexcept;
        ~NativeAnimationAssociationStorage();
        NativeAnimationAssociationStorage(const NativeAnimationAssociationStorage&) = delete;
        NativeAnimationAssociationStorage& operator=(const NativeAnimationAssociationStorage&) = delete;
        State                              GetState() const noexcept { return state.load(std::memory_order_acquire); }
        bool                               CanDiscard() const noexcept { return GetState() == State::Empty || GetState() == State::Released; }
        bool                               IsRetained() const noexcept { return !CanDiscard(); }

    private:
        friend struct NativeAnimationAssociationStorageAccess;
        struct Data;
        std::unique_ptr<Data>      data;
        mutable std::atomic<State> state{State::Empty};
    };
    namespace NativeAnimationAssociationCalls
    {
        using Storage = NativeAnimationAssociationStorage;
        using CheckOwner = ClockResult (*)() noexcept;
        enum class Scalar : std::uint8_t
        {
            BlendAmount,
            CurrentSpeed,
            AnimID,
            AnimGroup,
            Flags
        };
        struct GroupCopyReceipt;
        using GroupCopyFunction = std::uintptr_t (*)(std::int32_t, std::uint32_t);
        using InvokeGroupCopy = ClockResult (*)(GroupCopyFunction, std::int32_t, std::uint32_t, std::uintptr_t&, std::uint32_t&) noexcept;
        using ConstructorFunction = std::uintptr_t (*)(std::uintptr_t, std::uintptr_t);
        using ProgressFunction = void (*)(std::uintptr_t, float);
        using InvokeProgress = ClockResult (*)(ProgressFunction, std::uintptr_t, float, std::uint32_t&) noexcept;
        using DestructorFunction = std::uintptr_t (*)(std::uintptr_t, std::uint32_t);
        using InvokeConstructor = ClockResult (*)(ConstructorFunction, std::uintptr_t, std::uintptr_t, std::uintptr_t&, std::uint32_t&) noexcept;
        using InvokeDestructor = ClockResult (*)(DestructorFunction, std::uintptr_t, std::uint32_t, std::uintptr_t&, std::uint32_t&) noexcept;
        using Exchange = ClockResult (*)(std::uintptr_t, std::size_t, std::uint32_t, std::uint32_t, std::uint32_t&, bool&, std::uint32_t&) noexcept;
        using ExchangeNode = ClockResult (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t&, bool&, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t receiver = 0, nodes = 0, nodeRaw = 0, heap = 0, hierarchy = 0;
            std::size_t    nodeRequestBytes = 0;
            std::uint16_t  nodeCount = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, returned = false, accepted = false, verified = false, allocated = false, constructed = false, retired = false,
                 refVerified = false, pinned = false;
            std::uint32_t visitedNodes = 0, attemptedNodes = 0, acceptedNodes = 0;
            bool          noOp = false;
            std::uint32_t progressBits = 0, absoluteTimeBits = 0;
            std::size_t   preparedFrames = 0, modifiedFrames = 0;
            bool          progressed = false, progressUnknown = false;
            // A returned node-only free does not clear native pointer/count.
            // Retirement has a separate causal pointer-clear acknowledgement.
            std::uintptr_t nodeFreeEntryRva = 0;
            bool           nodeFreeEntered = false, nodesReleased = false, nodeFreeVerified = false;
            bool           nodePointerClearEntered = false, nodePointerCleared = false, nodePointerClearVerified = false;
        };
        ClockResult CopyFromGroup(std::uintptr_t, Storage&, const NativeAnimationAssociationReferenceToken&,
                                  const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t, CheckOwner, std::uintptr_t&, GroupCopyReceipt* = nullptr,
                                  InvokeGroupCopy = nullptr) noexcept;
        ClockResult Create(std::uintptr_t, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr,
                           NativeCustomAnimSequenceStorageCalls::InvokeMalloc = nullptr) noexcept;
        ClockResult Construct(std::uintptr_t, Storage&, std::uint64_t, std::uintptr_t, CheckOwner, std::uintptr_t&, Receipt* = nullptr,
                              InvokeConstructor = nullptr) noexcept;
        ClockResult Read(std::uintptr_t, const Storage&, std::uint64_t, CheckOwner, NativeAnimationAssociationValues&) noexcept;
        // Duration is a separate joint bank-H observation. Ordinary receiver
        // metadata does not acquire this extra field/profile dependency.
        ClockResult ReadLength(std::uintptr_t, const Storage&, std::uint64_t, CheckOwner, std::uint32_t&) noexcept;
        ClockResult Set(std::uintptr_t, Storage&, std::uint64_t, Scalar, std::uint32_t, CheckOwner, Receipt* = nullptr, Exchange = nullptr) noexcept;
        // Progress calls the genuine time helper after a bounded payload plan;
        // an unknown entered transition is never replayed or repaired by copying.
        ClockResult SetCurrentProgress(std::uintptr_t, Storage&, std::uint64_t, std::uint32_t, CheckOwner, Receipt* = nullptr,
                                       InvokeProgress = nullptr) noexcept;
        ClockResult FreeNodeArray(std::uintptr_t, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr,
                                  NativeCustomAnimSequenceStorageCalls::InvokeFree = nullptr) noexcept;
        ClockResult Retire(std::uintptr_t, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr, InvokeDestructor = nullptr,
                           NativeCustomAnimSequenceStorageCalls::InvokeFree = nullptr, ExchangeNode = nullptr) noexcept;
        // A hierarchy token remains a current borrowed bank identity, never
        // another reference or storage-release authority. Restrictions preserve
        // the immutable original map and acknowledge only null suppression.
        ClockResult ResolveHierarchy(std::uintptr_t, const Storage&, std::uint64_t, CheckOwner, NativeAnimationHierarchyReferenceToken&) noexcept;
        ClockResult RestrictToBones(std::uintptr_t, Storage&, std::uint64_t, std::uint64_t, CheckOwner, Receipt* = nullptr, ExchangeNode = nullptr) noexcept;
        ClockResult RestrictToBonesOf(std::uintptr_t, Storage&, std::uint64_t, std::uintptr_t, CheckOwner, Receipt* = nullptr, ExchangeNode = nullptr) noexcept;
        // No destructor invokes cleanup. An entered unknown constructor or
        // retirement keeps debt; rawpointer/4000/null are never release ACKs.
    }
}
