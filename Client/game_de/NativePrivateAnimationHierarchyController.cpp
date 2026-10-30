/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Durable private hierarchy commands over the same issued child owner
 ****************************************************************************/
#include "NativePrivateAnimationHierarchyController.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include <algorithm>
#include <iterator>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace PrivateHierarchyControllerDetail
    {
        struct Scope
        {
            std::atomic_flag& operation;
            ~Scope() { operation.clear(std::memory_order_release); }
        };
        namespace Calls = NativePrivateAnimationHierarchyCalls;
        void PreserveReservation(NativeCustomAnimSequenceController::PrivateDependencyReceipt&       receipt,
                                 const NativeCustomAnimSequenceController::PrivateDependencyReceipt& previous) noexcept
        {
            // An early Busy/refusal resets its output but cannot retract a
            // prior actual reservation. Preserve that durable truth until a
            // real returned cancellation/detach release receipt replaces it.
            if (!receipt.reserved && !receipt.released && !receipt.uncertain)
                receipt = previous;
        }
    }
    NativePrivateAnimationHierarchyController::Entries::iterator NativePrivateAnimationHierarchyController::Find(const Token& token) noexcept
    {
        auto result =
            std::find_if(entries.begin(), entries.end(), [&](const Entry& entry)
                         { return entry.issued && entry.storage.OwnerGeneration() == token.ownerGeneration && entry.storage.Address() == token.locator; });
        // Only the actual child owner's final release receipt consumes this
        // journal. An absent child map, pointer0 or Action Ok is never an ACK.
        if (result != entries.end() && result->link.active && result->link.dependency.released)
            result->link = {};
        return result;
    }
    bool NativePrivateAnimationHierarchyController::Owns(const Token& token) const noexcept
    {
        return std::any_of(entries.begin(), entries.end(), [&](const Entry& entry)
                           { return entry.issued && entry.storage.OwnerGeneration() == token.ownerGeneration && entry.storage.Address() == token.locator; });
    }
    void NativePrivateAnimationHierarchyController::Publish() noexcept
    {
        if (entries.empty())
            stopping = false;
        retained.store(!entries.empty(), std::memory_order_release);
        const bool fault =
            std::any_of(entries.begin(), entries.end(),
                        [](const Entry& entry)
                        {
                            const auto state = entry.storage.GetState();
                            return state == Storage::State::Failed || state == Storage::State::PublicationPinned || state == Storage::State::Entered ||
                                   std::any_of(entry.associations.begin(), entry.associations.end(),
                                               [](const AssociationDependent& dependent)
                                               {
                                                   return !dependent.identity ||
                                                          dependent.identity->GetState() == NativeAnimationAssociationHierarchyDependency::State::Uncertain;
                                               }) ||
                                   (entry.link.active && (entry.link.phase != LinkPhase::AttachedAccepted || entry.link.dependency.uncertain));
                        });
        recovery.store(!entries.empty() && (stopping || fault), std::memory_order_release);
    }
    void NativePrivateAnimationHierarchyController::AssociationReceipt(const NativeAnimationAssociationHierarchyDependency& dependency, ClockResult result,
                                                                       NativeAnimationAssociationHierarchyDependencyReceipt* output) noexcept
    {
        if (!output)
            return;
        NativeAnimationAssociationHierarchyDependencyReceipt next;
        next.receiverGeneration = dependency.receiver.ownerGeneration;
        next.receiverSerial = dependency.receiver.serial;
        next.command = dependency.command;
        next.hierarchyIssue = dependency.hierarchyIssue;
        next.hierarchy = dependency.hierarchy.locator;
        next.sequences = dependency.sequences;
        const auto state = dependency.GetState();
        next.planned = dependency.command && dependency.receiver.ownerGeneration && dependency.receiver.serial;
        next.held = state == NativeAnimationAssociationHierarchyDependency::State::Held;
        next.releaseEntered = state == NativeAnimationAssociationHierarchyDependency::State::ReleaseEntered ||
                              state == NativeAnimationAssociationHierarchyDependency::State::Released;
        next.released = state == NativeAnimationAssociationHierarchyDependency::State::Released;
        next.uncertain = state == NativeAnimationAssociationHierarchyDependency::State::Uncertain;
        next.verified = result == ClockResult::Ok;
        next.result = result;
        *output = next;
    }
    bool NativePrivateAnimationHierarchyController::HasAssociationDependents(const Entry& entry) noexcept
    {
        return !entry.associations.empty();
    }
    bool NativePrivateAnimationHierarchyController::AssociationBinding(const Entry&                                         entry,
                                                                       const NativeAnimationAssociationHierarchyDependency& dependency) const noexcept
    {
        const auto record = std::find_if(entry.associations.begin(), entry.associations.end(),
                                         [&](const AssociationDependent& value) { return value.identity == &dependency; });
        if (record == entry.associations.end() || dependency.controller != this || dependency.hierarchyIssue != entry.issue ||
            dependency.hierarchy.ownerGeneration != entry.storage.OwnerGeneration() || dependency.hierarchy.locator != entry.storage.Address() ||
            record->target != dependency.target || record->receiver.ownerGeneration != dependency.receiver.ownerGeneration ||
            record->receiver.serial != dependency.receiver.serial || record->command != dependency.command)
            return false;
        return true;
    }
    ClockResult NativePrivateAnimationHierarchyController::HoldForAssociation(std::uintptr_t image, const Token& requested,
                                                                              NativeAnimationAssociationHierarchyDependency& dependency, CheckOwner owner,
                                                                              NativeAnimationAssociationHierarchyDependencyReceipt* output) noexcept
    {
        using State = NativeAnimationAssociationHierarchyDependency::State;
        if (output)
            *output = {};
        const auto finish = [&](ClockResult value)
        {
            AssociationReceipt(dependency, value, output);
            return value;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        auto result = owner();
        if (result != ClockResult::Ok)
            return finish(result);
        if (!image || !requested.ownerGeneration || !requested.locator || !dependency.target || !dependency.receiver.serial || !dependency.command ||
            dependency.receiver.ownerGeneration != requested.ownerGeneration || dependency.GetState() != State::Planned || dependency.controller)
            return finish(ClockResult::OwnerConflict);
        if (operation.test_and_set(std::memory_order_acquire))
            return finish(ClockResult::Busy);
        PrivateHierarchyControllerDetail::Scope held{operation};
        if (stopping)
            return finish(ClockResult::OwnerConflict);
        const auto entry = Find(requested);
        if (entry == entries.end() || !entry->issue)
            return finish(ClockResult::BindingChanged);
        if (entry->cleanupCommand || entry->storage.GetState() != Storage::State::Ready ||
            (entry->link.active && (entry->link.phase != LinkPhase::AttachedAccepted || !entry->link.dependency.reserved || !entry->link.dependency.verified ||
                                    entry->link.dependency.released || entry->link.dependency.uncertain)))
            return finish(ClockResult::OwnerConflict);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        // Allocate the actual hierarchy's dependent record before defaults or
        // original entry. It owns no native ref and never acquires a gate.
        try
        {
            entry->associations.push_back({&dependency, dependency.target, dependency.receiver, dependency.command});
        }
        catch (...)
        {
            return finish(ClockResult::WriteFailure);
        }
        dependency.controller = this;
        dependency.image = image;
        dependency.hierarchy = requested;
        dependency.hierarchyIssue = entry->issue;
        dependency.hadSequenceLink = entry->link.active;
        dependency.sequences = entry->link.active ? entry->link.key.arrayTag : 0;
        dependency.sequenceCommand = entry->link.active ? entry->link.key.command : 0;
        dependency.state.store(State::Held, std::memory_order_release);
        Publish();
        return finish(owner());
    }
    ClockResult NativePrivateAnimationHierarchyController::RecheckForAssociation(std::uintptr_t                                        image,
                                                                                 const NativeAnimationAssociationHierarchyDependency&  dependency,
                                                                                 CheckOwner                                            owner,
                                                                                 NativeAnimationAssociationHierarchyDependencyReceipt* output) noexcept
    {
        using State = NativeAnimationAssociationHierarchyDependency::State;
        if (output)
            *output = {};
        const auto finish = [&](ClockResult value)
        {
            AssociationReceipt(dependency, value, output);
            return value;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        auto result = owner();
        if (result != ClockResult::Ok)
            return finish(result);
        if (image != dependency.image || dependency.controller != this || (dependency.GetState() != State::Held && dependency.GetState() != State::Uncertain))
            return finish(ClockResult::OwnerConflict);
        if (operation.test_and_set(std::memory_order_acquire))
            return finish(ClockResult::Busy);
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(dependency.hierarchy);
        if (entry == entries.end() || !AssociationBinding(*entry, dependency))
            return finish(ClockResult::BindingChanged);
        if (entry->cleanupCommand || entry->storage.GetState() != Storage::State::Ready || entry->link.active != dependency.hadSequenceLink ||
            (entry->link.active && (entry->link.phase != LinkPhase::AttachedAccepted || entry->link.key.arrayTag != dependency.sequences ||
                                    entry->link.key.command != dependency.sequenceCommand || !entry->link.dependency.reserved ||
                                    !entry->link.dependency.verified || entry->link.dependency.released || entry->link.dependency.uncertain)))
            return finish(ClockResult::OwnerConflict);
        return finish(owner());
    }
    ClockResult NativePrivateAnimationHierarchyController::ReadForAssociation(std::uintptr_t                                       image,
                                                                              const NativeAnimationAssociationHierarchyDependency& dependency, CheckOwner owner,
                                                                              Values& output, std::uintptr_t& sequencesOutput) noexcept
    {
        using State = NativeAnimationAssociationHierarchyDependency::State;
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (image != dependency.image || dependency.controller != this || (dependency.GetState() != State::Held && dependency.GetState() != State::Uncertain))
            return ClockResult::OwnerConflict;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(dependency.hierarchy);
        if (entry == entries.end() || !AssociationBinding(*entry, dependency))
            return ClockResult::BindingChanged;
        if (entry->cleanupCommand || entry->storage.GetState() != Storage::State::Ready || entry->link.active != dependency.hadSequenceLink ||
            (entry->link.active && (entry->link.phase != LinkPhase::AttachedAccepted || entry->link.key.arrayTag != dependency.sequences ||
                                    entry->link.key.command != dependency.sequenceCommand || !entry->link.dependency.reserved ||
                                    !entry->link.dependency.verified || entry->link.dependency.released || entry->link.dependency.uncertain)))
            return ClockResult::OwnerConflict;
        Values         next;
        std::uintptr_t sequencesValue = 0;
        // Snapshot/definedness are the existing real private storage rules.
        // The exact held record replaces neither sequence ownership nor the
        // caller's already-held native scope with a public raw read path.
        result = entry->storage.ReadForAssociationHeld(image, dependency.hierarchy.ownerGeneration, owner, next, sequencesValue);
        Publish();
        if (result != ClockResult::Ok)
            return result;
        if (next.interfaceAddress != dependency.hierarchy.locator || sequencesValue != dependency.sequences)
            return ClockResult::BindingChanged;
        if ((result = owner()) != ClockResult::Ok)
            return result;
        output = next;
        sequencesOutput = sequencesValue;
        return ClockResult::Ok;
    }
    ClockResult NativePrivateAnimationHierarchyController::ReleaseForAssociation(std::uintptr_t                                    image,
                                                                                 NativeAnimationAssociationHierarchyDependency&    dependency,
                                                                                 const NativeAnimationAssociationHierarchyRelease& proof, CheckOwner owner,
                                                                                 NativeAnimationAssociationHierarchyDependencyReceipt* output) noexcept
    {
        using State = NativeAnimationAssociationHierarchyDependency::State;
        using Reason = NativeAnimationAssociationHierarchyRelease::Reason;
        if (output)
            *output = {};
        const auto finish = [&](ClockResult value)
        {
            AssociationReceipt(dependency, value, output);
            return value;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        auto result = owner();
        if (result != ClockResult::Ok)
            return finish(result);
        if (image != dependency.image || dependency.controller != this || proof.target != &dependency || !proof.command || proof.command < dependency.command ||
            (dependency.GetState() != State::Held && dependency.GetState() != State::Uncertain))
            return finish(ClockResult::OwnerConflict);
        if (operation.test_and_set(std::memory_order_acquire))
            return finish(ClockResult::Busy);
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(dependency.hierarchy);
        if (entry == entries.end() || !AssociationBinding(*entry, dependency))
            return finish(ClockResult::BindingChanged);
        if (proof.reason == Reason::HierarchyReplaced)
        {
            if (!proof.replacementHierarchy || proof.replacementHierarchy == dependency.hierarchy.locator)
                return finish(ClockResult::OwnerConflict);
            if (proof.replacement)
            {
                const auto& replacement = *proof.replacement;
                const auto  next = Find(replacement.hierarchy);
                if (next == entries.end() || !AssociationBinding(*next, replacement) || !replacement.IsHeld() || replacement.target != dependency.target ||
                    replacement.receiver.ownerGeneration != dependency.receiver.ownerGeneration || replacement.receiver.serial != dependency.receiver.serial ||
                    replacement.image != image || replacement.hierarchy.locator != proof.replacementHierarchy)
                    return finish(ClockResult::OwnerConflict);
            }
            // A null replacement dependency means the closed actual storage
            // proof verified a bank origin; it never adopts a private raw H.
        }
        else if (proof.reason == Reason::ReceiverRetired || proof.reason == Reason::UnenteredNewHoldCancellation)
        {
            if (proof.replacementHierarchy || proof.replacement ||
                (proof.reason == Reason::UnenteredNewHoldCancellation && dependency.GetState() != State::Held))
                return finish(ClockResult::OwnerConflict);
        }
        else
            return finish(ClockResult::InvalidArgument);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        dependency.state.store(State::ReleaseEntered, std::memory_order_release);
        const auto record = std::find_if(entry->associations.begin(), entry->associations.end(),
                                         [&](const AssociationDependent& value) { return value.identity == &dependency; });
        entry->associations.erase(record);
        // The known CPU release precedes the final fallible owner bookend.
        // NodesReleased never reaches this path without an independent closed
        // replacement/retirement/cancellation proof from actual storage.
        dependency.state.store(State::Released, std::memory_order_release);
        Publish();
        return finish(owner());
    }

    ClockResult NativePrivateAnimationHierarchyController::ResolvePrivate(std::uintptr_t image, std::uint64_t generation, std::uintptr_t locator,
                                                                          CheckOwner owner, Token& output) noexcept
    {
        const Token request{generation, locator};
        if (!request.ownerGeneration || !request.locator)
            return ClockResult::InvalidArgument;
        Values values;
        // This controller's Read begins at its actual private list. Generic
        // bank routing is deliberately unavailable to a custom factory; a
        // matching readable image address cannot substitute for ownership.
        const auto result = Read(image, request, owner, values);
        if (result != ClockResult::Ok)
            return result;
        if (values.interfaceAddress != request.locator)
            return ClockResult::BindingChanged;
        output = request;
        return ClockResult::Ok;
    }
    ClockResult NativePrivateAnimationHierarchyController::Create(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, Token& output,
                                                                  Receipt* receipt) noexcept
    {
        const auto requested = generation;
        if (receipt)
            *receipt = {};
        if (!requested)
            return ClockResult::InvalidArgument;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope scope{operation};
        if (stopping || (!entries.empty() && entries.front().storage.OwnerGeneration() != requested))
            return ClockResult::OwnerConflict;
        if (nextIssue == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::OwnerConflict;
        try
        {
            entries.emplace_back(requested);
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        const auto entry = std::prev(entries.end());
        entry->issue = ++nextIssue;
        Receipt    resultReceipt;
        const auto result = PrivateHierarchyControllerDetail::Calls::Initialize(image, entry->storage, requested, owner, &resultReceipt);
        if (result == ClockResult::Ok)
        {
            entry->issued = true;
            Token next;
            next.ownerGeneration = requested;
            next.locator = entry->storage.Address();
            output = next;
        }
        else if (!resultReceipt.entered)
            entries.erase(entry);
        // An entered failed CPU record remains owned and cannot reinitialize
        // or replay. The ordinary empty-neverpublished retire path owns it.
        if (receipt)
            *receipt = resultReceipt;
        Publish();
        return result;
    }

    ClockResult NativePrivateAnimationHierarchyController::Initialize(std::uintptr_t image, const Token& value, CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto token = value;
        if (receipt)
            *receipt = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        if (stopping)
            return ClockResult::OwnerConflict;
        const auto entry = Find(token);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        // A live dynamic receiver still observes H even after node release.
        if (HasAssociationDependents(*entry))
            return ClockResult::OwnerConflict;
        if (entry->cleanupCommand)
            return ClockResult::OwnerConflict;
        if (entry->link.active)
            return ClockResult::OwnerConflict;
        const auto result = PrivateHierarchyControllerDetail::Calls::Initialize(image, entry->storage, token.ownerGeneration, owner, receipt);
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::SetScalar(std::uintptr_t image, const Token& value,
                                                                     NativePrivateAnimationHierarchyCalls::ScalarOperation operationValue,
                                                                     std::uint32_t bitsValue, CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto token = value;
        const auto kind = operationValue;
        const auto bits = bitsValue;
        if (receipt)
            *receipt = {};
        bool linked = false;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            if (stopping)
                return ClockResult::OwnerConflict;
            const auto entry = Find(token);
            if (entry == entries.end())
                return ClockResult::BindingChanged;
            if (entry->cleanupCommand)
                return ClockResult::OwnerConflict;
            linked = entry->link.active;
            if (HasAssociationDependents(*entry) && kind == NativePrivateAnimationHierarchyCalls::ScalarOperation::SequenceCount && !linked)
            {
                Values     current;
                const auto observed = PrivateHierarchyControllerDetail::Calls::Read(image, entry->storage, token.ownerGeneration, owner, current);
                if (observed != ClockResult::Ok)
                {
                    Publish();
                    return observed;
                }
                if (current.sequenceCount != static_cast<std::uint16_t>(bits))
                    return ClockResult::OwnerConflict;
            }
            if (!linked)
            {
                ClockResult result;
                using Operation = NativePrivateAnimationHierarchyCalls::ScalarOperation;
                if (kind == Operation::RunningCompressed)
                    result =
                        PrivateHierarchyControllerDetail::Calls::SetRunningCompressed(image, entry->storage, token.ownerGeneration, bits != 0, owner, receipt);
                else if (kind == Operation::AnimationBlockID)
                {
                    std::int32_t block;
                    std::memcpy(&block, &bits, sizeof(block));
                    result = PrivateHierarchyControllerDetail::Calls::SetAnimationBlockID(image, entry->storage, token.ownerGeneration, block, owner, receipt);
                }
                else if (kind == Operation::NameHash)
                    result = PrivateHierarchyControllerDetail::Calls::SetNameHash(image, entry->storage, token.ownerGeneration, bits, owner, receipt);
                else if (kind == Operation::SequenceCount)
                    result = PrivateHierarchyControllerDetail::Calls::SetSequenceCount(image, entry->storage, token.ownerGeneration,
                                                                                       static_cast<std::uint16_t>(bits), owner, receipt);
                else
                    result = ClockResult::InvalidArgument;
                Publish();
                return result;
            }
        }
        ActionContext context{this, token, {}, owner, Action::Scalar};
        context.scalar = kind;
        context.bits = bits;
        return LinkedAction(image, token, owner, context, receipt);
    }
    ClockResult NativePrivateAnimationHierarchyController::SetRunningCompressed(std::uintptr_t image, const Token& token, bool value, CheckOwner owner,
                                                                                Receipt* receipt) noexcept
    {
        return SetScalar(image, token, NativePrivateAnimationHierarchyCalls::ScalarOperation::RunningCompressed, value ? 1u : 0u, owner, receipt);
    }
    ClockResult NativePrivateAnimationHierarchyController::SetAnimationBlockID(std::uintptr_t image, const Token& token, std::int32_t value, CheckOwner owner,
                                                                               Receipt* receipt) noexcept
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return SetScalar(image, token, NativePrivateAnimationHierarchyCalls::ScalarOperation::AnimationBlockID, bits, owner, receipt);
    }
    ClockResult NativePrivateAnimationHierarchyController::SetNameHash(std::uintptr_t image, const Token& token, std::uint32_t value, CheckOwner owner,
                                                                       Receipt* receipt) noexcept
    {
        return SetScalar(image, token, NativePrivateAnimationHierarchyCalls::ScalarOperation::NameHash, value, owner, receipt);
    }
    ClockResult NativePrivateAnimationHierarchyController::SetSequenceCount(std::uintptr_t image, const Token& token, std::uint16_t value, CheckOwner owner,
                                                                            Receipt* receipt) noexcept
    {
        return SetScalar(image, token, NativePrivateAnimationHierarchyCalls::ScalarOperation::SequenceCount, value, owner, receipt);
    }

    ClockResult NativePrivateAnimationHierarchyController::CheckCacheZero(std::uintptr_t image, const Token& value, CheckOwner owner) noexcept
    {
        const auto token = value;
        Values     values;
        // Actual classic4D42A0 guards every list effect with cache!=nullptr.
        // The existing qualified snapshot proves cache0, including a linked
        // parent's actual array epoch. No native cache entry is invoked.
        return Read(image, token, owner, values);
    }
    ClockResult NativePrivateAnimationHierarchyController::RemoveEmptySequences(std::uintptr_t image, const Token& value, CheckOwner owner,
                                                                                Receipt* output) noexcept
    {
        const auto token = value;
        if (output)
            *output = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        if (stopping)
            return ClockResult::OwnerConflict;
        const auto entry = Find(token);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        if (entry->cleanupCommand)
            return ClockResult::OwnerConflict;
        // Pointer0 alone is not a detach ACK. A reserved/pending/linked parent
        // keeps all dependency journals until its separate matching command.
        if (entry->link.active)
            return ClockResult::OwnerConflict;
        const auto result = PrivateHierarchyControllerDetail::Calls::RemoveEmptySequences(image, entry->storage, token.ownerGeneration, owner, output);
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::ValidatePayloadHeld(const Epoch& epoch, std::uint16_t count, PostProcessOperation operation,
                                                                               void* opaque) noexcept
    {
        auto& context = *static_cast<ActionContext*>(opaque);
        return context.self->sequences->QualifyUncompressedRowsHeld(epoch, context.key, count, operation, context.owner);
    }
    ClockResult NativePrivateAnimationHierarchyController::RemoveQuaternionFlips(std::uintptr_t image, const Token& token, CheckOwner owner,
                                                                                 PostProcessReceipt* receipt, InvokePostProcess invoke) noexcept
    {
        return ProcessPayload(image, token, PostProcessOperation::RemoveQuaternionFlips, owner, receipt, invoke);
    }
    ClockResult NativePrivateAnimationHierarchyController::CalculateTotalTime(std::uintptr_t image, const Token& token, CheckOwner owner,
                                                                              PostProcessReceipt* receipt, InvokePostProcess invoke) noexcept
    {
        return ProcessPayload(image, token, PostProcessOperation::CalculateTotalTime, owner, receipt, invoke);
    }
    ClockResult NativePrivateAnimationHierarchyController::ProcessPayload(std::uintptr_t image, const Token& value, PostProcessOperation operationValue,
                                                                          CheckOwner owner, PostProcessReceipt* output, InvokePostProcess invoke) noexcept
    {
        const auto token = value;
        const auto requested = operationValue;
        const auto requestedInvoke = invoke;
        if (output)
            *output = {};
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            if (stopping)
                return ClockResult::OwnerConflict;
            const auto entry = Find(token);
            if (entry == entries.end())
                return ClockResult::BindingChanged;
            if (entry->cleanupCommand)
                return ClockResult::OwnerConflict;
            if (!entry->link.active)
            {
                // The genuine count0 branches touch no sequence/frame. A
                // positive count without an acknowledged link is refused by
                // the native qualifier rather than treated as an empty no-op.
                const auto result = PrivateHierarchyControllerDetail::Calls::PostProcessEmpty(image, entry->storage, token.ownerGeneration, requested, owner,
                                                                                              output, requestedInvoke);
                Publish();
                return result;
            }
        }
        ActionContext context{this, token, {}, owner, Action::PostProcess};
        context.postProcess = requested;
        context.invokePostProcess = requestedInvoke;
        auto result = LinkedAction(image, token, owner, context, nullptr);
        if (result == ClockResult::Ok)
        {
            if (operation.test_and_set(std::memory_order_acquire))
                result = ClockResult::Busy;
            else
            {
                PrivateHierarchyControllerDetail::Scope held{operation};
                const auto                              entry = Find(token);
                if (entry == entries.end() || !entry->link.active || !SequenceOwner::SamePrivateKey(entry->link.key, context.key) ||
                    entry->link.phase != LinkPhase::AttachedAccepted)
                    result = ClockResult::OwnerConflict;
                else
                    result = PrivateHierarchyControllerDetail::Calls::AcknowledgePostProcess(entry->storage, token.ownerGeneration, context.postReceipt);
                Publish();
            }
        }
        // This is the actual whole owner acknowledgement. Failed outer
        // bookends leave the dedicated pending command blocked; they neither
        // poison successful allocation records nor replay the native entry.
        if (result != ClockResult::Ok)
            context.postReceipt.verified = false;
        if (output)
            *output = context.postReceipt;
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::ParentAction(NativeCustomAnimSequenceStorage& array, const Epoch& epoch, void* opaque) noexcept
    {
        auto& context = *static_cast<ActionContext*>(opaque);
        auto& self = *context.self;
        if (self.operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{self.operation};
        const auto                              entry = self.Find(context.token);
        if (entry == self.entries.end() || !entry->link.active)
            return ClockResult::BindingChanged;
        if (entry->cleanupCommand && entry->cleanupCommand != context.cleanupCommand)
            return ClockResult::OwnerConflict;
        auto& link = entry->link;
        if (HasAssociationDependents(*entry) && (context.action == Action::Detach || context.action == Action::Cancel ||
                                                 (context.action == Action::Attach && link.phase != LinkPhase::AttachedAccepted)))
            return ClockResult::OwnerConflict;
        if (!SequenceOwner::SamePrivateKey(link.key, context.key) || epoch.Generation() != context.token.ownerGeneration ||
            array.IssuedTag() != link.key.arrayTag)
            return ClockResult::OwnerConflict;
        namespace Native = NativePrivateAnimationHierarchyCalls;
        const auto finish = [&](ClockResult result)
        {
            if (entry->storage.GetState() == Storage::State::PublicationPinned || link.phase == LinkPhase::Uncertain)
            {
                link.phase = LinkPhase::Uncertain;
                self.sequences->RetainPrivateDependencyHeld(link.key);
            }
            self.Publish();
            return result;
        };
        if (link.phase == LinkPhase::Uncertain)
            return finish(ClockResult::OwnerConflict);
        const auto acknowledge = [&](std::uintptr_t target) noexcept
        {
            Receipt    ack;
            const auto result = Native::AcknowledgeSequencesHeld(epoch, entry->storage, target, context.owner, &ack);
            context.receipt.hierarchy = ack.hierarchy;
            context.receipt.acknowledged = ack.acknowledged;
            context.receipt.verified = result == ClockResult::Ok;
            return result;
        };
        if (context.action == Action::Cancel)
        {
            if (link.phase != LinkPhase::Planned)
                return finish(ClockResult::OwnerConflict);
            Values values;
            auto   result = Native::ReadHeld(epoch, entry->storage, context.owner, values);
            if (result == ClockResult::Ok)
                result = self.sequences->RequestPrivateReservationCancelHeld(link.key);
            if (result == ClockResult::Ok)
                context.receipt.accepted = context.receipt.verified = context.receipt.acknowledged = true;
            return finish(result);
        }
        if (context.action == Action::Attach || context.action == Action::Detach)
        {
            const bool detach = context.action == Action::Detach;
            if (!detach && link.phase == LinkPhase::Planned)
            {
                Values before;
                auto   result = Native::ReadHeld(epoch, entry->storage, context.owner, before);
                if (result != ClockResult::Ok)
                    return finish(result);
                // The durable parent command precedes both reservation and
                // entered pointer state. An actual receipt is the only phase
                // producer; lost ACK resumes the read-only path below.
                link.phase = LinkPhase::AttachEntered;
                result = Native::SetSequencesHeld(epoch, entry->storage, link.key.arrayTag, context.owner, &context.receipt);
                if (!context.receipt.entered)
                {
                    link.phase = LinkPhase::Planned;
                    return finish(result);
                }
                auto advanced =
                    self.sequences->AdvancePrivateDependencyHeld(link.key, SequenceOwner::PrivatePhase::Reserved, SequenceOwner::PrivatePhase::AttachEntered);
                if (advanced != ClockResult::Ok)
                {
                    link.phase = LinkPhase::Uncertain;
                    return finish(advanced);
                }
                if (!context.receipt.accepted)
                {
                    link.phase = LinkPhase::Uncertain;
                    return finish(result);
                }
                advanced = self.sequences->AdvancePrivateDependencyHeld(link.key, SequenceOwner::PrivatePhase::AttachEntered,
                                                                        SequenceOwner::PrivatePhase::AttachedAccepted);
                if (advanced != ClockResult::Ok)
                {
                    link.phase = LinkPhase::Uncertain;
                    return finish(advanced);
                }
                link.phase = LinkPhase::AttachedAccepted;
                if (result != ClockResult::Ok)
                    return finish(result);
            }
            if (detach && link.phase == LinkPhase::AttachedAccepted)
            {
                auto result = acknowledge(link.key.arrayTag);
                if (result != ClockResult::Ok)
                    return finish(result);
                link.phase = LinkPhase::DetachEntered;
                result = Native::SetSequencesHeld(epoch, entry->storage, 0, context.owner, &context.receipt);
                if (!context.receipt.entered)
                {
                    link.phase = LinkPhase::AttachedAccepted;
                    return finish(result);
                }
                auto advanced = self.sequences->AdvancePrivateDependencyHeld(link.key, SequenceOwner::PrivatePhase::AttachedAccepted,
                                                                             SequenceOwner::PrivatePhase::DetachEntered);
                if (advanced != ClockResult::Ok)
                {
                    link.phase = LinkPhase::Uncertain;
                    return finish(advanced);
                }
                if (!context.receipt.accepted)
                {
                    link.phase = LinkPhase::Uncertain;
                    return finish(result);
                }
                advanced = self.sequences->AdvancePrivateDependencyHeld(link.key, SequenceOwner::PrivatePhase::DetachEntered,
                                                                        SequenceOwner::PrivatePhase::DetachAccepted);
                if (advanced != ClockResult::Ok)
                {
                    link.phase = LinkPhase::Uncertain;
                    return finish(advanced);
                }
                link.phase = LinkPhase::DetachAccepted;
                if (result != ClockResult::Ok)
                    return finish(result);
            }
            if ((!detach && link.phase != LinkPhase::AttachedAccepted) || (detach && link.phase != LinkPhase::DetachAccepted))
                return finish(ClockResult::OwnerConflict);
            auto result = acknowledge(detach ? 0 : link.key.arrayTag);
            if (result == ClockResult::Ok && detach)
                result = self.sequences->RequestPrivateDetachReleaseHeld(link.key);
            return finish(result);
        }
        if (link.phase != LinkPhase::AttachedAccepted)
            return finish(ClockResult::OwnerConflict);
        if (context.action == Action::PostProcess)
            return finish(Native::PostProcessHeld(epoch, entry->storage, context.postProcess, context.owner, &ValidatePayloadHeld, &context,
                                                  &context.postReceipt, context.invokePostProcess));
        if (context.action == Action::Scalar)
        {
            if (HasAssociationDependents(*entry) && context.scalar == NativePrivateAnimationHierarchyCalls::ScalarOperation::SequenceCount)
            {
                Values     current;
                const auto observed = Native::ReadHeld(epoch, entry->storage, context.owner, current);
                if (observed != ClockResult::Ok)
                    return finish(observed);
                if (current.sequenceCount != static_cast<std::uint16_t>(context.bits))
                    return finish(ClockResult::OwnerConflict);
            }
            return finish(Native::SetScalarHeld(epoch, entry->storage, context.scalar, context.bits, context.owner, &context.receipt));
        }
        auto result = Native::ReadHeld(epoch, entry->storage, context.owner, context.values);
        if (result != ClockResult::Ok)
            return finish(result);
        if (context.action == Action::ReadSequences)
            context.pointer = link.key.arrayTag;
        if (context.action == Action::ReadSequence)
        {
            // Capacity is the owned allocation extent, independent of the
            // full source WORD logical count. No hierarchy-count clamp.
            if (static_cast<std::size_t>(context.index) >= array.Capacity())
                return finish(ClockResult::InvalidArgument);
            const auto displacement = static_cast<std::size_t>(context.index) * NativeCustomAnimSequenceStorageProfile::RowBytes;
            if (displacement > (std::numeric_limits<std::uintptr_t>::max)() - link.key.arrayTag)
                return finish(ClockResult::InvalidArgument);
            context.pointer = link.key.arrayTag + displacement;
        }
        return finish(ClockResult::Ok);
    }
    ClockResult NativePrivateAnimationHierarchyController::LinkedAction(std::uintptr_t image, const Token& token, CheckOwner owner, ActionContext& context,
                                                                        Receipt* output) noexcept
    {
        Link* journal = nullptr;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            const auto                              entry = Find(token);
            if (entry == entries.end() || !entry->link.active || !sequences)
                return ClockResult::BindingChanged;
            context.key = entry->link.key;
            journal = &entry->link;
        }
        // Never acquire the sequence flag while holding the hierarchy flag.
        // Its callback acquires the hierarchy flag inside the single native
        // gate. Active journals cannot be deleted before this actual receipt.
        const auto previous = journal->dependency;
        const auto result = sequences->WithPrivatePreparedArray(image, context.key, owner, &ParentAction, &context, &journal->dependency);
        PrivateHierarchyControllerDetail::PreserveReservation(journal->dependency, previous);
        if (output)
            *output = context.receipt;
        if (!operation.test_and_set(std::memory_order_acquire))
        {
            PrivateHierarchyControllerDetail::Scope held{operation};
            Find(token);
            Publish();
        }
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::SetSequences(std::uintptr_t image, const Token& token, std::uintptr_t array, CheckOwner owner,
                                                                        Receipt* receipt) noexcept
    {
        return SetSequencesImpl(image, token, array, owner, receipt, false);
    }
    ClockResult NativePrivateAnimationHierarchyController::SetSequencesImpl(std::uintptr_t image, const Token& value, std::uintptr_t source, CheckOwner owner,
                                                                            Receipt* output, bool retirement, std::uint64_t cleanupCommand,
                                                                            bool* releaseAcknowledged) noexcept
    {
        const auto token = value;
        const auto requested = source;
        if (releaseAcknowledged)
            *releaseAcknowledged = false;
        if (output)
            *output = {};
        Link*  journal = nullptr;
        Entry* cleanupEntry = nullptr;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            if (stopping && !retirement)
                return ClockResult::OwnerConflict;
            const auto entry = Find(token);
            if (entry == entries.end())
                return ClockResult::BindingChanged;
            if (entry->cleanupCommand && entry->cleanupCommand != cleanupCommand)
                return ClockResult::OwnerConflict;
            if (HasAssociationDependents(*entry))
            {
                const auto current = entry->link.active ? entry->link.key.arrayTag : std::uintptr_t{0};
                if (requested != current || (entry->link.active && entry->link.phase != LinkPhase::AttachedAccepted))
                    return ClockResult::OwnerConflict;
            }
            if (!entry->link.active)
            {
                if (!requested)
                {
                    const auto result = PrivateHierarchyControllerDetail::Calls::SetEmptySequences(image, entry->storage, token.ownerGeneration, owner, output);
                    Publish();
                    return result;
                }
                if (!sequences)
                    return ClockResult::NotInitialized;
                if (nextCommand == (std::numeric_limits<std::uint64_t>::max)())
                    return ClockResult::OwnerConflict;
                // Reserve the durable command before acquiring child authority
                // or entering a pointer write. No callback can orphan holds.
                entry->link.active = true;
                entry->link.key = {token.ownerGeneration, ++nextCommand, token.locator, requested};
            }
            if (!sequences || (requested && requested != entry->link.key.arrayTag) || entry->link.phase == LinkPhase::Uncertain)
                return ClockResult::OwnerConflict;
            journal = &entry->link;
            cleanupEntry = &*entry;
            Publish();
        }
        const auto key = journal->key;
        if (requested && journal->phase == LinkPhase::Planned)
        {
            const auto previous = journal->dependency;
            const auto result = sequences->ReserveIssuedPrivatePreparedArray(image, key, owner, &journal->dependency);
            PrivateHierarchyControllerDetail::PreserveReservation(journal->dependency, previous);
            if (result != ClockResult::Ok)
                return result;
        }
        if (!requested && journal->phase == LinkPhase::Planned && !journal->dependency.reserved)
        {
            // Exact returned pre-entry receipt proves no child reservation was
            // created. Cancel this one planned command, never an absent map.
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            const auto                              entry = Find(token);
            if (entry == entries.end() || !entry->link.active || !SequenceOwner::SamePrivateKey(entry->link.key, key))
                return ClockResult::BindingChanged;
            const auto result = PrivateHierarchyControllerDetail::Calls::SetEmptySequences(image, entry->storage, token.ownerGeneration, owner, output);
            if (result == ClockResult::Ok)
            {
                if (releaseAcknowledged)
                    *releaseAcknowledged = true;
                if (cleanupCommand)
                    entry->cleanupReleased = true;
                entry->link = {};
            }
            Publish();
            return result;
        }
        ActionContext context{this, token, key, owner, requested ? Action::Attach : Action::Detach};
        context.cleanupCommand = cleanupCommand;
        ClockResult result;
        const auto  previous = journal->dependency;
        if (!requested && journal->phase == LinkPhase::Planned)
        {
            context.action = Action::Cancel;
            result = sequences->CancelPrivatePreparedReservation(image, key, owner, &ParentAction, &context, &journal->dependency);
        }
        else
            result = sequences->WithPrivatePreparedArray(image, key, owner, &ParentAction, &context, &journal->dependency);
        PrivateHierarchyControllerDetail::PreserveReservation(journal->dependency, previous);
        // Capture the actual child owner's release receipt before Find may
        // consume this link journal. Neither raw pointer0 nor a missing map
        // can later replace this acknowledged dependency release.
        const bool actuallyReleased = journal->dependency.released;
        if (releaseAcknowledged)
            *releaseAcknowledged = actuallyReleased;
        if (cleanupCommand && actuallyReleased)
            cleanupEntry->cleanupReleased.store(true, std::memory_order_release);
        if (output)
            *output = context.receipt;
        if (!operation.test_and_set(std::memory_order_acquire))
        {
            PrivateHierarchyControllerDetail::Scope held{operation};
            Find(token);
            Publish();
        }
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::Read(std::uintptr_t image, const Token& value, CheckOwner owner, Values& output) noexcept
    {
        const auto token = value;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            const auto                              entry = Find(token);
            if (entry == entries.end())
                return ClockResult::BindingChanged;
            if (entry->cleanupCommand)
                return ClockResult::OwnerConflict;
            if (!entry->link.active)
            {
                const auto result = PrivateHierarchyControllerDetail::Calls::Read(image, entry->storage, token.ownerGeneration, owner, output);
                Publish();
                return result;
            }
        }
        ActionContext context{this, token, {}, owner, Action::Read};
        const auto    result = LinkedAction(image, token, owner, context, nullptr);
        if (result == ClockResult::Ok)
            output = context.values;
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::ReadSequences(std::uintptr_t image, const Token& value, CheckOwner owner,
                                                                         std::uintptr_t& output) noexcept
    {
        const auto token = value;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            const auto                              entry = Find(token);
            if (entry == entries.end())
                return ClockResult::BindingChanged;
            if (entry->cleanupCommand)
                return ClockResult::OwnerConflict;
            if (!entry->link.active)
            {
                Values     values;
                const auto result = PrivateHierarchyControllerDetail::Calls::Read(image, entry->storage, token.ownerGeneration, owner, values);
                if (result == ClockResult::Ok)
                    output = 0;
                Publish();
                return result;
            }
        }
        ActionContext context{this, token, {}, owner, Action::ReadSequences};
        const auto    result = LinkedAction(image, token, owner, context, nullptr);
        if (result == ClockResult::Ok)
            output = context.pointer;
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::ReadSequence(std::uintptr_t image, const Token& value, std::uint32_t indexValue, CheckOwner owner,
                                                                        std::uintptr_t& output) noexcept
    {
        const auto token = value;
        const auto index = indexValue;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            const auto                              entry = Find(token);
            if (entry == entries.end())
                return ClockResult::BindingChanged;
            if (entry->cleanupCommand)
                return ClockResult::OwnerConflict;
            if (!entry->link.active)
            {
                Values     values;
                const auto result = PrivateHierarchyControllerDetail::Calls::Read(image, entry->storage, token.ownerGeneration, owner, values);
                Publish();
                if (result != ClockResult::Ok)
                    return result;
                if (index)
                    return ClockResult::InvalidArgument;
                output = 0;
                return ClockResult::Ok;
            }
        }
        ActionContext context{this, token, {}, owner, Action::ReadSequence};
        context.index = index;
        const auto result = LinkedAction(image, token, owner, context, nullptr);
        if (result == ClockResult::Ok)
            output = context.pointer;
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::Destroy(std::uintptr_t image, const Token& token, CheckOwner owner, Receipt* receipt) noexcept
    {
        return DestroyImpl(image, token, owner, receipt, false);
    }
    ClockResult NativePrivateAnimationHierarchyController::DestroyImpl(std::uintptr_t image, const Token& value, CheckOwner owner, Receipt* output,
                                                                       bool retirement) noexcept
    {
        const auto token = value;
        if (output)
            *output = {};
        bool linked = false;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            const auto                              entry = Find(token);
            if (entry == entries.end())
                return ClockResult::BindingChanged;
            if (HasAssociationDependents(*entry))
                return ClockResult::OwnerConflict;
            if (entry->cleanupCommand)
                return ClockResult::OwnerConflict;
            linked = entry->link.active;
        }
        if (linked)
        {
            const auto detached = SetSequencesImpl(image, token, 0, owner, output, retirement);
            if (detached != ClockResult::Ok)
                return detached;
        }
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(token);
        if (entry == entries.end() || entry->link.active || HasAssociationDependents(*entry))
            return ClockResult::OwnerConflict;
        Receipt receipt;
        auto    result = PrivateHierarchyControllerDetail::Calls::Retire(image, entry->storage, token.ownerGeneration, owner, &receipt);
        if (result == ClockResult::OwnerConflict && entry->storage.GetState() == Storage::State::Entered)
        {
            // A zero-only pointer store may have lost its final ACK without a
            // child reservation. Finish that known read-only ACK, never replay
            // the store or adopt an unknown/published receiver.
            Receipt    ack;
            const auto acknowledged = PrivateHierarchyControllerDetail::Calls::SetEmptySequences(image, entry->storage, token.ownerGeneration, owner, &ack);
            if (acknowledged == ClockResult::Ok)
                result = PrivateHierarchyControllerDetail::Calls::Retire(image, entry->storage, token.ownerGeneration, owner, &receipt);
            else
                result = acknowledged;
        }
        if (receipt.retired)
            entries.erase(entry);
        if (output)
            *output = receipt;
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::PinPublication(const Token& value) noexcept
    {
        const auto token = value;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(token);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        // A live dynamic receiver still observes H even after node release.
        if (HasAssociationDependents(*entry))
            return ClockResult::OwnerConflict;
        if (entry->cleanupCommand)
            return ClockResult::OwnerConflict;
        const auto result = PrivateHierarchyControllerDetail::Calls::PinPublication(entry->storage, token.ownerGeneration);
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::RetireEmpty(std::uintptr_t image, std::uint64_t generationValue, std::size_t budgetValue,
                                                                       CheckOwner owner, RetireProgress& output) noexcept
    {
        const auto     generation = generationValue;
        const auto     budget = budgetValue;
        std::size_t    limit = 0;
        RetireProgress progress;
        ClockResult    first = ClockResult::Ok;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            stopping = true;
            limit = (std::min)(budget, entries.size());
            Publish();
        }
        while (progress.scanned < limit)
        {
            Token       token;
            bool        issued = false;
            Receipt     receipt;
            ClockResult result = ClockResult::OwnerConflict;
            {
                if (operation.test_and_set(std::memory_order_acquire))
                {
                    first = ClockResult::Busy;
                    break;
                }
                PrivateHierarchyControllerDetail::Scope held{operation};
                if (entries.empty())
                    break;
                auto entry = entries.begin();
                ++progress.scanned;
                if (entry->cleanupCommand || HasAssociationDependents(*entry))
                {
                    entries.splice(entries.end(), entries, entry);
                    continue;
                }
                if (entry->storage.OwnerGeneration() == generation)
                {
                    ++progress.attempted;
                    issued = entry->issued;
                    token = {generation, entry->storage.Address()};
                    if (!issued)
                    {
                        result = PrivateHierarchyControllerDetail::Calls::Retire(image, entry->storage, generation, owner, &receipt);
                        if (receipt.retired)
                        {
                            entries.erase(entry);
                            ++progress.released;
                        }
                        else
                            entries.splice(entries.end(), entries, entry);
                    }
                }
                else
                    entries.splice(entries.end(), entries, entry);
            }
            if (issued)
            {
                result = DestroyImpl(image, token, owner, &receipt, true);
                if (receipt.retired)
                    ++progress.released;
                else if (!operation.test_and_set(std::memory_order_acquire))
                {
                    PrivateHierarchyControllerDetail::Scope held{operation};
                    const auto                              entry = Find(token);
                    if (entry != entries.end())
                        entries.splice(entries.end(), entries, entry);
                }
            }
            if (result != ClockResult::Ok && first == ClockResult::Ok)
                first = result;
        }
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        progress.retained = entries.size();
        output = progress;
        Publish();
        return first;
    }
    ClockResult NativePrivateAnimationHierarchyController::ReadCleanupIssue(const Token& token, std::uint64_t& output) noexcept
    {
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(token);
        if (entry == entries.end() || !entry->issue)
            return ClockResult::BindingChanged;
        output = entry->issue;
        return ClockResult::Ok;
    }
    ClockResult NativePrivateAnimationHierarchyController::ClaimCleanupIntent(const Token& value, std::uintptr_t array, std::uint64_t command) noexcept
    {
        const auto token = value;
        if (!command || !token.ownerGeneration || !token.locator)
            return ClockResult::InvalidArgument;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(token);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        // A live dynamic receiver still observes H even after node release.
        if (HasAssociationDependents(*entry))
            return ClockResult::OwnerConflict;
        if (entry->cleanupCommand)
            return entry->cleanupCommand == command ? ClockResult::Ok : ClockResult::OwnerConflict;
        const auto state = entry->storage.GetState();
        if (state != Storage::State::Ready && state != Storage::State::Entered)
            return ClockResult::OwnerConflict;
        if (entry->link.active && (!array || entry->link.key.arrayTag != array || entry->link.phase == LinkPhase::Uncertain))
            return ClockResult::OwnerConflict;
        // This is a CPU intent, not a native/cache/detach qualification. It is
        // installed before Begin returns so an off-callback destructor cannot
        // lose the exact cleanup request while native admission is deferred.
        entry->cleanupCommand = command;
        entry->cleanupHadLink = entry->link.active;
        return ClockResult::Ok;
    }
    ClockResult NativePrivateAnimationHierarchyController::VerifyCleanupHeld(const Epoch& epoch, const Token& token, std::uint64_t command,
                                                                             CheckOwner owner) noexcept
    {
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(token);
        if (entry == entries.end() || entry->cleanupCommand != command || !command)
            return ClockResult::BindingChanged;
        if (HasAssociationDependents(*entry))
            return ClockResult::OwnerConflict;
        ClockResult result;
        if (!entry->link.active)
        {
            if (entry->cleanupHadLink && !entry->cleanupReleased)
                return ClockResult::OwnerConflict;
            Values values;
            result = PrivateHierarchyControllerDetail::Calls::ReadHeld(epoch, entry->storage, owner, values);
        }
        else
        {
            const auto& link = entry->link;
            if (link.key.arrayTag != epoch.ArrayTag() || link.key.ownerGeneration != epoch.Generation())
                return ClockResult::OwnerConflict;
            if (link.phase == LinkPhase::AttachedAccepted || link.phase == LinkPhase::DetachAccepted)
            {
                Receipt ack;
                result = PrivateHierarchyControllerDetail::Calls::AcknowledgeSequencesHeld(
                    epoch, entry->storage, link.phase == LinkPhase::AttachedAccepted ? link.key.arrayTag : 0, owner, &ack);
            }
            else if (link.phase == LinkPhase::Planned)
            {
                Values values;
                result = PrivateHierarchyControllerDetail::Calls::ReadHeld(epoch, entry->storage, owner, values);
            }
            else
                result = ClockResult::OwnerConflict;
        }
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::DetachCleanup(std::uintptr_t image, const Token& value, std::uint64_t command, CheckOwner owner,
                                                                         Receipt& output, bool& released) noexcept
    {
        const auto token = value;
        released = false;
        output = {};
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            const auto                              entry = Find(token);
            if (entry == entries.end() || !command || entry->cleanupCommand != command)
                return ClockResult::BindingChanged;
            if (HasAssociationDependents(*entry))
                return ClockResult::OwnerConflict;
            if (!entry->link.active)
            {
                if (entry->cleanupHadLink && !entry->cleanupReleased)
                    return ClockResult::OwnerConflict;
                if (entry->storage.GetState() == Storage::State::Entered)
                {
                    const auto result = PrivateHierarchyControllerDetail::Calls::Retire(image, entry->storage, token.ownerGeneration, owner, &output);
                    if (output.retired)
                    {
                        released = true;
                        entries.erase(entry);
                    }
                    Publish();
                    return result;
                }
                Values     values;
                const auto result = PrivateHierarchyControllerDetail::Calls::Read(image, entry->storage, token.ownerGeneration, owner, values);
                if (result == ClockResult::Ok)
                {
                    // Producer ownership plus a current expected-empty/cache0
                    // snapshot proves no attachment. A raw null alone cannot.
                    entry->cleanupReleased = released = true;
                    output.hierarchy = token.locator;
                    output.acknowledged = output.verified = true;
                }
                Publish();
                return result;
            }
        }
        const auto result = SetSequencesImpl(image, token, 0, owner, &output, true, command, &released);
        if (released && !operation.test_and_set(std::memory_order_acquire))
        {
            PrivateHierarchyControllerDetail::Scope held{operation};
            const auto                              entry = Find(token);
            if (entry != entries.end() && entry->cleanupCommand == command)
                entry->cleanupReleased = true;
            Publish();
        }
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::RetireCleanup(std::uintptr_t image, const Token& token, std::uint64_t command, CheckOwner owner,
                                                                         Receipt& output) noexcept
    {
        output = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope held{operation};
        const auto                              entry = Find(token);
        if (entry == entries.end() || !command || entry->cleanupCommand != command || entry->link.active || HasAssociationDependents(*entry))
            return ClockResult::BindingChanged;
        const auto result = PrivateHierarchyControllerDetail::Calls::Retire(image, entry->storage, token.ownerGeneration, owner, &output);
        // Preserve the actual CPU retirement receipt before erasure. No native
        // destructor runs, and a later lost acknowledgement cannot delete it twice.
        if (output.retired)
            entries.erase(entry);
        Publish();
        return result;
    }

}
