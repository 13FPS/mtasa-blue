/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Couple actual published static rows to one synchronous temporary borrow
 ****************************************************************************/
#include "NativeTemporaryStaticAssociationInitializeCalls.h"
#include "NativeStaticAssociationInitializeCore.h"
namespace mta::game_de
{
    struct NativeStaticAssociationInitializeOwnershipAccess
    {
        using Record = NativeTemporaryStaticAssociationInitialization;
        using State = Record::State;
        using Borrow = NativeTemporaryModelClumpBorrow;
        using Arguments = NativeStaticAssociationInitializeCalls::Arguments;
        using Receipt = NativeStaticAssociationInitializeCalls::Receipt;
        struct Context
        {
            std::uintptr_t                                    image = 0;
            std::uint64_t                                     generation = 0;
            const Borrow*                                     borrow = nullptr;
            const NativeAnimationAssociationGroupReplacement* group = nullptr;
            const NativeCustomAnimSequenceStorage*            storage = nullptr;
            std::uint32_t                                     index = 0;
            NativeAnimationHierarchyReferenceToken            hierarchy;
            NativeAnimationStaticAssociationReferenceToken    target;
            Record*                                           record = nullptr;
        };
        static ClockResult Recheck(const void* opaque) noexcept
        {
            const auto& c = *static_cast<const Context*>(opaque);
            if (!c.borrow || c.borrow->image != c.image || c.borrow->generation != c.generation)
                return ClockResult::OwnerConflict;
            auto result = c.borrow->RecheckOrigin();
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationStaticAssociationReferenceToken next;
            result = c.group->ResolvePublishedInitializationOrigin(c.image, c.generation, *c.storage, c.index, c.hierarchy, c.borrow->owner, next);
            if (result != ClockResult::Ok)
                return result;
            if (next.ownerGeneration != c.target.ownerGeneration || next.groupIndex != c.target.groupIndex || next.index != c.target.index ||
                next.locator != c.target.locator)
                return ClockResult::BindingChanged;
            return c.borrow->RecheckOrigin();
        }
        static ClockResult RecheckArguments(const Arguments& input, bool, void* opaque) noexcept
        {
            const auto& c = *static_cast<const Context*>(opaque);
            if (!c.borrow->record || input.clump != c.borrow->record->clump || input.association != c.target.locator || input.hierarchy != c.hierarchy.locator)
                return ClockResult::BindingChanged;
            return Recheck(opaque);
        }
        static ClockResult Claim(const void* opaque) noexcept
        {
            const auto& c = *static_cast<const Context*>(opaque);
            const auto  result = Recheck(opaque);
            return result != ClockResult::Ok ? result : c.borrow->ClaimInitialization();
        }
        static void Entered(const void* opaque) noexcept
        {
            const auto& c = *static_cast<const Context*>(opaque);
            c.record->state.store(State::Entered, std::memory_order_release);
        }
        static ClockResult Initialize(std::uintptr_t image, Record& record, std::uint64_t generation, const Borrow& borrow,
                                      const NativeAnimationAssociationGroupReplacement& group, const NativeCustomAnimSequenceStorage& storage,
                                      std::uint32_t index, const NativeAnimationHierarchyReferenceToken& requested, Receipt* output,
                                      NativeStaticAssociationInitializeCalls::Invoke invoke) noexcept
        {
            Receipt receipt;
            if (output)
                *output = receipt;
            if (!image || !generation || borrow.image != image || borrow.generation != generation)
                return ClockResult::OwnerConflict;
            auto result = borrow.RecheckOrigin();
            if (result != ClockResult::Ok)
                return result;
            Context context;
            context.image = image;
            context.generation = generation;
            context.borrow = &borrow;
            context.group = &group;
            context.storage = &storage;
            context.index = index;
            context.hierarchy = requested;
            context.record = &record;
            result = group.ResolvePublishedInitializationOrigin(image, generation, storage, index, context.hierarchy, borrow.owner, context.target);
            if (result != ClockResult::Ok)
                return result;
            if ((result = Recheck(&context)) != ClockResult::Ok)
                return result;
            auto expected = State::Empty;
            if (!record.state.compare_exchange_strong(expected, State::Preparing, std::memory_order_acq_rel))
                return ClockResult::Busy;
            Arguments input;
            input.clump = borrow.record->clump;
            input.association = context.target.locator;
            input.hierarchy = context.hierarchy.locator;
            NativeStaticAssociationTemporaryOrigin origin;
            origin.context = &context;
            origin.recheck = &Recheck;
            origin.claim = &Claim;
            origin.entered = &Entered;
            origin.model = borrow.record->model;
            origin.image = image;
            origin.input = input;
            origin.owner = borrow.owner;
            origin.recheckArguments = &RecheckArguments;
            // WithTemporary owns the one actual scope. The unconstructible
            // active borrow, exact published record and single-use claim close
            // this entry; calling the public ped wrapper here would nest Busy.
            result = NativeStaticAssociationInitializeCoreAccess::Initialize(origin, &receipt, invoke);
            // Capture the actual per-call receipt before any further owner work.
            // Engine-group lifetime owns accepted nodes, never this CPU journal.
            record.receipt = receipt;
            record.state.store(result == ClockResult::Ok && receipt.verified ? State::Verified
                               : receipt.entered                             ? State::FaultDebt
                                                                             : State::Empty,
                               std::memory_order_release);
            if (output)
                *output = receipt;
            return result;
        }
    };
    namespace NativeTemporaryStaticAssociationInitializeCalls
    {
        ClockResult Initialize(std::uintptr_t image, NativeTemporaryStaticAssociationInitialization& record, std::uint64_t generation,
                               const NativeTemporaryModelClumpBorrow& borrow, const NativeAnimationAssociationGroupReplacement& group,
                               const NativeCustomAnimSequenceStorage& storage, std::uint32_t index, const NativeAnimationHierarchyReferenceToken& hierarchy,
                               Receipt* output, Invoke invoke) noexcept
        {
            return NativeStaticAssociationInitializeOwnershipAccess::Initialize(image, record, generation, borrow, group, storage, index, hierarchy, output,
                                                                                invoke);
        }
    }
}
