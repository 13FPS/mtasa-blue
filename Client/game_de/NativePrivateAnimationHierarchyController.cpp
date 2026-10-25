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
        const bool fault = std::any_of(entries.begin(), entries.end(),
                                       [](const Entry& entry)
                                       {
                                           const auto state = entry.storage.GetState();
                                           return state == Storage::State::Failed || state == Storage::State::PublicationPinned ||
                                                  state == Storage::State::Entered ||
                                                  (entry.link.active && (entry.link.phase != LinkPhase::AttachedAccepted || entry.link.dependency.uncertain));
                                       });
        recovery.store(!entries.empty() && (stopping || fault), std::memory_order_release);
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
        try
        {
            entries.emplace_back(requested);
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        const auto entry = std::prev(entries.end());
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
            linked = entry->link.active;
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
        // Pointer0 alone is not a detach ACK. A reserved/pending/linked parent
        // keeps all dependency journals until its separate matching command.
        if (entry->link.active)
            return ClockResult::OwnerConflict;
        const auto result = PrivateHierarchyControllerDetail::Calls::RemoveEmptySequences(image, entry->storage, token.ownerGeneration, owner, output);
        Publish();
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
        auto& link = entry->link;
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
        if (context.action == Action::Scalar)
            return finish(Native::SetScalarHeld(epoch, entry->storage, context.scalar, context.bits, context.owner, &context.receipt));
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
                                                                            Receipt* output, bool retirement) noexcept
    {
        const auto token = value;
        const auto requested = source;
        if (output)
            *output = {};
        Link* journal = nullptr;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            PrivateHierarchyControllerDetail::Scope held{operation};
            if (stopping && !retirement)
                return ClockResult::OwnerConflict;
            const auto entry = Find(token);
            if (entry == entries.end())
                return ClockResult::BindingChanged;
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
                entry->link = {};
            Publish();
            return result;
        }
        ActionContext context{this, token, key, owner, requested ? Action::Attach : Action::Detach};
        ClockResult   result;
        const auto    previous = journal->dependency;
        if (!requested && journal->phase == LinkPhase::Planned)
        {
            context.action = Action::Cancel;
            result = sequences->CancelPrivatePreparedReservation(image, key, owner, &ParentAction, &context, &journal->dependency);
        }
        else
            result = sequences->WithPrivatePreparedArray(image, key, owner, &ParentAction, &context, &journal->dependency);
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
        if (entry == entries.end() || entry->link.active)
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
}
