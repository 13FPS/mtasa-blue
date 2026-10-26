/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exactly-once private bank-backed dynamic animation ownership
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationAccess.h"
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
            PublicationPinned
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
        using ConstructorFunction = std::uintptr_t (*)(std::uintptr_t, std::uintptr_t);
        using DestructorFunction = std::uintptr_t (*)(std::uintptr_t, std::uint32_t);
        using InvokeConstructor = ClockResult (*)(ConstructorFunction, std::uintptr_t, std::uintptr_t, std::uintptr_t&, std::uint32_t&) noexcept;
        using InvokeDestructor = ClockResult (*)(DestructorFunction, std::uintptr_t, std::uint32_t, std::uintptr_t&, std::uint32_t&) noexcept;
        using Exchange = ClockResult (*)(std::uintptr_t, std::size_t, std::uint32_t, std::uint32_t, std::uint32_t&, bool&, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t receiver = 0, nodes = 0, nodeRaw = 0, heap = 0, hierarchy = 0;
            std::size_t    nodeRequestBytes = 0;
            std::uint16_t  nodeCount = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, returned = false, accepted = false, verified = false, allocated = false, constructed = false, retired = false,
                 refVerified = false, pinned = false;
        };
        ClockResult Create(std::uintptr_t, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr,
                           NativeCustomAnimSequenceStorageCalls::InvokeMalloc = nullptr) noexcept;
        ClockResult Construct(std::uintptr_t, Storage&, std::uint64_t, std::uintptr_t, CheckOwner, std::uintptr_t&, Receipt* = nullptr,
                              InvokeConstructor = nullptr) noexcept;
        ClockResult Read(std::uintptr_t, const Storage&, std::uint64_t, CheckOwner, NativeAnimationAssociationValues&) noexcept;
        ClockResult Set(std::uintptr_t, Storage&, std::uint64_t, Scalar, std::uint32_t, CheckOwner, Receipt* = nullptr, Exchange = nullptr) noexcept;
        ClockResult Retire(std::uintptr_t, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr, InvokeDestructor = nullptr,
                           NativeCustomAnimSequenceStorageCalls::InvokeFree = nullptr) noexcept;
        // No destructor invokes cleanup. An entered unknown constructor or
        // retirement keeps debt; rawpointer/4000/null are never release ACKs.
    }
}
