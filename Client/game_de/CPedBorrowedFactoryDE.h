/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit typed wrapper lifetime over one existing native ped token
 ****************************************************************************/
#pragma once
#include "CPoolsDE.h"
#include "CPedVitalsDE.h"
#include "NativeGameTimingAccess.h"
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>

namespace mta::game_de
{
    // This is an internal enclosing-factory seam, not AddPed, a player factory,
    // or a native ped allocator. The caller must provide a genuinely complete
    // concrete SDK implementation. No rejecting implementation is supplied by
    // production, and the wrapper never owns/deletes the existing native ped.
    //
    // Pool and its native access/controller outlive this object. The enclosing
    // owner serializes all SDK borrows, mutable row edits and retirement. Native
    // operations still require that controller's real admitted callback; the
    // acquired CPU token is neither a pointer lease nor independent authority.
    template <class ConcretePed>
    class CPedBorrowedFactoryDE final
    {
        static_assert(std::is_base_of_v<CPedVitalsDE, ConcretePed> && !std::is_abstract_v<ConcretePed>,
                      "a borrowed ped factory requires a complete concrete DE SDK wrapper");
        static_assert(std::is_nothrow_destructible_v<ConcretePed>, "typed wrapper retirement must not throw");

    public:
        enum class Phase
        {
            Fresh,
            Acquiring,
            Constructing,
            Registering,
            Live,
            Withdrawing,
            Retired
        };
        struct Receipt
        {
            Phase                   phase = Phase::Fresh;
            std::uint32_t           handle = 0;
            NativePedReferenceToken reference{};
            ClockResult             result = ClockResult::Ok, withdrawal = ClockResult::NotInitialized, release = ClockResult::NotInitialized;
            bool acquired = false, constructed = false, registered = false, constructionThrew = false, wrapperDestroyed = false, releaseAttempted = false;
        };

        explicit CPedBorrowedFactoryDE(CPoolsDE& pool) noexcept : poolRegistry(pool) {}
        ~CPedBorrowedFactoryDE()
        {
            // A destructor has no natural-frame admission and cannot know
            // whether a failed Release entered native cleanup. Require the
            // explicit lifecycle instead of retrying native work, deleting a
            // still-linked wrapper, or silently discarding an issued token.
            // Failed acquisition debt and a failed one-shot Release remain
            // owned by the existing controller's documented End retirement.
            if (wrapper || registration.IsBound() || (state.acquired && !state.releaseAttempted))
                std::terminate();
        }
        CPedBorrowedFactoryDE(const CPedBorrowedFactoryDE&) = delete;
        CPedBorrowedFactoryDE& operator=(const CPedBorrowedFactoryDE&) = delete;
        CPedBorrowedFactoryDE(CPedBorrowedFactoryDE&&) = delete;
        CPedBorrowedFactoryDE& operator=(CPedBorrowedFactoryDE&&) = delete;

        // ConcretePed(owner, token, args...) constructs a new typed CPU wrapper
        // for this exact acquired identity. Construction is CPU-only and must
        // not publish a registry link, create/release native cells, or adopt
        // somebody else's SDK/native object. Its destructor must likewise be
        // the borrowed-wrapper destructor, not a native ped deletion route.
        // C++ construction failures are rethrown unchanged after one explicit
        // cleanup attempt; they are not relabeled native OriginalException.
        template <class... Args>
        ClockResult Create(std::uint32_t handle, CClientEntity* client, Args&&... args)
        {
            Operation operation(inOperation);
            if (!operation || state.phase != Phase::Fresh)
                return ClockResult::Busy;
            state.phase = Phase::Acquiring;
            state.handle = handle;
            auto& owner = poolRegistry.pedRegistryOwner;
            CPed* existing = nullptr;
            auto  result = poolRegistry.ReadPedFromRef(handle, existing);
            if (result != ClockResult::Ok)
                return RefuseBeforeAcquire(result);
            if (existing)
                return RefuseBeforeAcquire(ClockResult::Busy);

            bool                       present = false;
            NativePedPoolReader::Value before, acquired;
            result = owner.ProbePedPoolHandle(handle, present, before);
            if (result != ClockResult::Ok)
                return RefuseBeforeAcquire(result);
            if (!present)
                return RefuseBeforeAcquire(ClockResult::NotInitialized);
            result = owner.AcquirePedReference(handle, state.reference);
            if (result != ClockResult::Ok)
                return RefuseBeforeAcquire(result);
            state.acquired = true;
            result = owner.ResolvePedReference(state.reference, acquired);
            if (result != ClockResult::Ok)
                return Abort(result);
            if (!CPoolsDE::Same(before, acquired))
                return Abort(ClockResult::BindingChanged);
            state.phase = Phase::Constructing;
            try
            {
                wrapper = std::make_unique<ConcretePed>(owner, state.reference, std::forward<Args>(args)...);
                state.constructed = true;
            }
            catch (...)
            {
                state.constructionThrew = true;
                RetireNow();
                throw;
            }
            if (!poolRegistry.HasExactBorrowedPedBinding(*wrapper, state.reference))
                return Abort(ClockResult::OwnerConflict);
            state.phase = Phase::Registering;
            result = poolRegistry.BindBorrowedPed(*wrapper, client, registration);
            if (result != ClockResult::Ok)
                return Abort(result);
            state.registered = true;
            state.phase = Phase::Live;
            return ClockResult::Ok;
        }

        // Only the registry can publish this object after fresh native/token
        // checks. There is deliberately no unchecked cached-pointer getter.
        ClockResult Read(CPed*& output) noexcept
        {
            Operation operation(inOperation);
            if (!operation)
                return ClockResult::Busy;
            if (state.phase != Phase::Live || !wrapper)
                return ClockResult::NotInitialized;
            CPed*      current = nullptr;
            const auto result = poolRegistry.ReadPedFromRef(state.handle, current);
            if (result != ClockResult::Ok)
                return result;
            if (current != wrapper.get())
                return ClockResult::BindingChanged;
            output = current;
            return ClockResult::Ok;
        }

        // This may be called again only to finish a refused CPU withdrawal.
        // Once native Release is attempted it is permanently one-shot, even
        // when the owner refuses before returning any entered/ack receipt.
        ClockResult Retire() noexcept
        {
            Operation operation(inOperation);
            return operation ? RetireNow() : ClockResult::Busy;
        }
        const Receipt& ReadReceipt() const noexcept { return state; }

    private:
        class Operation final
        {
        public:
            explicit Operation(bool& flag) noexcept : activeFlag(flag), held(!flag)
            {
                if (held)
                    activeFlag = true;
            }
            ~Operation()
            {
                if (held)
                    activeFlag = false;
            }
            explicit operator bool() const noexcept { return held; }

        private:
            bool&      activeFlag;
            const bool held;
        };
        ClockResult RefuseBeforeAcquire(ClockResult result) noexcept
        {
            state.result = result;
            state.phase = Phase::Retired;
            return result;
        }
        ClockResult Abort(ClockResult result) noexcept
        {
            state.result = result;
            RetireNow();
            return result;
        }
        ClockResult RetireNow() noexcept
        {
            if (state.phase == Phase::Retired)
                return state.result;
            state.phase = Phase::Withdrawing;
            const auto withdrawn = registration.Reset();
            if (state.withdrawal == ClockResult::NotInitialized || state.withdrawal == ClockResult::Ok)
                state.withdrawal = withdrawn;
            if (state.result == ClockResult::Ok && withdrawn != ClockResult::Ok)
                state.result = withdrawn;
            if (wrapper && poolRegistry.HasBorrowedPedLink(*wrapper))
            {
                // Public SDK pairs are mutable. A changed client association
                // can still name our original pEntity after guard withdrawal.
                // Keep the typed object and its token until that association
                // is removed; never clear somebody else's pair or dangle it.
                if (state.result == ClockResult::Ok)
                    state.result = ClockResult::BindingChanged;
                return ClockResult::BindingChanged;
            }
            if (wrapper)
            {
                wrapper.reset();
                state.wrapperDestroyed = true;
            }
            if (state.acquired && !state.releaseAttempted)
            {
                state.releaseAttempted = true;
                state.release = poolRegistry.pedRegistryOwner.ReleasePedReference(state.reference);
                if (state.result == ClockResult::Ok && state.release != ClockResult::Ok)
                    state.result = state.release;
            }
            state.phase = Phase::Retired;
            return state.result;
        }
        CPoolsDE&                         poolRegistry;
        std::unique_ptr<ConcretePed>      wrapper;
        CPoolsDE::BorrowedPedRegistration registration;
        Receipt                           state{};
        bool                              inOperation = false;
    };
}
