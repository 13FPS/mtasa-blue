/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the original temporary model-clump creation/retirement path
 ****************************************************************************/
#include "NativeTemporaryModelClump.h"
#include "NativeTemporaryModelClumpProfile.h"
#include "NativeClassChainAccess.h"
#include "NativeModelReferenceMap.h"
// Vtable qualification reuses the shared native heap image policy.
#include "NativeModelReferenceHeap.h"
#include "NativeStaticAssociationInitializeProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeStartGameCalls.h"
#include <array>
#include <cstring>
namespace mta::game_de
{
    namespace TemporaryClumpDetail
    {
        namespace P = NativeTemporaryModelClumpProfile;
        namespace S = NativeStaticAssociationInitializeProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace C = NativeStreamingModelContext;
        namespace I = NativeImageAccess;
        namespace M = NativeModelReferenceMap;
        namespace A = NativeTemporaryModelClumpCalls;
        struct Model
        {
            D::Value       world{};
            C::Value       context{};
            std::uintptr_t entry = 0, setupClass = 0;
            std::uint16_t  flags = 0;
        };
        struct Actor
        {
            D::Value                                    world{};
            std::uintptr_t                              vtable = 0, actorClass = 0, container = 0, cleanup = 0;
            std::uintptr_t                              component = 0, skeleton = 0;
            std::int32_t                                count = 0, frameCount = 0, frameCapacity = 0;
            std::array<std::uint32_t, S::FrameCapacity> keys{};
            std::array<std::int32_t, S::FrameCapacity>  names{};
            bool                                        setupRequired = false;
        };
        bool Profile(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            for (const auto& pin : P::Code)
                if (!pin.size || pin.size > actual.size() || !I::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            // The saved whole caller proves the opaque setup entry/ABI. We
            // retain that original routine; no decoded setup-effect claim or
            // guessed semantic structure for the second argument is needed.
            return D::Profile(image) && C::QualifyProfile(image) && I::ImagePages(image, image + P::SetupRva, 1, false, true) &&
                   I::ImagePages(image, image + P::OpaqueDefaultRva, 1, false, false);
        }
        ClockResult CaptureModel(std::uintptr_t image, const NativeModelBankReader::Value& model, A::CheckOwner owner, Model& output) noexcept
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            Model next;
            if ((result = NativeModelBankReader({image}, owner).Recheck(model)) != ClockResult::Ok)
                return result;
            if (!model.nativeModelInfo)
                return ClockResult::NotInitialized;
            if ((result = D::Capture(image, next.world)) != ClockResult::Ok || (result = C::Capture(image, model, next.context)) != ClockResult::Ok)
                return result;
            if (!NativeModelReferenceHeap::ReadOnlyImage(image, next.context.modelVtable, P::CreateSlot + sizeof(std::uintptr_t)) ||
                !I::ReadImage(image, next.context.modelVtable + P::CreateSlot, &next.entry, sizeof(next.entry)) || next.entry != image + P::CreateRva ||
                !D::At(model.nativeModelInfo, P::FlagsOffset, next.flags) ||
                !I::ReadImage(image, image + P::SetupClassSlotRva, &next.setupClass, sizeof(next.setupClass), true))
                return ClockResult::BindingChanged;
            // The original caller lazily obtains this class when absent. This
            // bounded foundation waits for that actual native cache instead of
            // inventing its constructor, class object or initialization tuple.
            if (!next.setupClass)
                return ClockResult::NotInitialized;
            if (!D::Add(image, next.setupClass, next.world))
                return ClockResult::BindingChanged;
            for (std::size_t index = 0; index != next.world.count; ++index)
                if (!D::Add(image, next.world.members[index].classAddress, next.world))
                    return ClockResult::BindingChanged;
            if (!D::Fence(image, next.world) || !C::MembershipUnchanged(image, next.context))
                return ClockResult::BindingChanged;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool Same(const Model& first, const Model& second) noexcept
        {
            return D::Same(first.world, second.world, true) && C::Same(first.context, second.context) && first.entry == second.entry &&
                   first.setupClass == second.setupClass && first.flags == second.flags;
        }
        ClockResult RecheckModel(std::uintptr_t image, const NativeModelBankReader::Value& model, A::CheckOwner owner, const Model& before) noexcept
        {
            Model      now;
            const auto result = CaptureModel(image, model, owner, now);
            if (result != ClockResult::Ok)
                return result;
            return Same(before, now) && D::Fence(image, before.world) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        ClockResult CaptureActor(std::uintptr_t image, std::uintptr_t clump, const Model& model, A::CheckOwner owner, bool prepared, Actor& output) noexcept
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            Actor next;
            next.world = model.world;
            if (!clump || clump % 8 || !M::PrivateWritable(clump, 0x310) || !M::Disjoint(clump, 0x310, image, P::ImageSize) ||
                !D::Add(image, clump, next.world) || !D::At(clump, 0, next.vtable) || !D::At(clump, 0x10, next.actorClass) ||
                !NativeModelReferenceHeap::ReadOnlyImage(image, next.vtable, 0x6d0) || !D::Add(image, next.actorClass, next.world) ||
                !NativeClassChainAccess(&D::Copy).IsChildOf(next.actorClass, model.setupClass, next.setupRequired))
                return ClockResult::BindingChanged;
            std::uintptr_t event = 0, name = 0, skeleton = 0;
            if (!I::ReadImage(image, next.vtable + 0x218, &event, sizeof(event)) || !I::ImagePages(image, event, 1, false, true) ||
                !I::ReadImage(image, next.vtable + 0x698, &name, sizeof(name)) || name != image + S::NameProviderRva ||
                !I::ReadImage(image, next.vtable + 0x6c8, &skeleton, sizeof(skeleton)) || skeleton != image + S::SkeletonProviderRva ||
                !D::At(clump, 0x298, next.component) ||
                (next.component &&
                 (!M::PrivateWritable(next.component, 0x480) || !D::Add(image, next.component, next.world) || !D::At(next.component, 0x478, next.skeleton))) ||
                (next.skeleton && !D::Add(image, next.skeleton, next.world)))
                return ClockResult::BindingChanged;
            if (prepared)
            {
                std::uintptr_t back = 0, containerOwner = 0;
                if (!D::At(clump, 0x300, next.container) || !D::At(clump, 0x308, next.cleanup) || !next.container || next.container % 8 ||
                    next.cleanup != image + S::CleanupRva || !M::PrivateWritable(next.container, S::ContainerBytes) ||
                    !M::Disjoint(next.container, S::ContainerBytes, clump, 0x310) || !D::At(next.container, 0x18, back) || back != clump ||
                    !D::At(next.container, 0x20, containerOwner) || containerOwner != 0 || !D::At(next.container, 0x10, next.count) || next.count < 0 ||
                    next.count > static_cast<std::int32_t>(S::FrameCapacity) || !D::At(next.container, 0x988, next.frameCount) ||
                    next.frameCount != next.count || !D::At(next.container, 0x98c, next.frameCapacity) ||
                    next.frameCapacity != static_cast<std::int32_t>(S::FrameCapacity))
                    return ClockResult::BindingChanged;
                for (std::size_t index = 0; index != static_cast<std::size_t>(next.count); ++index)
                    if (!D::At(next.container, S::FrameOffset + index * S::FrameStride + 0x18, next.keys[index]) ||
                        !D::At(next.container, 0x11cc + index * 4, next.names[index]) || next.names[index] < -1)
                        return ClockResult::BindingChanged;
            }
            for (std::size_t index = 0; index != next.world.count; ++index)
                if (!D::Add(image, next.world.members[index].classAddress, next.world))
                    return ClockResult::BindingChanged;
            if (!D::Fence(image, next.world))
                return ClockResult::BindingChanged;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool Same(const Actor& first, const Actor& second, bool prepared) noexcept
        {
            return D::Same(first.world, second.world, true) && first.vtable == second.vtable && first.actorClass == second.actorClass &&
                   first.component == second.component && first.skeleton == second.skeleton && first.setupRequired == second.setupRequired &&
                   (!prepared || (first.container == second.container && first.cleanup == second.cleanup && first.count == second.count &&
                                  first.frameCount == second.frameCount && first.frameCapacity == second.frameCapacity && first.keys == second.keys &&
                                  first.names == second.names));
        }
        ClockResult Create(A::CreateFunction entry, std::uintptr_t model, std::uintptr_t opaque, std::uint32_t zero, std::int32_t minusOne,
                           std::uintptr_t stackZero, std::uintptr_t& result, std::uint32_t& exception) noexcept
        {
            __try
            {
                result = entry(model, opaque, zero, minusOne, stackZero);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult Setup(A::SetupFunction entry, std::uintptr_t clump, std::uintptr_t opaque, std::uint32_t zero, std::uint32_t secondZero,
                          std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(clump, opaque, zero, secondZero);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult Prepare(A::PrepareFunction entry, std::uintptr_t clump, std::uintptr_t model, std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(clump, model);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult Retire(A::RetireFunction entry, std::uintptr_t clump, bool flag, bool& accepted, std::uint32_t& exception) noexcept
        {
            __try
            {
                accepted = entry(clump, flag);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
    }
    struct NativeTemporaryModelClumpBorrow::Context
    {
        const TemporaryClumpDetail::Model* model = nullptr;
        const TemporaryClumpDetail::Actor* actor = nullptr;
    };
    struct NativeTemporaryModelClumpAccess
    {
        static ClockResult RecheckBorrow(const NativeTemporaryModelClumpBorrow& borrow) noexcept
        {
            namespace T = TemporaryClumpDetail;
            namespace D = NativeStreamingCompletionContext;
            if (!borrow.image || !borrow.generation || !borrow.owner || !borrow.record || !borrow.origin || !borrow.origin->model || !borrow.origin->actor)
                return ClockResult::NotInitialized;
            // Admit the actual native thread/scope before reading the mutable
            // record. A copied observation does not create an actor/GC lease.
            auto result = D::Owner(borrow.image, borrow.owner);
            if (result != ClockResult::Ok)
                return result;
            const auto active = [&]() noexcept
            {
                const auto& record = *borrow.record;
                return record.generation == borrow.generation && record.activeBorrow == &borrow &&
                       record.GetState() == NativeTemporaryModelClump::State::RowEntered && record.outstanding.load(std::memory_order_acquire) &&
                       record.model.nativeModelInfo && record.clump && record.container && record.container == borrow.origin->actor->container;
            };
            if (!active())
                return ClockResult::OwnerConflict;
            if (borrow.world != &borrow.origin->actor->world || borrow.modelContext != &borrow.origin->model->context)
                return ClockResult::BindingChanged;
            const auto& record = *borrow.record;
            if ((result = T::RecheckModel(borrow.image, record.model, borrow.owner, *borrow.origin->model)) != ClockResult::Ok)
                return result;
            T::Actor observed;
            if ((result = T::CaptureActor(borrow.image, record.clump, *borrow.origin->model, borrow.owner, true, observed)) != ClockResult::Ok)
                return result;
            if (!T::Same(*borrow.origin->actor, observed, true) || !D::Fence(borrow.image, borrow.origin->actor->world))
                return ClockResult::BindingChanged;
            if ((result = D::Owner(borrow.image, borrow.owner)) != ClockResult::Ok)
                return result;
            return active() ? ClockResult::Ok : ClockResult::OwnerConflict;
        }
        static ClockResult Run(std::uintptr_t image, NativeTemporaryModelClump& record, std::uint64_t generation, const NativeModelBankReader::Value& source,
                               NativeTemporaryModelClumpCalls::CheckOwner owner, NativeTemporaryModelClumpCalls::RowInitializer initialize, void* context,
                               NativeTemporaryModelClumpCalls::Receipt* output, const NativeTemporaryModelClumpCalls::Invocations* injected) noexcept
        {
            namespace T = TemporaryClumpDetail;
            namespace A = NativeTemporaryModelClumpCalls;
            namespace D = NativeStreamingCompletionContext;
            namespace P = NativeTemporaryModelClumpProfile;
            using State = NativeTemporaryModelClump::State;
            const auto model = source;
            const auto calls = injected ? *injected : A::Invocations{};
            A::Receipt receipt;
            receipt.model = model;
            receipt.generation = generation;
            receipt.opaqueDefault = image + P::OpaqueDefaultRva;
            bool       ownsRecord = false;
            const auto publish = [&](ClockResult result) noexcept
            {
                if (output)
                    *output = receipt;
                return result;
            };
            const auto phase = [&](State value) noexcept
            {
                record.state.store(value, std::memory_order_release);
                receipt.phase = value;
            };
            const auto finish = [&](ClockResult result) noexcept
            {
                if (!ownsRecord)
                    return publish(result);
                record.activeBorrow = nullptr;
                if (record.outstanding.load(std::memory_order_acquire))
                {
                    receipt.debt = true;
                    phase(State::FaultDebt);
                }
                return publish(result);
            };
            if (record.GetState() != State::Empty)
            {
                // A second call cannot turn an entered/unknown native outcome
                // into a retry, nor adopt a previous raw receipt as authority.
                if (output)
                    *output = receipt;
                return ClockResult::Busy;
            }
            if (!image || !generation || !owner || !initialize)
                return finish(ClockResult::InvalidArgument);
            if (!D::TryEnter())
                return finish(ClockResult::Busy);
            D::Scope scope;
            // A caller may have observed Empty before another invocation won
            // the gate. Only the invocation that owns this scope and rechecks
            // Empty may mutate private record state or clear an active borrow.
            if (record.GetState() != State::Empty)
                return publish(ClockResult::Busy);
            ownsRecord = true;
            T::Model before, confirmed;
            auto     result = T::CaptureModel(image, model, owner, before);
            if (result != ClockResult::Ok || (result = T::CaptureModel(image, model, owner, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!T::Same(before, confirmed) || !D::Fence(image, before.world))
                return finish(ClockResult::BindingChanged);
            if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            record.generation = generation;
            record.model = model;
            record.outstanding.store(true, std::memory_order_release);
            receipt.createEntry = before.entry;
            receipt.modelFlags = before.flags;
            receipt.setupClass = before.setupClass;
            receipt.createEntered = true;
            phase(State::CreateEntered);
            std::uintptr_t returned = 0;
            result = (calls.create ? calls.create : &T::Create)(reinterpret_cast<A::CreateFunction>(before.entry), model.nativeModelInfo, receipt.opaqueDefault,
                                                                0, -1, 0, returned, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            // This causal return is journaled before owner/world/page checks.
            // Even a subsequent refusal must keep a nonnull native actor owned.
            record.clump = returned;
            receipt.clump = returned;
            receipt.createReturned = true;
            if (!returned)
            {
                record.outstanding.store(false, std::memory_order_release);
                receipt.returnedNull = true;
                phase(State::ReturnedNull);
                result = T::RecheckModel(image, model, owner, before);
                receipt.verified = result == ClockResult::Ok;
                return finish(result);
            }
            phase(State::LiveUnprepared);
            if ((result = T::RecheckModel(image, model, owner, before)) != ClockResult::Ok)
                return finish(result);
            T::Actor actor, actorConfirmed;
            if ((result = T::CaptureActor(image, returned, before, owner, false, actor)) != ClockResult::Ok ||
                (result = T::CaptureActor(image, returned, before, owner, false, actorConfirmed)) != ClockResult::Ok)
                return finish(result);
            if (!T::Same(actor, actorConfirmed, false))
                return finish(ClockResult::BindingChanged);
            receipt.actorClass = actor.actorClass;
            receipt.setupRequired = actor.setupRequired;
            if (actor.setupRequired)
            {
                if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                    return finish(result);
                receipt.setupEntered = true;
                phase(State::SetupEntered);
                result = (calls.setup ? calls.setup : &T::Setup)(reinterpret_cast<A::SetupFunction>(image + P::SetupRva), returned, receipt.opaqueDefault, 0, 0,
                                                                 receipt.exceptionCode);
                if (result != ClockResult::Ok)
                    return finish(result);
                receipt.setupReturned = true;
                phase(State::SetupReturned);
            }
            if ((result = T::RecheckModel(image, model, owner, before)) != ClockResult::Ok ||
                (result = T::CaptureActor(image, returned, before, owner, false, actorConfirmed)) != ClockResult::Ok)
                return finish(result);
            if (!T::Same(actor, actorConfirmed, false))
                return finish(ClockResult::BindingChanged);
            receipt.prepareEntered = true;
            phase(State::InitializeEntered);
            // The model wrapper may already have called FFBDF0. Source calls
            // it explicitly again; preserving both calls avoids inventing a
            // deduplication policy for the wrapper's 0x100 flag branch.
            result = (calls.prepare ? calls.prepare : &T::Prepare)(reinterpret_cast<A::PrepareFunction>(image + P::PrepareRva), returned, model.nativeModelInfo,
                                                                   receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.prepareReturned = true;
            if (!D::Fence(image, actor.world))
                return finish(ClockResult::BindingChanged);
            if ((result = T::RecheckModel(image, model, owner, before)) != ClockResult::Ok ||
                (result = T::CaptureActor(image, returned, before, owner, true, actor)) != ClockResult::Ok ||
                (result = T::CaptureActor(image, returned, before, owner, true, actorConfirmed)) != ClockResult::Ok)
                return finish(result);
            if (!T::Same(actor, actorConfirmed, true))
                return finish(ClockResult::BindingChanged);
            record.container = actor.container;
            receipt.container = actor.container;
            receipt.frameCount = static_cast<std::uint16_t>(actor.count);
            receipt.containerObserved = true;
            phase(State::Prepared);
            const NativeTemporaryModelClumpBorrow::Context origin{&before, &actor};
            NativeTemporaryModelClumpBorrow                borrow;
            borrow.origin = &origin;
            borrow.record = &record;
            borrow.image = image;
            borrow.generation = generation;
            borrow.owner = owner;
            borrow.world = &actor.world;
            borrow.modelContext = &before.context;
            record.activeBorrow = &borrow;
            receipt.rowEntered = true;
            phase(State::RowEntered);
            receipt.rowResult = initialize(borrow, context);
            receipt.rowReturned = true;
            record.activeBorrow = nullptr;
            phase(State::RowReturned);
            if (receipt.rowResult != ClockResult::Ok)
                return finish(receipt.rowResult);
            if ((result = T::RecheckModel(image, model, owner, before)) != ClockResult::Ok ||
                (result = T::CaptureActor(image, returned, before, owner, true, actorConfirmed)) != ClockResult::Ok)
                return finish(result);
            if (!T::Same(actor, actorConfirmed, true) || !D::Fence(image, actor.world))
                return finish(ClockResult::BindingChanged);
            if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            bool accepted = false;
            receipt.retireEntered = true;
            phase(State::RetireEntered);
            result = (calls.retire ? calls.retire : &T::Retire)(reinterpret_cast<A::RetireFunction>(image + P::RetireRva), returned, false, accepted,
                                                                receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.retireReturned = true;
            receipt.retireAccepted = accepted;
            if (!accepted)
                return finish(ClockResult::RestoreFailure);
            // Native returns an accepted deferred retirement. The object may
            // still occupy its allocation/registry row; never poll for free or
            // call a native heap free as a substitute for that acknowledgement.
            record.outstanding.store(false, std::memory_order_release);
            phase(State::RetiredDeferred);
            result = T::RecheckModel(image, model, owner, before);
            receipt.verified = result == ClockResult::Ok;
            return finish(result);
        }
    };
    ClockResult NativeTemporaryModelClumpBorrow::RecheckOrigin() const noexcept
    {
        return NativeTemporaryModelClumpAccess::RecheckBorrow(*this);
    }
    ClockResult NativeTemporaryModelClumpBorrow::ClaimInitialization() const noexcept
    {
        const auto result = RecheckOrigin();
        if (result != ClockResult::Ok)
            return result;
        // Only this admitted current native owner can mutate the claim. A
        // second journal or nested call cannot replay the original initializer;
        // ordinary later origin rechecks remain valid after the claim.
        if (initializationClaimed)
            return ClockResult::Busy;
        initializationClaimed = true;
        return ClockResult::Ok;
    }
    namespace NativeTemporaryModelClumpCalls
    {
        ClockResult WithTemporary(std::uintptr_t image, NativeTemporaryModelClump& record, std::uint64_t generation, const NativeModelBankReader::Value& model,
                                  CheckOwner owner, RowInitializer initialize, void* context, Receipt* receipt, const Invocations* invocations) noexcept
        {
            return NativeTemporaryModelClumpAccess::Run(image, record, generation, model, owner, initialize, context, receipt, invocations);
        }
    }
}
