/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain native sequence storage across failed publication and End
 ****************************************************************************/
#include "NativeCustomAnimSequenceController.h"
#include <algorithm>
#include <iterator>
#include <limits>
#include <new>

namespace mta::game_de
{
    namespace CustomAnimSequenceControllerDetail
    {
        struct Operation
        {
            std::atomic_flag& value;
            ~Operation() { value.clear(std::memory_order_release); }
        };
    }
    void NativeCustomAnimSequenceController::Publish() noexcept
    {
        retained.store(!entries.empty(), std::memory_order_release);
        bool needsRecovery = stopping && !entries.empty();
        for (const auto& entry : entries)
            needsRecovery = needsRecovery || (!entry.issued && entry.storage.HasOutstandingResource()) || entry.storage.GetState() == Storage::State::FaultDebt;
        recovery.store(needsRecovery, std::memory_order_release);
        if (entries.empty())
        {
            ownerGeneration = 0;
            stopping = false;
        }
    }
    NativeCustomAnimSequenceController::Entries::iterator NativeCustomAnimSequenceController::Find(std::uint64_t generation, std::uintptr_t issuedTag) noexcept
    {
        for (auto entry = entries.begin(); entry != entries.end(); ++entry)
            if (entry->issued && entry->storage.OwnerGeneration() == generation && entry->storage.IssuedTag() == issuedTag)
                return entry;
        return entries.end();
    }
    ClockResult NativeCustomAnimSequenceController::Allocate(std::uintptr_t image, std::uint64_t generation, std::size_t capacity, CheckOwner owner,
                                                             std::uintptr_t& output, Receipt* receipt, InvokeMalloc invoke) noexcept
    {
        return AllocateResource(image, generation, capacity, Storage::Kind::SequenceArray, owner, output, receipt, invoke);
    }
    ClockResult NativeCustomAnimSequenceController::AllocateFrames(std::uintptr_t image, std::uint64_t generation, std::uint32_t bytes, CheckOwner owner,
                                                                   std::uintptr_t& output, Receipt* receipt, InvokeMalloc invoke) noexcept
    {
        return AllocateResource(image, generation, bytes, Storage::Kind::KeyFrames, owner, output, receipt, invoke);
    }
    ClockResult NativeCustomAnimSequenceController::AllocateResource(std::uintptr_t image, std::uint64_t generation, std::size_t amount, Storage::Kind kind,
                                                                     CheckOwner owner, std::uintptr_t& output, Receipt* receipt, InvokeMalloc invoke) noexcept
    {
        if (receipt)
            *receipt = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!generation || (ownerGeneration && ownerGeneration != generation) || stopping)
            return ClockResult::OwnerConflict;
        if (!owner)
            return ClockResult::NotInitialized;
        if (kind == Storage::Kind::KeyFrames && amount > (std::numeric_limits<std::uint32_t>::max)())
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (!amount)
        {
            output = 0;
            return ClockResult::Ok;
        }
        // Allocate the stable CPU journal before native entry. No list node,
        // lookup insertion or output allocation may fail after GTA allocates.
        try
        {
            entries.emplace_back();
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
        ownerGeneration = generation;
        retained.store(true, std::memory_order_release);
        auto    entry = std::prev(entries.end());
        Receipt local;
        result =
            kind == Storage::Kind::SequenceArray
                ? NativeCustomAnimSequenceStorageCalls::Allocate(image, entry->storage, generation, amount, owner, &local, invoke)
                : NativeAnimationKeyFrameStorageCalls::Allocate(image, entry->storage, generation, static_cast<std::uint32_t>(amount), owner, &local, invoke);
        if (result == ClockResult::Ok && local.readyTag)
            entry->issued = true;
        if (entry->storage.CanDiscard())
            entries.erase(entry);
        Publish();
        if (receipt)
            *receipt = local;
        if (result == ClockResult::Ok)
            output = local.readyTag;
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::FreeStorage(std::uintptr_t image, std::uint64_t generation, std::uintptr_t issuedTag, CheckOwner owner,
                                                                Receipt* receipt, InvokeFree invoke) noexcept
    {
        return FreeResource(image, generation, issuedTag, Storage::Kind::SequenceArray, owner, receipt, invoke);
    }
    ClockResult NativeCustomAnimSequenceController::FreeFrames(std::uintptr_t image, std::uint64_t generation, std::uintptr_t issuedBase, CheckOwner owner,
                                                               Receipt* receipt, InvokeFree invoke) noexcept
    {
        return FreeResource(image, generation, issuedBase, Storage::Kind::KeyFrames, owner, receipt, invoke);
    }
    ClockResult NativeCustomAnimSequenceController::FreeResource(std::uintptr_t image, std::uint64_t generation, std::uintptr_t issuedTag, Storage::Kind kind,
                                                                 CheckOwner owner, Receipt* receipt, InvokeFree invoke) noexcept
    {
        if (receipt)
            *receipt = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!generation || (ownerGeneration && ownerGeneration != generation))
            return ClockResult::OwnerConflict;
        if (!owner)
            return ClockResult::NotInitialized;
        const auto checked = owner();
        if (checked != ClockResult::Ok)
            return checked;
        if (!issuedTag)
            return ClockResult::Ok;
        auto entry = Find(generation, issuedTag);
        if (entry == entries.end() || entry->storage.GetKind() != kind)
            return ClockResult::BindingChanged;
        Receipt    local;
        const auto result = kind == Storage::Kind::SequenceArray
                                ? NativeCustomAnimSequenceStorageCalls::FreeStorage(image, entry->storage, generation, owner, &local, invoke)
                                : NativeAnimationKeyFrameStorageCalls::FreeStorage(image, entry->storage, generation, owner, &local, invoke);
        // A returned native free is already consumed even if its post-call
        // acknowledgement failed. Erasing that released record cannot replay
        // free or infer anything by probing a possibly reused native address.
        if (entry->storage.CanDiscard())
            entries.erase(entry);
        Publish();
        if (receipt)
            *receipt = local;
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::SelectRow(const NativeAnimationSequenceReferenceToken& token, Entries::iterator& entry,
                                                              RowState*& row) noexcept
    {
        if (!token.ownerGeneration || ownerGeneration != token.ownerGeneration || stopping)
            return ClockResult::OwnerConflict;
        entry = Find(token.ownerGeneration, token.firstTag);
        if (entry == entries.end() || entry->storage.GetKind() != Storage::Kind::SequenceArray ||
            entry->storage.GetState() != Storage::State::LiveUnpublished || token.index >= entry->storage.Capacity())
            return ClockResult::BindingChanged;
        for (auto& value : entry->rows)
            if (value.index == token.index)
            {
                row = &value;
                return ClockResult::Ok;
            }
        try
        {
            // Allocation initialized every capacity row. Its logical union
            // starts low-defined, even if the first observation comes later.
            entry->rows.emplace_back(token.index);
            row = &entry->rows.back();
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::CheckSequenceFramePointer(std::uintptr_t image, std::uint64_t generation, std::uintptr_t pointer,
                                                                              CheckOwner owner) noexcept
    {
        if (!pointer)
            return ClockResult::Ok;
        for (const auto& entry : entries)
        {
            const auto& storage = entry.storage;
            if (!entry.issued || storage.OwnerGeneration() != generation || storage.GetKind() != Storage::Kind::KeyFrames ||
                storage.GetState() != Storage::State::LiveUnpublished || pointer < storage.IssuedTag())
                continue;
            const auto displacement = pointer - storage.IssuedTag();
            if (displacement < storage.ByteSize())
                // Pointer observation alone grants no frame-format/count
                // guarantee and cannot extend the allocation's lifetime.
                return NativeAnimationSequenceRowCalls::CheckFramePointer(image, storage, generation, pointer, owner);
        }
        return ClockResult::BindingChanged;
    }
    ClockResult NativeCustomAnimSequenceController::ReadSequenceValue(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                      CheckOwner owner, NativeAnimationSequenceValues* output, std::uint32_t* hash) noexcept
    {
        const auto request = token;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        auto      entry = entries.end();
        RowState* row = nullptr;
        if ((result = SelectRow(request, entry, row)) != ClockResult::Ok)
            return result;
        NativeAnimationSequenceRowCalls::Value physical;
        result = NativeAnimationSequenceRowCalls::Read(image, entry->storage, request.ownerGeneration, request.index, owner, physical);
        if (result != ClockResult::Ok)
            return result;
        if (hash)
        {
            std::uint32_t next = 0;
            if (!row->logical.TryReadHash(next))
                return ClockResult::NotInitialized;
            *hash = next;
            return ClockResult::Ok;
        }
        if ((result = CheckSequenceFramePointer(image, request.ownerGeneration, physical.keyFrames, owner)) != ClockResult::Ok)
            return result;
        if (physical.keyFrames)
        {
            // Frame qualification has its own owner/heap observations. A
            // final row bookend must still describe that same pointer and
            // scalar snapshot before the compound view can be published.
            NativeAnimationSequenceRowCalls::Value after;
            result = NativeAnimationSequenceRowCalls::Read(image, entry->storage, request.ownerGeneration, request.index, owner, after);
            if (result != ClockResult::Ok)
                return result;
            if (physical.row != after.row || physical.keyFrames != after.keyFrames || physical.boneTag != after.boneTag || physical.flags != after.flags ||
                physical.frameCount != after.frameCount)
                return ClockResult::BindingChanged;
        }
        std::uint16_t bone = 0;
        if (!row->logical.TryReadBoneTag(bone))
            return ClockResult::NotInitialized;
        const NativeAnimationSequenceValues next{physical.row, physical.keyFrames, physical.frameCount, bone, (physical.flags & 8) != 0};
        *output = next;
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::ReadSequence(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token, CheckOwner owner,
                                                                 NativeAnimationSequenceValues& output) noexcept
    {
        return ReadSequenceValue(image, token, owner, &output, nullptr);
    }
    ClockResult NativeCustomAnimSequenceController::ReadSequenceHash(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token, CheckOwner owner,
                                                                     std::uint32_t& output) noexcept
    {
        return ReadSequenceValue(image, token, owner, nullptr, &output);
    }
    ClockResult NativeCustomAnimSequenceController::InitializeSequence(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                       CheckOwner owner, RowReceipt* receipt) noexcept
    {
        const auto request = token;
        if (receipt)
            *receipt = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        auto      entry = entries.end();
        RowState* row = nullptr;
        if ((result = SelectRow(request, entry, row)) != ClockResult::Ok)
            return result;
        RowReceipt local;
        result = NativeAnimationSequenceRowCalls::Initialize(image, entry->storage, request.ownerGeneration, request.index, owner, &local);
        // A completed physical assignment remains accepted after lost
        // acknowledgement. Keep logical state and all allocation journals;
        // the runtime stops rather than replaying or rewinding the write.
        if (local.accepted)
            row->logical.Initialize();
        Publish();
        if (receipt)
            *receipt = local;
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::SetSequenceBoneTag(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                       std::int32_t value, CheckOwner owner, RowReceipt* receipt) noexcept
    {
        const auto request = token;
        if (receipt)
            *receipt = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        auto      entry = entries.end();
        RowState* row = nullptr;
        if ((result = SelectRow(request, entry, row)) != ClockResult::Ok)
            return result;
        RowReceipt local;
        result = NativeAnimationSequenceRowCalls::SetBoneTag(image, entry->storage, request.ownerGeneration, request.index, value, owner, &local);
        if (local.accepted)
            row->logical.SetBoneTag(value);
        Publish();
        if (receipt)
            *receipt = local;
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::PinPublication(std::uint64_t generation, std::uintptr_t issuedTag,
                                                                   std::uintptr_t hierarchyAffinity) noexcept
    {
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!generation || ownerGeneration != generation || stopping)
            return ClockResult::OwnerConflict;
        auto entry = Find(generation, issuedTag);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        const auto result = NativeCustomAnimSequenceStorageCalls::PinPublication(entry->storage, generation, hierarchyAffinity);
        Publish();
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::Retire(std::uintptr_t image, std::uint64_t generation, std::size_t budget, CheckOwner owner,
                                                           RetireProgress& progress, InvokeFree invoke) noexcept
    {
        progress = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!generation || (ownerGeneration && ownerGeneration != generation))
            return ClockResult::OwnerConflict;
        if (!owner)
            return ClockResult::NotInitialized;
        const auto checked = owner();
        if (checked != ClockResult::Ok)
            return checked;
        stopping = !entries.empty();
        ClockResult result = ClockResult::Ok;
        const auto  limit = (std::min)(budget, entries.size());
        for (auto entry = entries.begin(); entry != entries.end() && progress.scanned != limit;)
        {
            ++progress.scanned;
            if (entry->storage.CanDiscard())
            {
                entry = entries.erase(entry);
                ++progress.released;
                continue;
            }
            if (!entry->storage.CanFreeStorage())
            {
                // Pinned arrays need their actual hierarchy/association
                // retirement proof. Unknown entered effects cannot be retried.
                auto preserved = entry++;
                entries.splice(entries.end(), entries, preserved);
                continue;
            }
            ++progress.attempted;
            Receipt receipt;
            result = entry->storage.GetKind() == Storage::Kind::SequenceArray
                         ? NativeCustomAnimSequenceStorageCalls::FreeStorage(image, entry->storage, generation, owner, &receipt, invoke)
                         : NativeAnimationKeyFrameStorageCalls::FreeStorage(image, entry->storage, generation, owner, &receipt, invoke);
            if (entry->storage.CanDiscard())
            {
                entry = entries.erase(entry);
                ++progress.released;
            }
            else
            {
                auto preserved = entry++;
                entries.splice(entries.end(), entries, preserved);
            }
            if (result != ClockResult::Ok)
                break;
        }
        progress.retained = entries.size();
        Publish();
        return result;
    }
}
