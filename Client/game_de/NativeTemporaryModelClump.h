/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One causal model-clump itinerary inside an existing native callback
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include <atomic>
#include <cstdint>
namespace mta::game_de
{
    struct NativeTemporaryModelClumpAccess;
    struct NativeStaticAssociationInitializeOwnershipAccess;
    struct NativeAnimationAssociationClumpOwnershipAccess;
    namespace NativeStreamingCompletionContext
    {
        struct Value;
    }
    namespace NativeStreamingModelContext
    {
        struct Value;
    }
    class NativeTemporaryModelClumpBorrow;
    class NativeTemporaryModelClump final
    {
    public:
        enum class State : std::uint32_t
        {
            Empty,
            CreateEntered,
            ReturnedNull,
            LiveUnprepared,
            SetupEntered,
            SetupReturned,
            InitializeEntered,
            Prepared,
            RowEntered,
            RowReturned,
            RetireEntered,
            RetiredDeferred,
            FaultDebt
        };
        NativeTemporaryModelClump() noexcept = default;
        NativeTemporaryModelClump(const NativeTemporaryModelClump&) = delete;
        NativeTemporaryModelClump& operator=(const NativeTemporaryModelClump&) = delete;
        NativeTemporaryModelClump(NativeTemporaryModelClump&&) = delete;
        NativeTemporaryModelClump& operator=(NativeTemporaryModelClump&&) = delete;
        // A CPU destructor never retires a UE actor. Entered unknown outcomes
        // keep this record and its real runtime/module owner alive.
        ~NativeTemporaryModelClump() = default;
        State GetState() const noexcept { return state.load(std::memory_order_acquire); }
        bool  CanDiscard() const noexcept { return !outstanding.load(std::memory_order_acquire); }
        bool  HasOutstandingResource() const noexcept { return !CanDiscard(); }

    private:
        friend struct NativeTemporaryModelClumpAccess;
        friend struct NativeStaticAssociationInitializeOwnershipAccess;
        friend struct NativeAnimationAssociationClumpOwnershipAccess;
        std::atomic<State>                     state{State::Empty};
        std::atomic<bool>                      outstanding{false};
        std::uint64_t                          generation = 0;
        NativeModelBankReader::Value           model{};
        std::uintptr_t                         clump = 0, container = 0;
        const NativeTemporaryModelClumpBorrow* activeBorrow = nullptr;
    };
    // Only the synchronous row adapter can consume this noncopyable borrow.
    // A public receipt/raw actor address is never an alternate admission path.
    class NativeTemporaryModelClumpBorrow final
    {
    public:
        NativeTemporaryModelClumpBorrow(const NativeTemporaryModelClumpBorrow&) = delete;
        NativeTemporaryModelClumpBorrow& operator=(const NativeTemporaryModelClumpBorrow&) = delete;

    private:
        friend struct NativeTemporaryModelClumpAccess;
        friend struct NativeStaticAssociationInitializeOwnershipAccess;
        friend struct NativeAnimationAssociationClumpOwnershipAccess;
        NativeTemporaryModelClumpBorrow() noexcept = default;
        // The joint initializer borrows the original synchronous snapshots;
        // it cannot replace them with a raw actor or extend their lifetime.
        struct Context;
        ClockResult                                    RecheckOrigin() const noexcept;
        ClockResult                                    ClaimInitialization() const noexcept;
        const Context*                                 origin = nullptr;
        mutable bool                                   initializationClaimed = false;
        NativeTemporaryModelClump*                     record = nullptr;
        std::uintptr_t                                 image = 0;
        std::uint64_t                                  generation = 0;
        NativeModelBankReader::CheckOwner              owner = nullptr;
        const NativeStreamingCompletionContext::Value* world = nullptr;
        const NativeStreamingModelContext::Value*      modelContext = nullptr;
    };
    namespace NativeTemporaryModelClumpCalls
    {
        using CheckOwner = NativeModelBankReader::CheckOwner;
        using CreateFunction = std::uintptr_t (*)(std::uintptr_t, std::uintptr_t, std::uint32_t, std::int32_t, std::uintptr_t);
        using SetupFunction = void (*)(std::uintptr_t, std::uintptr_t, std::uint32_t, std::uint32_t);
        using PrepareFunction = void (*)(std::uintptr_t, std::uintptr_t);
        using RetireFunction = bool (*)(std::uintptr_t, bool);
        using InvokeCreate = ClockResult (*)(CreateFunction, std::uintptr_t, std::uintptr_t, std::uint32_t, std::int32_t, std::uintptr_t, std::uintptr_t&,
                                             std::uint32_t&) noexcept;
        using InvokeSetup = ClockResult (*)(SetupFunction, std::uintptr_t, std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&) noexcept;
        using InvokePrepare = ClockResult (*)(PrepareFunction, std::uintptr_t, std::uintptr_t, std::uint32_t&) noexcept;
        using InvokeRetire = ClockResult (*)(RetireFunction, std::uintptr_t, bool, bool&, std::uint32_t&) noexcept;
        struct Invocations
        {
            InvokeCreate  create = nullptr;
            InvokeSetup   setup = nullptr;
            InvokePrepare prepare = nullptr;
            InvokeRetire  retire = nullptr;
        };
        using RowInitializer = ClockResult (*)(const NativeTemporaryModelClumpBorrow&, void*) noexcept;
        struct Receipt
        {
            NativeTemporaryModelClump::State phase{NativeTemporaryModelClump::State::Empty};
            NativeModelBankReader::Value     model{};
            std::uint64_t                    generation = 0;
            std::uintptr_t                   createEntry = 0, opaqueDefault = 0, clump = 0, container = 0;
            std::uintptr_t                   setupClass = 0, actorClass = 0;
            std::uint16_t                    modelFlags = 0, frameCount = 0;
            std::uint32_t                    exceptionCode = 0;
            ClockResult                      rowResult{ClockResult::NotInitialized};
            bool                             createEntered = false, createReturned = false, returnedNull = false;
            bool                             setupRequired = false, setupEntered = false, setupReturned = false;
            bool                             prepareEntered = false, prepareReturned = false, containerObserved = false;
            bool                             rowEntered = false, rowReturned = false;
            bool                             retireEntered = false, retireReturned = false, retireAccepted = false;
            bool                             verified = false, debt = false;
        };
        // The typed row adapter must already own its actual row/H through its
        // own issued storage or engine-group record. This foundation grants no
        // row authority and does not implement that adapter. All four native
        // entries and the adapter execute under ONE current owner/world scope;
        // no model/actor lease is manufactured between callbacks.
        ClockResult WithTemporary(std::uintptr_t image, NativeTemporaryModelClump&, std::uint64_t generation, const NativeModelBankReader::Value&, CheckOwner,
                                  RowInitializer, void* context, Receipt* = nullptr, const Invocations* = nullptr) noexcept;
    }
}
