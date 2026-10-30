/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One durable issued serial owner for private native associations
 ****************************************************************************/
#include "NativeAnimationAssociationController.h"
#include "NativeAnimationAssociationGroupCopy.h"
#include "NativeAnimationAssociationClumpOrigin.h"
#include <limits>
#include <algorithm>
#include <new>
namespace mta::game_de
{
    namespace AnimationAssociationControllerDetail
    {
        struct Guard
        {
            std::atomic<bool>& busy;
            ~Guard() { busy.store(false, std::memory_order_release); }
        };
        bool Enter(std::atomic<bool>& busy) noexcept
        {
            bool expected = false;
            return busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }
    }
    struct NativeAnimationAssociationController::Entry
    {
        Token                                                     token{};
        NativeAnimationAssociationStorage                         storage;
        std::shared_ptr<NativeAnimationAssociationFacadeLifetime> code;
        Receipt                                                   retirement{};
        bool                                                      retirementRecorded = false;
    };
    NativeAnimationAssociationController::NativeAnimationAssociationController() noexcept = default;
    NativeAnimationAssociationController::~NativeAnimationAssociationController() = default;
    bool NativeAnimationAssociationController::CodePending(const Entry& entry) noexcept
    {
        return entry.code && (entry.code->views.load(std::memory_order_acquire) != 0 || entry.code->dirty.load(std::memory_order_acquire) ||
                              entry.code->offCreatorDrop.load(std::memory_order_acquire));
    }
    bool NativeAnimationAssociationController::CanErase(const Entry& entry) noexcept
    {
        return entry.storage.CanDiscard() &&
               (!entry.code || (entry.code->views.load(std::memory_order_acquire) == 0 && !entry.code->offCreatorDrop.load(std::memory_order_acquire) &&
                                entry.code->creator == std::this_thread::get_id()));
    }
    void NativeAnimationAssociationController::Publish() noexcept
    {
        bool any = false, code = false, fault = false;
        for (const auto& entry : records)
        {
            any |= entry->storage.IsRetained();
            code |= CodePending(*entry);
            const auto state = entry->storage.GetState();
            fault |= state == NativeAnimationAssociationStorage::State::FaultDebt || state == NativeAnimationAssociationStorage::State::PublicationPinned ||
                     state == NativeAnimationAssociationStorage::State::ConstructEntered || state == NativeAnimationAssociationStorage::State::RetireEntered ||
                     state == NativeAnimationAssociationStorage::State::NodeFreeEntered ||
                     state == NativeAnimationAssociationStorage::State::NodeClearEntered || state == NativeAnimationAssociationStorage::State::CopyEntered;
        }
        retained.store(any, std::memory_order_release);
        codeRetained.store(code, std::memory_order_release);
        recovery.store(fault, std::memory_order_release);
    }
    void NativeAnimationAssociationController::CloseFacadeIssuance(std::uint64_t generation) noexcept
    {
        // Stop can run inside the original call while busy is held. Closure
        // therefore raises a monotonic barrier without waiting or reopening.
        auto closed = closedFacadeThrough.load(std::memory_order_acquire);
        while (generation > closed && !closedFacadeThrough.compare_exchange_weak(closed, generation, std::memory_order_acq_rel))
        {
        }
    }
    ClockResult NativeAnimationAssociationController::PruneFacadeCode(std::uintptr_t image, std::uint64_t generation, CheckOwner owner) noexcept
    {
        if (!image || !generation || !owner)
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        if (facadeGeneration && facadeGeneration != generation)
            return ClockResult::OwnerConflict;
        for (auto at = records.begin(); at != records.end();)
        {
            auto& entry = **at;
            if (!entry.code || entry.token.ownerGeneration != generation)
            {
                ++at;
                continue;
            }
            if (entry.code->image != image || entry.code->token.ownerGeneration != generation || entry.code->token.serial != entry.token.serial ||
                entry.code->creator != std::this_thread::get_id())
                return ClockResult::OwnerConflict;
            // A CPU destructor only drops a count. This creator callback runs
            // afterwards and can erase code-only released native records.
            if (CanErase(entry))
                at = records.erase(at);
            else
                ++at;
        }
        Publish();
        return owner();
    }
    NativeAnimationAssociationController::Entry* NativeAnimationAssociationController::Find(std::uint64_t generation, const Token& token) noexcept
    {
        if (!generation || token.ownerGeneration != generation || !token.serial)
            return nullptr;
        for (const auto& entry : records)
            if (entry->token.ownerGeneration == generation && entry->token.serial == token.serial)
                return entry.get();
        return nullptr;
    }
    ClockResult NativeAnimationAssociationController::CopyFromGroup(std::uintptr_t image, std::uint64_t generation,
                                                                    const NativeAnimationAssociationGroupReferenceToken& group, std::uint32_t animationId,
                                                                    CheckOwner owner, std::uintptr_t& output,
                                                                    NativeAnimationAssociationCalls::GroupCopyReceipt* receipt) noexcept
    {
        return CopyFromGroupImpl(image, generation, group, animationId, owner, output, nullptr, receipt);
    }
    ClockResult NativeAnimationAssociationController::CopyFromGroupIssued(std::uintptr_t image, std::uint64_t generation,
                                                                          const NativeAnimationAssociationGroupReferenceToken& group, std::uint32_t animationId,
                                                                          CheckOwner owner, NativeAnimationAssociationIssuedCopy& output,
                                                                          NativeAnimationAssociationCalls::GroupCopyReceipt* receipt) noexcept
    {
        std::uintptr_t returned = 0;
        return CopyFromGroupImpl(image, generation, group, animationId, owner, returned, &output, receipt);
    }
    ClockResult NativeAnimationAssociationController::CopyFromGroupImpl(std::uintptr_t image, std::uint64_t generation,
                                                                        const NativeAnimationAssociationGroupReferenceToken& group, std::uint32_t animationId,
                                                                        CheckOwner owner, std::uintptr_t& output, NativeAnimationAssociationIssuedCopy* issued,
                                                                        NativeAnimationAssociationCalls::GroupCopyReceipt* receipt) noexcept
    {
        const auto                                        request = group;
        const auto                                        animation = animationId;
        NativeAnimationAssociationCalls::GroupCopyReceipt next;
        if (receipt)
            *receipt = next;
        if (!generation || request.ownerGeneration != generation)
            return ClockResult::OwnerConflict;
        if (issued && (issued->token.ownerGeneration || issued->token.serial || issued->interfaceAddress || issued->code.IsIssued()))
            return ClockResult::InvalidArgument;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        if (serial == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::OwnerConflict;
        if (issued)
        {
            if (!image || !owner || generation <= closedFacadeThrough.load(std::memory_order_acquire) || generation < facadeGeneration)
                return ClockResult::OwnerConflict;
            const auto checked = owner();
            if (checked != ClockResult::Ok)
                return checked;
            if (facadeGeneration != generation)
            {
                // A new generation may follow prior aggregate quiescence;
                // raw records already issued in this same new epoch are fine.
                for (const auto& entry : records)
                    if (entry->token.ownerGeneration != generation && (CodePending(*entry) || !CanErase(*entry)))
                        return ClockResult::OwnerConflict;
                for (auto at = records.begin(); at != records.end();)
                    if ((*at)->token.ownerGeneration != generation)
                        at = records.erase(at);
                    else
                        ++at;
                facadeGeneration = generation;
            }
        }
        // Both products use one actual Entry/serial. The typed product also
        // reserves code lifetime before the original allocator may execute.
        try
        {
            auto entry = std::make_unique<Entry>();
            entry->token = {generation, serial + 1};
            if (issued)
                entry->code.reset(new NativeAnimationAssociationFacadeLifetime(image, entry->token));
            records.push_back(std::move(entry));
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        auto& entry = *records.back();
        serial = entry.token.serial;
        if (issued && generation <= closedFacadeThrough.load(std::memory_order_acquire))
        {
            records.pop_back();
            Publish();
            return ClockResult::OwnerConflict;
        }
        std::uintptr_t returned = 0;
        auto           result = NativeAnimationAssociationCalls::CopyFromGroup(image, entry.storage, entry.token, request, animation, owner, returned, &next);
        if (issued && result == ClockResult::Ok)
        {
            if (!next.entered || !next.returned || !next.verified || next.issued.ownerGeneration != entry.token.ownerGeneration ||
                next.issued.serial != entry.token.serial || next.returnedInterface != returned ||
                (next.returnedNull ? (returned != 0 || !entry.storage.CanDiscard())
                                   : (!returned || entry.storage.GetState() != NativeAnimationAssociationStorage::State::LiveUnpublished ||
                                      next.resources.receiver != returned)))
                result = ClockResult::BindingChanged;
            if (result == ClockResult::Ok)
                result = owner();
            // Closure during the original call cannot publish another facade.
            // Its actual native resource remains in the same existing ledger.
            if (result == ClockResult::Ok && generation <= closedFacadeThrough.load(std::memory_order_acquire))
                result = ClockResult::OwnerConflict;
            if (result == ClockResult::Ok && !next.returnedNull)
            {
                entry.code->views.store(1, std::memory_order_release);
                issued->token = entry.token;
                issued->interfaceAddress = returned;
                issued->code = NativeAnimationAssociationFacadeView(entry.code);
            }
        }
        if (result == ClockResult::Ok)
            output = returned;
        if (CanErase(entry))
            records.pop_back();
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::ResolveIssuedInterface(std::uintptr_t image, std::uint64_t generation, std::uintptr_t candidate,
                                                                             CheckOwner owner, NativeAnimationAssociationIssuedCopy& output) noexcept
    {
        if (!image || !generation || !candidate || !owner || output.token.ownerGeneration || output.token.serial || output.interfaceAddress ||
            output.code.IsIssued())
            return ClockResult::InvalidArgument;
        if (generation <= closedFacadeThrough.load(std::memory_order_acquire))
            return ClockResult::OwnerConflict;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto                                        finish = [&](ClockResult value)
        {
            Publish();
            return value;
        };
        if (generation <= closedFacadeThrough.load(std::memory_order_acquire) || generation < facadeGeneration)
            return finish(ClockResult::OwnerConflict);
        if (facadeGeneration != generation)
        {
            // Raw Create records may precede their first CPU wrapper in this
            // epoch. Prior native/code debt must still be fully quiescent.
            for (const auto& entry : records)
                if (entry->token.ownerGeneration != generation && (CodePending(*entry) || !CanErase(*entry)))
                    return finish(ClockResult::OwnerConflict);
            for (auto at = records.begin(); at != records.end();)
                if ((*at)->token.ownerGeneration != generation)
                    at = records.erase(at);
                else
                    ++at;
            facadeGeneration = generation;
        }
        Entry* selected = nullptr;
        for (const auto& entry : records)
        {
            if (entry->token.ownerGeneration != generation || !NativeAnimationAssociationCalls::MatchesIssuedInterface(entry->storage, generation, candidate))
                continue;
            if (selected)
                return finish(ClockResult::BindingChanged);
            selected = entry.get();
        }
        if (!selected)
            return finish(ClockResult::OwnerConflict);
        std::uintptr_t confirmed = 0;
        if ((result = NativeAnimationAssociationCalls::ReadInterface(image, selected->storage, generation, owner, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (confirmed != candidate)
            return finish(ClockResult::BindingChanged);
        if (!selected->code)
        {
            try
            {
                // This allocates CPU bookkeeping only. The native outer and
                // serial already belong to this same actual Entry; no source
                // row, constructor/ref or second native allocation is adopted.
                selected->code.reset(new NativeAnimationAssociationFacadeLifetime(image, selected->token));
            }
            catch (...)
            {
                return finish(ClockResult::WriteFailure);
            }
        }
        auto& code = *selected->code;
        if (code.image != image || code.token.ownerGeneration != generation || code.token.serial != selected->token.serial ||
            code.creator != std::this_thread::get_id() || code.offCreatorDrop.load(std::memory_order_acquire))
            return finish(ClockResult::OwnerConflict);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        if (generation <= closedFacadeThrough.load(std::memory_order_acquire))
            return finish(ClockResult::OwnerConflict);
        auto views = code.views.load(std::memory_order_acquire);
        do
        {
            if (views == (std::numeric_limits<std::size_t>::max)())
                return finish(ClockResult::OwnerConflict);
        } while (!code.views.compare_exchange_weak(views, views + 1, std::memory_order_acq_rel));
        if (code.offCreatorDrop.load(std::memory_order_acquire) || generation <= closedFacadeThrough.load(std::memory_order_acquire))
        {
            code.views.fetch_sub(1, std::memory_order_acq_rel);
            return finish(ClockResult::OwnerConflict);
        }
        output.token = selected->token;
        output.interfaceAddress = confirmed;
        output.code = NativeAnimationAssociationFacadeView(selected->code);
        return finish(ClockResult::Ok);
    }
    ClockResult NativeAnimationAssociationController::ReadInterface(std::uintptr_t image, std::uint64_t generation, const Token& requested, CheckOwner owner,
                                                                    std::uintptr_t& output) noexcept
    {
        const auto token = requested;
        if (!image || !generation || !owner)
            return ClockResult::InvalidArgument;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, token);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::ReadInterface(image, entry->storage, generation, owner, output);
        // Constructed identity reads preserve Snapshot's ability to pin a
        // changed native record. Publish that real recovery state as Read does.
        Publish();
        return result;
    }
    ClockResult NativeAnimationAssociationController::Create(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, Token& output,
                                                             Receipt* receipt) noexcept
    {
        Receipt next;
        if (receipt)
            *receipt = next;
        if (!generation)
            return ClockResult::InvalidArgument;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        if (serial == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::OwnerConflict;
        // Reserve the durable record before the first native allocator entry.
        // CPU OOM therefore cannot orphan an acknowledged outer allocation.
        try
        {
            auto entry = std::make_unique<Entry>();
            entry->token = {generation, serial + 1};
            records.push_back(std::move(entry));
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        auto& entry = *records.back();
        serial = entry.token.serial;
        const auto result = NativeAnimationAssociationCalls::Create(image, entry.storage, generation, owner, &next);
        if (result == ClockResult::Ok)
            output = entry.token;
        if (entry.storage.CanDiscard())
            records.pop_back();
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::Construct(std::uintptr_t image, std::uint64_t generation, const Token& token, std::uintptr_t source,
                                                                CheckOwner owner, std::uintptr_t& output, Receipt* receipt) noexcept
    {
        const auto request = token;
        const auto locator = source;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::Construct(image, entry->storage, generation, locator, owner, output, &next);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::ApplyClump(std::uintptr_t image, std::uint64_t generation, const Token& requested,
                                                                 NativeAnimationAssociationClumpOperation     operation,
                                                                 const NativeAnimationAssociationClumpOrigin& origin, CheckOwner owner, std::uintptr_t& output,
                                                                 NativeAnimationAssociationCalls::ClumpReceipt*           receipt,
                                                                 const NativeAnimationAssociationCalls::ClumpInvocations* invocations) noexcept
    {
        const auto                                    token = requested;
        NativeAnimationAssociationCalls::ClumpReceipt next;
        if (receipt)
            *receipt = next;
        if (!generation || origin.Image() != image || origin.Generation() != generation || origin.Receiver().ownerGeneration != token.ownerGeneration ||
            origin.Receiver().serial != token.serial)
            return ClockResult::OwnerConflict;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, token);
        if (!entry)
            return ClockResult::OwnerConflict;
        // Find supplies the actual issued storage. The synchronous origin
        // never adopts a raw104 receiver or extends a clump/ped lease.
        const auto result =
            NativeAnimationAssociationCalls::ApplyClump(image, entry->storage, generation, operation, origin, owner, output, &next, invocations);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::Read(std::uintptr_t image, std::uint64_t generation, const Token& token, CheckOwner owner,
                                                           Values& output) noexcept
    {
        const auto request = token;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::Read(image, entry->storage, generation, owner, output);
        Publish();
        return result;
    }
    ClockResult NativeAnimationAssociationController::ReadLength(std::uintptr_t image, std::uint64_t generation, const Token& token, CheckOwner owner,
                                                                 std::uint32_t& output) noexcept
    {
        const auto request = token;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::ReadLength(image, entry->storage, generation, owner, output);
        Publish();
        return result;
    }
    ClockResult NativeAnimationAssociationController::Set(std::uintptr_t image, std::uint64_t generation, const Token& token, Scalar scalar, std::uint32_t bits,
                                                          CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto request = token;
        const auto value = bits;
        const auto field = scalar;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::Set(image, entry->storage, generation, field, value, owner, &next);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::SetCurrentProgress(std::uintptr_t image, std::uint64_t generation, const Token& token, std::uint32_t bits,
                                                                         CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto request = token;
        const auto progress = bits;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::SetCurrentProgress(image, entry->storage, generation, progress, owner, &next);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::FreeNodeArray(std::uintptr_t image, std::uint64_t generation, const Token& token, CheckOwner owner,
                                                                    Receipt* receipt) noexcept
    {
        const auto request = token;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::FreeNodeArray(image, entry->storage, generation, owner, &next);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::RetireOne(std::uintptr_t image, std::uint64_t generation, const Token& token, CheckOwner owner,
                                                                Receipt* receipt) noexcept
    {
        const auto request = token;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        ClockResult result = ClockResult::Ok;
        if (entry->storage.CanDiscard())
        {
            if (!entry->retirementRecorded)
                return ClockResult::OwnerConflict;
            next = entry->retirement;
        }
        else
        {
            result = NativeAnimationAssociationCalls::Retire(image, entry->storage, generation, owner, &next);
            if (entry->storage.CanDiscard())
            {
                entry->retirement = next;
                entry->retirementRecorded = true;
            }
        }
        // Native release and facade deletion are independent. Keep a released
        // Entry while code views exist; repeated retirement uses its real ACK.
        if (!entry->code && CanErase(*entry))
            for (auto at = records.begin(); at != records.end(); ++at)
                if (at->get() == entry)
                {
                    records.erase(at);
                    break;
                }
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::Retire(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, std::uint32_t budget,
                                                             RetireProgress& output) noexcept
    {
        RetireProgress next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        ClockResult                                 result = ClockResult::Ok;
        const auto                                  limit = (std::min)(static_cast<std::size_t>(budget), records.size());
        for (std::size_t index = 0; index != limit; ++index)
        {
            auto  at = records.begin();
            auto& entry = **at;
            ++next.scanned;
            if (entry.token.ownerGeneration != generation || entry.storage.CanDiscard())
            {
                // Code-only records require no native retirement. Only the
                // separate creator pump prunes their final released views.
                records.splice(records.end(), records, at);
                continue;
            }
            Receipt receipt;
            ++next.attempted;
            const auto current = NativeAnimationAssociationCalls::Retire(image, entry.storage, generation, owner, &receipt);
            if (entry.storage.CanDiscard())
            {
                entry.retirement = receipt;
                entry.retirementRecorded = true;
                ++next.released;
                if (!entry.code && CanErase(entry))
                    records.erase(at);
                else
                    records.splice(records.end(), records, at);
            }
            else
                records.splice(records.end(), records, at);
            if (current != ClockResult::Ok && result == ClockResult::Ok)
                result = current;
            if (receipt.entered && current != ClockResult::Ok)
                break;
        }
        Publish();
        for (const auto& entry : records)
        {
            if (entry->storage.IsRetained())
                ++next.retained;
            if (CodePending(*entry))
                ++next.codeRetained;
        }
        output = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::ResolveHierarchy(std::uintptr_t image, std::uint64_t generation, const Token& token, CheckOwner owner,
                                                                       NativeAnimationHierarchyReferenceToken& output) noexcept
    {
        const auto request = token;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::ResolveHierarchy(image, entry->storage, generation, owner, output);
        Publish();
        return result;
    }
    ClockResult NativeAnimationAssociationController::RestrictToBones(std::uintptr_t image, std::uint64_t generation, const Token& token, std::uint64_t mask,
                                                                      CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto request = token;
        const auto bits = mask;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::RestrictToBones(image, entry->storage, generation, bits, owner, &next);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::RestrictToBonesOf(std::uintptr_t image, std::uint64_t generation, const Token& token,
                                                                        std::uintptr_t original, CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto request = token;
        const auto source = original;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::RestrictToBonesOf(image, entry->storage, generation, source, owner, &next);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }

}
