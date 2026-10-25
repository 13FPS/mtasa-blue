/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain native sequence storage across failed publication and End
 ****************************************************************************/
#include "NativeCustomAnimSequenceController.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativeAnimationSequenceFactoryCalls.h"
#include "NativePrivateAnimationHierarchyController.h"
#include "NativeAnimationSequenceCleanupCalls.h"
#include "NativeAnimationSequenceOperationCalls.h"
#include <tuple>
#include <algorithm>
#include <iterator>
#include <limits>
#include <new>

namespace mta::game_de
{
    namespace CustomAnimSequenceControllerDetail
    {
        struct OptionalOperation
        {
            std::atomic_flag* value;
            ~OptionalOperation()
            {
                if (value)
                    value->clear(std::memory_order_release);
            }
        };
        struct Operation
        {
            std::atomic_flag& value;
            ~Operation() { value.clear(std::memory_order_release); }
        };
    }
    bool NativeCustomAnimSequenceController::FrameDescriptor::Matches(const NativeAnimationSequenceRowCalls::Value& physical) const noexcept
    {
        if (format == FrameFormat::Unknown || !record || !base || !count)
            return false;
        const auto flags = static_cast<std::uint16_t>(0x01u | (root ? 0x02u : 0u) | (format == FrameFormat::Compressed ? 0x04u : 0u));
        return physical.keyFrames == base && physical.frameCount == static_cast<std::uint16_t>(count) && (physical.flags & ~std::uint16_t{0x10}) == flags;
    }
    bool NativeCustomAnimSequenceController::CurrentFrameDescriptor(const FrameDescriptor& descriptor) const noexcept
    {
        if (descriptor.format == FrameFormat::Unknown || !descriptor.record || !descriptor.count)
            return false;
        const std::size_t stride = descriptor.format == FrameFormat::Compressed ? (descriptor.root ? 16u : 10u) : (descriptor.root ? 32u : 20u);
        for (const auto& entry : entries)
        {
            const auto& storage = entry.storage;
            if (&storage != descriptor.record)
                continue;
            // Compare a live CPU record before dereferencing its metadata;
            // known retirement invalidates every referring descriptor below.
            return entry.issued && storage.GetKind() == Storage::Kind::KeyFrames && storage.OwnerGeneration() == ownerGeneration &&
                   storage.GetState() == Storage::State::LiveUnpublished && storage.IssuedTag() == descriptor.base &&
                   descriptor.count <= storage.ByteSize() / stride;
        }
        return false;
    }
    void NativeCustomAnimSequenceController::ForgetFrameAuthority(const Storage& retired) noexcept
    {
        if (retired.GetKind() != Storage::Kind::KeyFrames)
            return;
        for (auto& entry : entries)
            for (auto& row : entry.rows)
                if (row.frames.record == &retired)
                    row.frames = {};
        // This forgets only format affinity after a known consumed record.
        // It neither frees aliases nor erases any other allocation journal.
    }
    void NativeCustomAnimSequenceController::Publish() noexcept
    {
        retained.store(!entries.empty() || !cleanups.empty(), std::memory_order_release);
        bool needsRecovery = stopping && !entries.empty();
        for (const auto& entry : entries)
        {
            needsRecovery = needsRecovery || (!entry.issued && entry.storage.HasOutstandingResource()) || entry.storage.GetState() == Storage::State::FaultDebt;
            if (entry.privateDependency)
            {
                const auto phase = entry.privateDependency->phase;
                needsRecovery = needsRecovery || phase == PrivatePhase::Preparing || phase == PrivatePhase::AttachEntered ||
                                phase == PrivatePhase::DetachEntered || phase == PrivatePhase::Uncertain;
            }
        }
        for (const auto& cleanup : cleanups)
            needsRecovery = needsRecovery || cleanup->refused ||
                            std::any_of(cleanup->frames.begin(), cleanup->frames.end(), [](const auto& ack) { return ack->stage == CleanupStage::Unknown; }) ||
                            (cleanup->arrayAck && cleanup->arrayAck->stage == CleanupStage::Unknown);
        recovery.store(needsRecovery, std::memory_order_release);
        if (entries.empty() && cleanups.empty())
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
    ClockResult NativeCustomAnimSequenceController::ResolveRowAt(std::uintptr_t image, std::uint64_t generation, std::uintptr_t originalArray,
                                                                 std::uint32_t index, CheckOwner owner, bool& present,
                                                                 NativeAnimationSequenceReferenceToken& output) noexcept
    {
        const auto requestedGeneration = generation;
        const auto requestedArray = originalArray;
        const auto requestedIndex = index;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!requestedGeneration || ownerGeneration != requestedGeneration || stopping)
            return ClockResult::OwnerConflict;
        if (!owner)
            return ClockResult::NotInitialized;
        const auto checked = owner();
        if (checked != ClockResult::Ok)
            return checked;
        const auto array = Find(requestedGeneration, requestedArray);
        if (array == entries.end() || array->cleanupOwner || array->operationOwner || array->purpose != SequencePurpose::ArrayRows ||
            array->storage.GetKind() != Storage::Kind::SequenceArray)
            return ClockResult::BindingChanged;
        std::size_t capacity = 0;
        const auto  result = NativeAnimationSequenceFactoryCalls::ReadCapacity(image, array->storage, requestedGeneration, owner, capacity);
        if (result != ClockResult::Ok)
            return result;
        const bool found = static_cast<std::size_t>(requestedIndex) < capacity;
        if (found)
            output = {requestedGeneration, requestedArray, static_cast<std::size_t>(requestedIndex)};
        present = found;
        return ClockResult::Ok;
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
                                                                     CheckOwner owner, std::uintptr_t& output, Receipt* receipt, InvokeMalloc invoke,
                                                                     SequencePurpose purpose) noexcept
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
        if (nextResourceIssue == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::OwnerConflict;
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
        auto entry = std::prev(entries.end());
        entry->issue = ++nextResourceIssue;
        entry->purpose = purpose;
        Receipt local;
        result =
            kind == Storage::Kind::SequenceArray
                ? NativeCustomAnimSequenceStorageCalls::Allocate(image, entry->storage, generation, amount, owner, &local, invoke)
                : NativeAnimationKeyFrameStorageCalls::Allocate(image, entry->storage, generation, static_cast<std::uint32_t>(amount), owner, &local, invoke);
        if (result == ClockResult::Ok && local.readyTag)
            entry->issued = true;
        if (entry->storage.CanDiscard())
        {
            ForgetFrameAuthority(entry->storage);
            entries.erase(entry);
        }
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
        if (kind == Storage::Kind::SequenceArray && entry->purpose != SequencePurpose::ArrayRows)
            return ClockResult::BindingChanged;
        if (entry->operationOwner || entry->cleanupOwner || (entry->cleanupAck && entry->cleanupAck->claims) || entry->privateDependency || entry->privateHolds)
            return ClockResult::OwnerConflict;
        if (kind == Storage::Kind::KeyFrames && entry->cleanupAck && CleanupResourceBlocked(*entry->cleanupAck))
            return ClockResult::OwnerConflict;
        Receipt    local;
        const auto result = kind == Storage::Kind::SequenceArray
                                ? NativeCustomAnimSequenceStorageCalls::FreeStorage(image, entry->storage, generation, owner, &local, invoke)
                                : NativeAnimationKeyFrameStorageCalls::FreeStorage(image, entry->storage, generation, owner, &local, invoke);
        RecordResourceReceipt(*entry, local);
        // A returned native free is already consumed even if its post-call
        // acknowledgement failed. Erasing that released record cannot replay
        // free or infer anything by probing a possibly reused native address.
        if (entry->storage.CanDiscard())
        {
            ForgetFrameAuthority(entry->storage);
            entries.erase(entry);
        }
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
            entry->storage.GetState() != Storage::State::LiveUnpublished || entry->cleanupOwner || entry->operationOwner ||
            token.index >= entry->storage.Capacity())
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
    ClockResult NativeCustomAnimSequenceController::ReadSequenceFrameFormat(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                            CheckOwner owner, AcceptedFrameFormat& output) noexcept
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
        auto      array = entries.end();
        RowState* row = nullptr;
        if ((result = SelectRow(request, array, row)) != ClockResult::Ok)
            return result;
        const auto descriptor = row->frames;
        if (descriptor.format == FrameFormat::Unknown)
            return ClockResult::NotInitialized;
        if (!CurrentFrameDescriptor(descriptor))
            return ClockResult::BindingChanged;
        NativeAnimationSequenceRowCalls::Value first, after;
        result = NativeAnimationSequenceRowCalls::Read(image, array->storage, request.ownerGeneration, request.index, owner, first);
        if (result != ClockResult::Ok)
            return result;
        if (!descriptor.Matches(first))
            return ClockResult::BindingChanged;
        result = NativeAnimationSequenceRowCalls::CheckFramePointer(image, *descriptor.record, request.ownerGeneration, descriptor.base, owner);
        if (result != ClockResult::Ok)
            return result;
        result = NativeAnimationSequenceRowCalls::Read(image, array->storage, request.ownerGeneration, request.index, owner, after);
        if (result != ClockResult::Ok)
            return result;
        if (!descriptor.Matches(after) || first.row != after.row || first.keyFrames != after.keyFrames || first.flags != after.flags ||
            first.frameCount != after.frameCount || first.boneTag != after.boneTag || !CurrentFrameDescriptor(descriptor))
            return ClockResult::BindingChanged;
        output = {descriptor.format, descriptor.base, descriptor.count, descriptor.root};
        return ClockResult::Ok;
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
        if (entry->privateDependency)
            return ClockResult::OwnerConflict;
        RowReceipt local;
        result = NativeAnimationSequenceRowCalls::Initialize(image, entry->storage, request.ownerGeneration, request.index, owner, &local);
        // A completed physical assignment remains accepted after lost
        // acknowledgement. Keep logical state and all allocation journals;
        // the runtime stops rather than replaying or rewinding the write.
        if (local.accepted)
        {
            row->logical.Initialize();
            row->frames = {};
        }
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
    ClockResult NativeCustomAnimSequenceController::CopySequenceProperties(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                           std::uintptr_t sourceRow, CheckOwner owner, CopyReceipt* receipt) noexcept
    {
        const auto request = token;
        const auto sourceAddress = sourceRow;
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
        auto      targetEntry = entries.end();
        RowState* targetState = nullptr;
        if ((result = SelectRow(request, targetEntry, targetState)) != ClockResult::Ok)
            return result;
        if (targetEntry->privateDependency)
            return ClockResult::OwnerConflict;
        NativeAnimationSequenceReferenceToken sourceToken;
        for (const auto& candidate : entries)
        {
            const auto& storage = candidate.storage;
            if (!candidate.issued || storage.OwnerGeneration() != request.ownerGeneration || storage.GetKind() != Storage::Kind::SequenceArray ||
                storage.GetState() != Storage::State::LiveUnpublished || sourceAddress < storage.IssuedTag())
                continue;
            const auto displacement = sourceAddress - storage.IssuedTag();
            if (displacement % NativeCustomAnimSequenceStorageProfile::RowBytes)
                continue;
            const auto index = displacement / NativeCustomAnimSequenceStorageProfile::RowBytes;
            if (index < storage.Capacity())
            {
                sourceToken = {request.ownerGeneration, storage.IssuedTag(), index};
                break;
            }
        }
        if (!sourceToken.firstTag)
            return ClockResult::BindingChanged;
        auto      sourceEntry = entries.end();
        RowState* sourceState = nullptr;
        if ((result = SelectRow(sourceToken, sourceEntry, sourceState)) != ClockResult::Ok)
            return result;
        // The logical union follows the same full property copy. Reserve both
        // CPU row nodes before entry, and retain overwritten/aliased frame
        // journals instead of interpreting shallow copy as native retirement.
        const auto logical = sourceState->logical;
        const auto frames = sourceState->frames;
        for (const auto& resource : entries)
            if (&resource.storage == frames.record && resource.cleanupAck && resource.cleanupAck->claims)
                return ClockResult::OwnerConflict;
        std::shared_ptr<CleanupResource> obligation;
        bool                             obligationAdded = false;
        if (frames.record)
        {
            const auto nativeFrame = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return &e.storage == frames.record; });
            if (nativeFrame == entries.end() || (result = ReserveFrameObligation(*targetEntry, *nativeFrame, obligation, obligationAdded)) != ClockResult::Ok)
                return nativeFrame == entries.end() ? ClockResult::BindingChanged : result;
        }
        CopyReceipt local;
        result = NativeAnimationSequenceCopyCalls::Copy(image, targetEntry->storage, request.index, sourceEntry->storage, sourceToken.index,
                                                        request.ownerGeneration, owner, &local);
        if (obligationAdded && !local.entered && !local.selfCopy)
            targetEntry->frameObligations.pop_back();
        if (local.rawCopied || (result == ClockResult::Ok && local.selfCopy))
        {
            targetState->logical = logical;
            // Transfer only accepted authority matching the actual raw-copy
            // intent. Raw flags, a parent bool or a different pre-read cannot
            // fabricate a format; Unknown copies as Unknown as well.
            targetState->frames = frames.Matches(local.sourceValue) && CurrentFrameDescriptor(frames) ? frames : FrameDescriptor{};
        }
        Publish();
        if (receipt)
            *receipt = local;
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::ReadSequenceFrame(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                      std::size_t frameIndex, std::uint32_t stride, CheckOwner owner,
                                                                      std::uintptr_t& output) noexcept
    {
        const auto request = token;
        const auto requestedIndex = frameIndex;
        const auto requestedStride = stride;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        auto      array = entries.end();
        RowState* state = nullptr;
        if ((result = SelectRow(request, array, state)) != ClockResult::Ok)
            return result;
        NativeAnimationSequenceRowCalls::Value physical;
        result = NativeAnimationSequenceRowCalls::Read(image, array->storage, request.ownerGeneration, request.index, owner, physical);
        if (result != ClockResult::Ok)
            return result;
        for (const auto& candidate : entries)
        {
            const auto& frames = candidate.storage;
            if (!candidate.issued || frames.OwnerGeneration() != request.ownerGeneration || frames.GetKind() != Storage::Kind::KeyFrames ||
                frames.GetState() != Storage::State::LiveUnpublished || physical.keyFrames < frames.IssuedTag() ||
                physical.keyFrames - frames.IssuedTag() >= frames.ByteSize())
                continue;
            // The native helper qualifies the array and exact frame record in
            // one shared epoch, preserving full arithmetic widths. No local
            // WORD count clamp or payload read can change MTA's byte request.
            std::uintptr_t next = 0;
            result = NativeAnimationSequenceFrameViewCalls::Read(image, array->storage, request.ownerGeneration, request.index, frames, requestedIndex,
                                                                 requestedStride, owner, next);
            if (result == ClockResult::Ok)
                output = next;
            return result;
        }
        return ClockResult::BindingChanged;
    }
    ClockResult NativeCustomAnimSequenceController::SetSequenceName(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                    const NativeAnimationSequenceName& name, CheckOwner owner, NameReceipt* receipt) noexcept
    {
        const auto request = token;
        const auto copiedName = name;
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
        RowState* state = nullptr;
        if ((result = SelectRow(request, entry, state)) != ClockResult::Ok)
            return result;
        std::uint32_t hash = 0;
        // Reuse the original source's byte normalization and existing raw
        // key generator. Physical name spelling stays independent of that
        // logical union, and no native name cache or matcher runs here.
        if (!NativeAnimationSequenceNamePolicy::Hash(copiedName, hash))
            return ClockResult::InvalidArgument;
        NameReceipt local;
        result = NativeAnimationSequenceNameCalls::Set(image, entry->storage, request.ownerGeneration, request.index, copiedName, hash, owner, &local);
        if (local.accepted)
            state->logical.SetNameHash(hash);
        Publish();
        if (receipt)
            *receipt = local;
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::PrepareFreshCompressedSequence(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                                   std::size_t count, bool root, std::uintptr_t originalFrameBase,
                                                                                   CheckOwner owner, FrameBindReceipt* receipt) noexcept
    {
        return PrepareFreshSequence(image, token, count, root, originalFrameBase, true, owner, receipt);
    }
    ClockResult NativeCustomAnimSequenceController::PrepareFreshUncompressedSequence(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                                     std::size_t count, bool root, std::uintptr_t originalFrameBase,
                                                                                     CheckOwner owner, FrameBindReceipt* receipt) noexcept
    {
        return PrepareFreshSequence(image, token, count, root, originalFrameBase, false, owner, receipt);
    }
    ClockResult NativeCustomAnimSequenceController::PrepareFreshSequence(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& token,
                                                                         std::size_t count, bool root, std::uintptr_t originalFrameBase, bool compressed,
                                                                         CheckOwner owner, FrameBindReceipt* receipt) noexcept
    {
        const auto request = token;
        const auto requestedCount = count;
        const auto requestedRoot = root;
        const auto requestedBase = originalFrameBase;
        const auto requestedCompressed = compressed;
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
        auto      array = entries.end();
        RowState* state = nullptr;
        if ((result = SelectRow(request, array, state)) != ClockResult::Ok)
            return result;
        if (array->privateDependency)
            return ClockResult::OwnerConflict;
        auto frames = Find(request.ownerGeneration, requestedBase);
        if (frames == entries.end() || frames->storage.GetKind() != Storage::Kind::KeyFrames || frames->storage.GetState() != Storage::State::LiveUnpublished)
            return ClockResult::BindingChanged;
        if (frames->cleanupAck && frames->cleanupAck->claims)
            return ClockResult::OwnerConflict;
        // This restricted builder links existing private records only. It
        // does not implement the general setter, publish to GTA, infer shared
        // bit08 from a pointer or transfer either allocation's free authority.
        std::shared_ptr<CleanupResource> obligation;
        bool                             obligationAdded = false;
        if ((result = ReserveFrameObligation(*array, *frames, obligation, obligationAdded)) != ClockResult::Ok)
            return result;
        FrameBindReceipt local;
        result = requestedCompressed ? NativeAnimationSequenceFrameBindCalls::AttachCompressed(image, array->storage, request.ownerGeneration, request.index,
                                                                                               frames->storage, requestedCount, requestedRoot, owner, &local)
                                     : NativeAnimationSequenceFrameBindCalls::AttachUncompressed(image, array->storage, request.ownerGeneration, request.index,
                                                                                                 frames->storage, requestedCount, requestedRoot, owner, &local);
        if (obligationAdded && !local.entered)
            array->frameObligations.pop_back();
        if (local.accepted)
            state->frames = {requestedCompressed ? FrameFormat::Compressed : FrameFormat::Uncompressed, &frames->storage, requestedBase, requestedCount,
                             requestedRoot};
        Publish();
        if (receipt)
            *receipt = local;
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::ResolveUncompressedFrame(std::size_t index, const NativeAnimationSequenceRowCalls::Value& physical,
                                                                             const Storage*& output, void* opaque) noexcept
    {
        auto& context = *static_cast<PayloadVisitContext*>(opaque);
        for (const auto& row : context.array->rows)
        {
            if (row.index != index)
                continue;
            if (row.frames.format != FrameFormat::Uncompressed)
                return ClockResult::NotInitialized;
            if (!row.frames.Matches(physical) || !context.self->CurrentFrameDescriptor(row.frames))
                return ClockResult::BindingChanged;
            output = row.frames.record;
            return ClockResult::Ok;
        }
        // No row node/descriptor is allocated during a borrowed native gate.
        // A raw uncompressed-looking flag is never a producer of authority.
        return ClockResult::NotInitialized;
    }
    ClockResult NativeCustomAnimSequenceController::QualifyUncompressedRowsHeld(const NativeAnimationSequenceDependencyCalls::Epoch& epoch,
                                                                                const PrivateDependencyKey& key, std::uint16_t count,
                                                                                NativeAnimationSequencePayloadCalls::Operation operationValue,
                                                                                CheckOwner                                     owner) noexcept
    {
        if (epoch.Generation() != key.ownerGeneration || ownerGeneration != key.ownerGeneration || epoch.ArrayTag() != key.arrayTag)
            return ClockResult::OwnerConflict;
        auto array = Find(key.ownerGeneration, key.arrayTag);
        if (array == entries.end() || !array->privateDependency || !SamePrivateKey(array->privateDependency->key, key) ||
            array->privateDependency->phase != PrivatePhase::AttachedAccepted)
            return ClockResult::OwnerConflict;
        PayloadVisitContext context{this, &*array};
        return NativeAnimationSequencePayloadCalls::QualifyHeld(epoch, array->storage, count, operationValue, owner, &ResolveUncompressedFrame, &context);
    }
    bool NativeCustomAnimSequenceController::SamePrivateKey(const PrivateDependencyKey& a, const PrivateDependencyKey& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.hierarchy == b.hierarchy && a.arrayTag == b.arrayTag && a.command == b.command;
    }
    ClockResult NativeCustomAnimSequenceController::ResolvePrivateFrame(std::size_t index, const NativeAnimationSequenceRowCalls::Value& row,
                                                                        const Storage*& output, void* opaque) noexcept
    {
        auto& context = *static_cast<PrivateVisitContext*>(opaque);
        auto& link = *context.array->privateDependency;
        if (index >= link.rows.size())
            return ClockResult::BindingChanged;
        auto& previous = link.rows[index];
        if (previous.row)
        {
            if (previous.row != row.row || previous.keyFrames != row.keyFrames || previous.frameCount != row.frameCount ||
                (previous.flags & 0x0fu) != (row.flags & 0x0fu))
            {
                link.phase = PrivatePhase::Uncertain;
                return ClockResult::BindingChanged;
            }
        }
        else
            previous = row;
        output = nullptr;
        if (!row.keyFrames)
            return ClockResult::Ok;
        auto frame = context.self->Find(link.key.ownerGeneration, row.keyFrames);
        if (frame == context.self->entries.end() || frame->storage.GetKind() != Storage::Kind::KeyFrames ||
            frame->storage.GetState() != Storage::State::LiveUnpublished)
        {
            if (link.phase != PrivatePhase::Preparing)
                link.phase = PrivatePhase::Uncertain;
            return ClockResult::BindingChanged;
        }
        auto* record = &*frame;
        if (std::find(link.frames.begin(), link.frames.end(), record) == link.frames.end())
        {
            if (frame->privateHolds == (std::numeric_limits<std::size_t>::max)())
                return ClockResult::OwnerConflict;
            // Capacity was reserved before callbacks, so this pointer push
            // cannot allocate after native qualification has begun.
            link.frames.push_back(record);
            ++frame->privateHolds;
        }
        output = &frame->storage;
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::ReservePrivateAction(Storage&, const NativeAnimationSequenceDependencyCalls::Epoch&, void* opaque) noexcept
    {
        auto& context = *static_cast<PrivateVisitContext*>(opaque);
        if (context.array->privateDependency->phase == PrivatePhase::Preparing)
            context.array->privateDependency->phase = PrivatePhase::Reserved;
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::ReservePrivatePreparedArray(std::uintptr_t image, const PrivateDependencyKey& key, std::size_t capacity,
                                                                                CheckOwner owner, PrivateDependencyReceipt* receipt) noexcept
    {
        return ReservePrivatePreparedArrayImpl(image, key, capacity, owner, receipt, false);
    }
    ClockResult NativeCustomAnimSequenceController::ReserveIssuedPrivatePreparedArray(std::uintptr_t image, const PrivateDependencyKey& key, CheckOwner owner,
                                                                                      PrivateDependencyReceipt* receipt) noexcept
    {
        return ReservePrivatePreparedArrayImpl(image, key, 0, owner, receipt, true);
    }
    ClockResult NativeCustomAnimSequenceController::ReservePrivatePreparedArrayImpl(std::uintptr_t image, const PrivateDependencyKey& value,
                                                                                    std::size_t expectedCapacity, CheckOwner owner,
                                                                                    PrivateDependencyReceipt* receipt, bool actualCapacity) noexcept
    {
        const auto key = value;
        auto       capacity = expectedCapacity;
        if (receipt)
            *receipt = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!key.ownerGeneration || !key.hierarchy || !key.arrayTag || !key.command || ownerGeneration != key.ownerGeneration || stopping)
            return ClockResult::OwnerConflict;
        if (!owner)
            return ClockResult::NotInitialized;
        auto array = Find(key.ownerGeneration, key.arrayTag);
        if (array == entries.end() || array->storage.GetKind() != Storage::Kind::SequenceArray || array->storage.GetState() != Storage::State::LiveUnpublished)
            return ClockResult::BindingChanged;
        if (array->cleanupOwner || array->operationOwner || array->purpose == SequencePurpose::StandaloneSequence)
            return ClockResult::OwnerConflict;
        if (actualCapacity)
            capacity = array->storage.Capacity();
        if (!capacity || capacity != array->storage.Capacity())
            return ClockResult::InvalidArgument;
        if (array->privateDependency)
        {
            if (!SamePrivateKey(array->privateDependency->key, key) ||
                (array->privateDependency->phase != PrivatePhase::Preparing && array->privateDependency->phase != PrivatePhase::Reserved))
                return ClockResult::OwnerConflict;
        }
        else
        {
            try
            {
                auto planned = std::make_unique<PrivateDependency>();
                planned->key = key;
                planned->rows.resize(capacity);
                planned->frames.reserve(capacity);
                array->privateDependency = std::move(planned);
            }
            catch (...)
            {
                return ClockResult::WriteFailure;
            }
        }
        PrivateVisitContext context{this, &*array};
        const auto result = NativeAnimationSequenceDependencyCalls::WithPrepared(image, array->storage, key.ownerGeneration, owner, &ResolvePrivateFrame,
                                                                                 &context, &ReservePrivateAction, &context);
        // Even failed preparation stays with the planned parent command until
        // its explicit no-entry cancellation. Absence/raw null is no ACK.
        if (receipt)
        {
            receipt->capacity = capacity;
            receipt->frameRecords = array->privateDependency->frames.size();
            receipt->reserved = true;
            receipt->verified = result == ClockResult::Ok;
            receipt->uncertain = array->privateDependency->phase == PrivatePhase::Uncertain;
        }
        Publish();
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::WithPrivatePreparedArray(std::uintptr_t image, const PrivateDependencyKey& key, CheckOwner owner,
                                                                             PrivatePreparedAction action, void* context,
                                                                             PrivateDependencyReceipt* receipt) noexcept
    {
        return WithPrivateArray(image, key, owner, action, context, receipt, false);
    }
    ClockResult NativeCustomAnimSequenceController::CancelPrivatePreparedReservation(std::uintptr_t image, const PrivateDependencyKey& key, CheckOwner owner,
                                                                                     PrivatePreparedAction action, void* context,
                                                                                     PrivateDependencyReceipt* receipt) noexcept
    {
        return WithPrivateArray(image, key, owner, action, context, receipt, true);
    }
    ClockResult NativeCustomAnimSequenceController::WithPrivateArray(std::uintptr_t image, const PrivateDependencyKey& value, CheckOwner owner,
                                                                     PrivatePreparedAction action, void* actionContext, PrivateDependencyReceipt* receipt,
                                                                     bool cancel) noexcept
    {
        const auto key = value;
        if (receipt)
            *receipt = {};
        if (!owner)
            return ClockResult::NotInitialized;
        if (!action)
            return ClockResult::InvalidArgument;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!key.ownerGeneration || ownerGeneration != key.ownerGeneration)
            return ClockResult::OwnerConflict;
        auto array = Find(key.ownerGeneration, key.arrayTag);
        if (array == entries.end() || !array->privateDependency || !SamePrivateKey(array->privateDependency->key, key))
            return ClockResult::BindingChanged;
        auto& link = *array->privateDependency;
        if (link.phase == PrivatePhase::Uncertain)
            return ClockResult::OwnerConflict;
        if (!cancel && link.phase == PrivatePhase::Preparing)
            return ClockResult::OwnerConflict;
        if (cancel && link.phase != PrivatePhase::Preparing && link.phase != PrivatePhase::Reserved)
            return ClockResult::OwnerConflict;
        PrivateVisitContext visit{this, &*array};
        const auto          capacity = array->storage.Capacity();
        const auto          frames = link.frames.size();
        const auto result = cancel ? NativeAnimationSequenceDependencyCalls::WithArray(image, array->storage, key.ownerGeneration, owner, action, actionContext)
                            : array->cleanupOwner ? NativeAnimationSequenceCleanupCalls::WithArray(image, array->storage, key.ownerGeneration, owner,
                                                                                                   &ResolvePrivateFrame, &visit, action, actionContext)
                                                  : NativeAnimationSequenceDependencyCalls::WithPrepared(image, array->storage, key.ownerGeneration, owner,
                                                                                                         &ResolvePrivateFrame, &visit, action, actionContext);
        bool       released = false;
        ClockResult final = result;
        if (result == ClockResult::Ok && ((cancel && link.cancelRequested) || (!cancel && link.releaseRequested)))
        {
            final = ReleasePrivateDependencyHeld(*array);
            released = final == ClockResult::Ok;
            if (!released)
                link.phase = PrivatePhase::Uncertain;
        }
        if (!released && array->storage.GetState() != Storage::State::LiveUnpublished)
            link.phase = PrivatePhase::Uncertain;
        if (receipt)
        {
            receipt->capacity = capacity;
            receipt->frameRecords = frames;
            receipt->reserved = !released;
            receipt->released = released;
            receipt->verified = final == ClockResult::Ok;
            receipt->uncertain = !released && array->privateDependency->phase == PrivatePhase::Uncertain;
        }
        Publish();
        return final;
    }
    ClockResult NativeCustomAnimSequenceController::ReleasePrivateDependencyHeld(Entry& array) noexcept
    {
        auto&      link = *array.privateDependency;
        const bool cancelled = link.cancelRequested && (link.phase == PrivatePhase::Preparing || link.phase == PrivatePhase::Reserved);
        const bool detached = link.releaseRequested && link.phase == PrivatePhase::DetachAccepted;
        if (!cancelled && !detached)
            return ClockResult::OwnerConflict;
        if (array.storage.GetState() != Storage::State::LiveUnpublished)
            return ClockResult::OwnerConflict;
        for (const auto* frame : link.frames)
            if (!frame->privateHolds || frame->storage.GetState() != Storage::State::LiveUnpublished)
                return ClockResult::OwnerConflict;
        for (auto* frame : link.frames)
            --frame->privateHolds;
        array.privateDependency.reset();
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::AdvancePrivateDependencyHeld(const PrivateDependencyKey& key, PrivatePhase expected,
                                                                                 PrivatePhase next) noexcept
    {
        auto array = Find(key.ownerGeneration, key.arrayTag);
        if (array == entries.end() || !array->privateDependency || !SamePrivateKey(array->privateDependency->key, key) ||
            array->privateDependency->phase != expected)
            return ClockResult::OwnerConflict;
        const bool allowed = (expected == PrivatePhase::Reserved && next == PrivatePhase::AttachEntered) ||
                             (expected == PrivatePhase::AttachEntered && next == PrivatePhase::AttachedAccepted) ||
                             (expected == PrivatePhase::AttachedAccepted && next == PrivatePhase::DetachEntered) ||
                             (expected == PrivatePhase::DetachEntered && next == PrivatePhase::DetachAccepted);
        if (!allowed)
            return ClockResult::InvalidArgument;
        array->privateDependency->phase = next;
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::RequestPrivateDetachReleaseHeld(const PrivateDependencyKey& key) noexcept
    {
        auto array = Find(key.ownerGeneration, key.arrayTag);
        if (array == entries.end() || !array->privateDependency || !SamePrivateKey(array->privateDependency->key, key) ||
            array->privateDependency->phase != PrivatePhase::DetachAccepted)
            return ClockResult::OwnerConflict;
        array->privateDependency->releaseRequested = true;
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::RequestPrivateReservationCancelHeld(const PrivateDependencyKey& key) noexcept
    {
        auto array = Find(key.ownerGeneration, key.arrayTag);
        if (array == entries.end() || !array->privateDependency || !SamePrivateKey(array->privateDependency->key, key))
            return ClockResult::BindingChanged;
        auto& link = *array->privateDependency;
        if (link.phase != PrivatePhase::Preparing && link.phase != PrivatePhase::Reserved)
            return ClockResult::OwnerConflict;
        link.cancelRequested = true;
        return ClockResult::Ok;
    }
    void NativeCustomAnimSequenceController::RetainPrivateDependencyHeld(const PrivateDependencyKey& key) noexcept
    {
        auto array = Find(key.ownerGeneration, key.arrayTag);
        if (array != entries.end() && array->privateDependency && SamePrivateKey(array->privateDependency->key, key))
            array->privateDependency->phase = PrivatePhase::Uncertain;
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
        if (entry == entries.end() || entry->operationOwner || entry->cleanupOwner || (entry->cleanupAck && entry->cleanupAck->claims))
            return ClockResult::BindingChanged;
        const auto result = NativeCustomAnimSequenceStorageCalls::PinPublication(entry->storage, generation, hierarchyAffinity);
        if (result == ClockResult::Ok && entry->privateDependency)
            entry->privateDependency->phase = PrivatePhase::Uncertain;
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
            if (entry->operationOwner || entry->cleanupOwner || (entry->cleanupAck && entry->cleanupAck->claims) || entry->privateDependency ||
                entry->privateHolds)
            {
                auto preserved = entry++;
                entries.splice(entries.end(), entries, preserved);
                continue;
            }
            if (entry->storage.CanDiscard())
            {
                ForgetFrameAuthority(entry->storage);
                entry = entries.erase(entry);
                ++progress.released;
                continue;
            }
            if (entry->storage.GetKind() == Storage::Kind::KeyFrames && entry->cleanupAck && CleanupResourceBlocked(*entry->cleanupAck))
            {
                auto preserved = entry++;
                entries.splice(entries.end(), entries, preserved);
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
            RecordResourceReceipt(*entry, receipt);
            if (entry->storage.CanDiscard())
            {
                ForgetFrameAuthority(entry->storage);
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
    bool NativeCustomAnimSequenceController::HasPendingCleanup() const noexcept
    {
        if (operation.test_and_set(std::memory_order_acquire))
            return true;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        // A completed view still contains backend vtable code. Entries and
        // unresolved commands also keep the actual module owner fenced.
        return !entries.empty() || !cleanups.empty();
    }
    void NativeCustomAnimSequenceController::PruneCleanupHeld() noexcept
    {
        for (auto item = cleanups.begin(); item != cleanups.end();)
        {
            const auto& record = **item;
            if (record.complete && !record.resuming && record.lifetime->views.load(std::memory_order_acquire) == 0 &&
                !record.lifetime->offCreatorDrop.load(std::memory_order_acquire))
                item = cleanups.erase(item);
            else
                ++item;
        }
        // This runs only after a qualified callback check. A CPU view's last
        // destructor merely sets a dirty hint; it never prunes native owners.
    }
    ClockResult NativeCustomAnimSequenceController::BeginCleanup(std::uintptr_t image, const NativeCustomAnimationCleanupRequest& value,
                                                                 NativePrivateAnimationHierarchyController& hierarchyOwner, CheckOwner owner,
                                                                 NativeCustomAnimationCleanupBeginReceipt& output) noexcept
    {
        return BeginCleanupImpl(image, value, hierarchyOwner, owner, output, SequencePurpose::ArrayRows, CommandKind::AnimationCleanup);
    }
    ClockResult NativeCustomAnimSequenceController::BeginCleanupImpl(std::uintptr_t, const NativeCustomAnimationCleanupRequest& value,
                                                                     NativePrivateAnimationHierarchyController& hierarchyOwner, CheckOwner,
                                                                     NativeCustomAnimationCleanupBeginReceipt& output, SequencePurpose purpose,
                                                                     CommandKind kind, bool alreadyHeld) noexcept
    {
        const auto request = value;
        output = {};
        if (!request.generation ||
            (request.hierarchy.locator ? request.hierarchy.ownerGeneration != request.generation : request.hierarchy.ownerGeneration != 0) ||
            (!request.originalArray && request.capacity) || (request.originalArray && !request.capacity))
            return ClockResult::InvalidArgument;
        if (!alreadyHeld && operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::OptionalOperation held{alreadyHeld ? nullptr : &operation};
        if ((ownerGeneration && ownerGeneration != request.generation) || hierarchyOwner.sequences != this)
            return ClockResult::OwnerConflict;
        std::uint64_t hierarchyIssue = 0;
        if (request.hierarchy.locator)
        {
            const auto qualified = hierarchyOwner.ReadCleanupIssue(request.hierarchy, hierarchyIssue);
            if (qualified != ClockResult::Ok)
                return qualified;
        }
        auto       array = request.originalArray ? Find(request.generation, request.originalArray) : entries.end();
        const auto arrayIssue = array != entries.end() ? array->issue : std::uint64_t{0};
        for (const auto& previous : cleanups)
        {
            const auto& p = previous->request;
            if (p.generation != request.generation || p.hierarchy.locator != request.hierarchy.locator || previous->hierarchyIssue != hierarchyIssue ||
                previous->arrayIssue != arrayIssue || (!request.hierarchy.locator && p.originalArray != request.originalArray))
                continue;
            if (previous->commandKind == CommandKind::FrameAttachment)
                continue;
            if (previous->complete || p.originalArray != request.originalArray || p.capacity != request.capacity || previous->hierarchyOwner != &hierarchyOwner)
                return ClockResult::OwnerConflict;
            const auto views = previous->lifetime->views.load(std::memory_order_acquire);
            if (views == (std::numeric_limits<std::size_t>::max)())
                return ClockResult::OwnerConflict;
            if (kind == CommandKind::SequenceCleanup && previous->commandKind == CommandKind::SequenceTransfer)
                previous->cancelRequested = true;
            previous->lifetime->views.fetch_add(1, std::memory_order_acq_rel);
            output.view = NativeCustomAnimationCleanupView(previous->lifetime);
            output.accepted = true;
            return ClockResult::Ok;
        }
        if (nextCleanupCommand == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::OwnerConflict;

        if (request.originalArray && (array == entries.end() || array->storage.GetKind() != Storage::Kind::SequenceArray ||
                                      (purpose == SequencePurpose::StandaloneSequence ? !array->storage.CanFreeStorage()
                                                                                      : array->storage.GetState() != Storage::State::LiveUnpublished) ||
                                      array->purpose != purpose || array->storage.Capacity() != request.capacity || array->cleanupOwner))
            return ClockResult::BindingChanged;
        std::shared_ptr<CleanupRecord> record;
        try
        {
            record = std::make_shared<CleanupRecord>();
            record->request = request;
            record->commandKind = kind;
            record->hierarchyIssue = hierarchyIssue;
            record->arrayIssue = arrayIssue;
            record->token = {request.generation, nextCleanupCommand + 1};
            record->hierarchyOwner = &hierarchyOwner;
            record->lifetime = std::shared_ptr<NativeCustomAnimationCleanupLifetime>(new NativeCustomAnimationCleanupLifetime(record->token));
            if (array != entries.end())
            {
                record->array = &*array;
                record->rows.resize(request.capacity);
                record->frames.reserve(request.capacity);
                record->arrayAck = std::make_shared<CleanupResource>();
                record->arrayAck->live = &*array;
                record->arrayAck->kind = Storage::Kind::SequenceArray;
                record->arrayAck->generation = request.generation;
                record->arrayAck->tag = request.originalArray;
                const auto addFrame = [&](Entry* frame)
                {
                    if (!frame || !frame->issued || frame->storage.OwnerGeneration() != request.generation ||
                        frame->storage.GetKind() != Storage::Kind::KeyFrames || frame->storage.GetState() != Storage::State::LiveUnpublished)
                        return false;
                    for (const auto& ack : record->frames)
                        if (ack->live == frame)
                            return true;
                    if (frame->cleanupAck && frame->cleanupAck->claims == (std::numeric_limits<std::size_t>::max)())
                        return false;
                    auto ack = frame->cleanupAck;
                    if (!ack)
                    {
                        ack = std::make_shared<CleanupResource>();
                        ack->live = frame;
                        ack->kind = Storage::Kind::KeyFrames;
                        ack->generation = request.generation;
                        ack->tag = frame->storage.IssuedTag();
                    }
                    if (ack->stage != CleanupStage::Prepared)
                        return false;
                    record->frames.push_back(std::move(ack));
                    return true;
                };
                for (const auto& obligation : array->frameObligations)
                    if (obligation->stage != CleanupStage::Returned && (!obligation->live || !addFrame(obligation->live)))
                        return ClockResult::BindingChanged;
                for (const auto& row : array->rows)
                {
                    if (!row.frames.record)
                        continue;
                    auto frame = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return &e.storage == row.frames.record; });
                    if (frame == entries.end() || !addFrame(&*frame))
                        return ClockResult::BindingChanged;
                }
                if (array->privateDependency)
                {
                    if (array->privateDependency->key.hierarchy != request.hierarchy.locator || array->privateDependency->phase == PrivatePhase::Uncertain)
                        return ClockResult::OwnerConflict;
                    for (auto* frame : array->privateDependency->frames)
                        if (!addFrame(frame))
                            return ClockResult::BindingChanged;
                }
            }
            // Allocate the durable list node before installing any claim.
            cleanups.push_back(record);
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        const auto claimed =
            request.hierarchy.locator ? hierarchyOwner.ClaimCleanupIntent(request.hierarchy, request.originalArray, record->token.command) : ClockResult::Ok;
        if (claimed != ClockResult::Ok)
        {
            cleanups.pop_back();
            return claimed;
        }
        ++nextCleanupCommand;
        if (array != entries.end())
        {
            if (array->operationOwner)
                for (const auto& pending : cleanups)
                    if (pending->token.command == array->operationOwner)
                        pending->cancelRequested = true;
            array->cleanupOwner = record->token.command;
            array->cleanupAck = record->arrayAck;
            ++record->arrayAck->claims;
        }
        for (const auto& ack : record->frames)
        {
            ack->live->cleanupAck = ack;
            ++ack->claims;
        }
        ownerGeneration = request.generation;
        output.view = NativeCustomAnimationCleanupView(record->lifetime);
        output.accepted = true;
        Publish();
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::ResolveCleanupFrame(std::size_t index, const NativeAnimationSequenceRowCalls::Value& row,
                                                                        const Storage*& output, void* opaque) noexcept
    {
        const auto& context = *static_cast<std::tuple<NativeCustomAnimSequenceController*, CleanupRecord*, CheckOwner>*>(opaque);
        auto&       self = *std::get<0>(context);
        auto&       record = *std::get<1>(context);
        output = nullptr;
        if (!record.array || index >= record.rows.size())
            return ClockResult::BindingChanged;
        auto& previous = record.rows[index];
        if (previous.row && (previous.row != row.row || previous.keyFrames != row.keyFrames || previous.frameCount != row.frameCount ||
                             (previous.flags & 0x0fu) != (row.flags & 0x0fu)))
            return ClockResult::BindingChanged;
        previous = row;
        if (!row.keyFrames)
            return row.frameCount ? ClockResult::BindingChanged : ClockResult::Ok;
        for (const auto& ack : record.frames)
        {
            auto* frame = ack->live;
            if (!frame || ack->stage != CleanupStage::Prepared || ack->tag != row.keyFrames)
                continue;
            bool affinity = false;
            for (const auto& known : record.array->rows)
                if (known.index == index && known.frames.record == &frame->storage && known.frames.Matches(row) && self.CurrentFrameDescriptor(known.frames))
                    affinity = true;
            const auto& link = record.array->privateDependency;
            if (link && index < link->rows.size())
            {
                const auto& known = link->rows[index];
                affinity = affinity ||
                           (known.row == row.row && known.keyFrames == row.keyFrames && known.frameCount == row.frameCount &&
                            (known.flags & 0x0fu) == (row.flags & 0x0fu) && std::find(link->frames.begin(), link->frames.end(), frame) != link->frames.end());
            }
            if (!affinity)
                return ClockResult::BindingChanged;
            output = &frame->storage;
            return ClockResult::Ok;
        }
        // No raw address coincidence can adopt an uncommitted caller buffer.
        return ClockResult::BindingChanged;
    }
    ClockResult NativeCustomAnimSequenceController::VerifyCleanupAction(Storage&, const NativeAnimationSequenceDependencyCalls::Epoch& epoch,
                                                                        void* opaque) noexcept
    {
        auto& context = *static_cast<std::tuple<NativeCustomAnimSequenceController*, CleanupRecord*, CheckOwner>*>(opaque);
        auto& record = *std::get<1>(context);
        if (!record.request.hierarchy.locator)
            return record.array->privateDependency ? ClockResult::OwnerConflict : ClockResult::Ok;
        return record.hierarchyOwner->VerifyCleanupHeld(epoch, record.request.hierarchy, record.token.command, std::get<2>(context));
    }
    bool NativeCustomAnimSequenceController::CleanupResourceBlocked(const CleanupResource& ack) const noexcept
    {
        if (!ack.live || ack.live->privateHolds || ack.live->privateDependency)
            return true;
        if (ack.kind != Storage::Kind::KeyFrames)
            return false;
        for (const auto& record : cleanups)
            if (!record->complete && std::find(record->frames.begin(), record->frames.end(), ack.live->cleanupAck) != record->frames.end() &&
                (!record->snapshotVerified || !record->dependencyReleased))
                return true;
        for (const auto& entry : entries)
        {
            if (entry.storage.GetKind() != Storage::Kind::SequenceArray || entry.cleanupOwner)
                continue;
            if (std::find(entry.frameObligations.begin(), entry.frameObligations.end(), ack.live->cleanupAck) != entry.frameObligations.end())
                return true;
            for (const auto& row : entry.rows)
                if (row.frames.record == &ack.live->storage)
                    return true;
        }
        return false;
    }
    ClockResult NativeCustomAnimSequenceController::ResumeCleanup(std::uintptr_t image, const NativeCustomAnimationCleanupToken& value,
                                                                  NativePrivateAnimationHierarchyController& hierarchyOwner, std::size_t budget,
                                                                  CheckOwner owner, SCustomAnimationCleanupResult& output) noexcept
    {
        const auto token = value;
        output = {};
        if (!token.generation || !token.command)
            return ClockResult::InvalidArgument;
        std::shared_ptr<CleanupRecord> record;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            const CustomAnimSequenceControllerDetail::Operation held{operation};
            if (ownerGeneration != token.generation)
                return ClockResult::OwnerConflict;
            for (const auto& candidate : cleanups)
                if (candidate->token.generation == token.generation && candidate->token.command == token.command)
                    record = candidate;
            if (!record || record->hierarchyOwner != &hierarchyOwner)
                return ClockResult::BindingChanged;
            if (record->complete)
            {
                output.status = ECustomAnimationCleanupStatus::Complete;
                output.disposition = ECustomAnimationCleanupDisposition::PrivateDetached;
                return ClockResult::Ok;
            }
            if (record->refused)
            {
                output.status = ECustomAnimationCleanupStatus::Refused;
                return ClockResult::OwnerConflict;
            }
            // A saved completed/refused command is CPU acknowledgement, not
            // a new native observation. Its live view keeps this code pinned
            // while a destructor consumes completion outside the callback.
            if (!owner)
                return ClockResult::NotInitialized;
            const auto checked = owner();
            if (checked != ClockResult::Ok)
                return checked;
            if (record->resuming.exchange(true, std::memory_order_acq_rel))
                return ClockResult::Busy;
        }
        struct ResumeScope
        {
            std::atomic<bool>& active;
            ~ResumeScope() { active.store(false, std::memory_order_release); }
        } resume{record->resuming};
        const auto failed = [&](ClockResult status) noexcept
        {
            if (record->refused)
                output.status = ECustomAnimationCleanupStatus::Refused;
            return status;
        };
        if (record->commandKind == CommandKind::FrameAttachment || (record->commandKind == CommandKind::SequenceTransfer && !record->writerAcknowledged))
            return ClockResult::Busy;
        if (!record->snapshotVerified)
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            const CustomAnimSequenceControllerDetail::Operation held{operation};
            if (record->array && record->array->operationOwner && record->array->operationOwner != token.command)
                return ClockResult::Ok;
            if (record->array && record->array->purpose == SequencePurpose::StandaloneSequence)
            {
                if (record->array->privateDependency || record->request.hierarchy.locator)
                    return ClockResult::OwnerConflict;
                const auto checked = NativeAnimationSequenceOperationCalls::QualifyStandaloneRetirement(image, record->array->storage, token.generation, owner);
                if (checked != ClockResult::Ok)
                    return checked;
                record->snapshotVerified = record->dependencyReleased = true;
            }
            else if (record->array)
            {
                std::tuple<NativeCustomAnimSequenceController*, CleanupRecord*, CheckOwner> context{this, record.get(), owner};
                const auto checked = NativeAnimationSequenceCleanupCalls::WithArray(image, record->array->storage, token.generation, owner,
                                                                                    &ResolveCleanupFrame, &context, &VerifyCleanupAction, &context);
                if (checked != ClockResult::Ok)
                {
                    if (checked == ClockResult::BindingChanged || checked == ClockResult::InvalidArgument || checked == ClockResult::UnsupportedBuild)
                        record->refused = true;
                    Publish();
                    return failed(checked);
                }
                record->snapshotVerified = true;
            }
        }
        if (!record->dependencyReleased)
        {
            if (record->request.hierarchy.locator)
            {
                NativePrivateAnimationHierarchyCalls::Receipt receipt;
                bool                                          released = false;
                const auto status = hierarchyOwner.DetachCleanup(image, record->request.hierarchy, token.command, owner, receipt, released);
                // Physical command acceptance and the final child release are
                // different receipts. Preserve both before any late refusal;
                // the existing parent command resumes ACK without rewriting.
                record->detach.entered = record->detach.entered || receipt.entered;
                record->detach.accepted = record->detach.accepted || receipt.accepted;
                record->detach.acknowledged = record->detach.acknowledged || receipt.acknowledged;
                record->detach.hierarchy = receipt.hierarchy ? receipt.hierarchy : record->detach.hierarchy;
                record->detach.exceptionCode = receipt.exceptionCode ? receipt.exceptionCode : record->detach.exceptionCode;
                if (released)
                    record->dependencyReleased = true;
                if (receipt.retired)
                    record->outer = receipt;
                if (status != ClockResult::Ok)
                {
                    if ((receipt.entered && !receipt.accepted) || status == ClockResult::BindingChanged)
                        record->refused = true;
                    if (!operation.test_and_set(std::memory_order_acquire))
                    {
                        const CustomAnimSequenceControllerDetail::Operation held{operation};
                        Publish();
                    }
                    return failed(status);
                }
                if (!released)
                    return ClockResult::Ok;
            }
            else
            {
                // Explicit no-H input is admitted only for the original CPU
                // producer/current unlinked record; snapshot cannot contain an
                // active parent command. This is not missing-map detach proof.
                if (record->array && record->array->privateDependency)
                    return ClockResult::OwnerConflict;
                record->dependencyReleased = true;
            }
            record->snapshotVerified = true;
        }
        if (record->request.hierarchy.locator && !record->outer.retired)
        {
            NativePrivateAnimationHierarchyCalls::Receipt current;
            bool                                          acknowledged = false;
            const auto checked = hierarchyOwner.DetachCleanup(image, record->request.hierarchy, token.command, owner, current, acknowledged);
            if (current.retired)
                record->outer = current;
            if (checked != ClockResult::Ok || !acknowledged)
            {
                if (checked == ClockResult::BindingChanged)
                    record->refused = true;
                return failed(checked == ClockResult::Ok ? ClockResult::OwnerConflict : checked);
            }
        }
        ClockResult status = ClockResult::Ok;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            const CustomAnimSequenceControllerDetail::Operation held{operation};
            const auto                                          consume = [&](const std::shared_ptr<CleanupResource>& ack) noexcept
            {
                if (!ack || ack->stage == CleanupStage::Returned)
                    return ClockResult::Ok;
                if (ack->stage == CleanupStage::Unknown || ack->stage == CleanupStage::Entered)
                {
                    record->refused = true;
                    return ClockResult::OwnerConflict;
                }
                if (!budget || CleanupResourceBlocked(*ack))
                    return ClockResult::Ok;
                --budget;
                auto* entry = ack->live;
                if (!entry || entry->storage.OwnerGeneration() != token.generation || entry->storage.IssuedTag() != ack->tag ||
                    entry->storage.GetKind() != ack->kind || !entry->storage.CanFreeStorage())
                {
                    record->refused = true;
                    return ClockResult::BindingChanged;
                }
                ack->stage = CleanupStage::Entered;
                Receipt    receipt;
                const auto result = ack->kind == Storage::Kind::SequenceArray
                                        ? NativeCustomAnimSequenceStorageCalls::FreeStorage(image, entry->storage, token.generation, owner, &receipt)
                                        : NativeAnimationKeyFrameStorageCalls::FreeStorage(image, entry->storage, token.generation, owner, &receipt);
                ack->receipt = receipt;
                if (receipt.released)
                {
                    // The acknowledgement survives Entry erasure and every
                    // other participating ticket sees this exact same return.
                    ack->stage = CleanupStage::Returned;
                    ack->live = nullptr;
                    ForgetFrameAuthority(entry->storage);
                    const auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return &e == entry; });
                    if (found != entries.end())
                        entries.erase(found);
                }
                else if (receipt.entered)
                {
                    ack->stage = CleanupStage::Unknown;
                    record->refused = true;
                }
                else
                    ack->stage = CleanupStage::Prepared;
                return result;
            };
            for (const auto& ack : record->frames)
            {
                status = consume(ack);
                if (status != ClockResult::Ok)
                    break;
            }
            // A blocked shared frame need not retain this already-detached
            // metadata array. Its frozen journal and shared free ACK survive.
            if (status == ClockResult::Ok)
                status = consume(record->arrayAck);
            Publish();
            if (status != ClockResult::Ok)
                return failed(status);
            if ((record->arrayAck && record->arrayAck->stage != CleanupStage::Returned) ||
                std::any_of(record->frames.begin(), record->frames.end(), [](const auto& ack) { return ack->stage != CleanupStage::Returned; }))
                return ClockResult::Ok;
        }
        if (record->request.hierarchy.locator && !record->outer.retired)
        {
            const auto result = hierarchyOwner.RetireCleanup(image, record->request.hierarchy, token.command, owner, record->outer);
            if (result != ClockResult::Ok)
                return failed(result);
        }
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if ((status = owner()) != ClockResult::Ok)
            return failed(status);
        if (!record->complete.exchange(true, std::memory_order_acq_rel))
        {
            if (record->arrayAck)
                --record->arrayAck->claims;
            for (const auto& ack : record->frames)
                --ack->claims;
        }
        output.status = ECustomAnimationCleanupStatus::Complete;
        output.disposition = ECustomAnimationCleanupDisposition::PrivateDetached;
        if (record->commandKind != CommandKind::AnimationCleanup)
        {
            record->sequenceResult.status = ECustomAnimationSequenceStatus::Complete;
            record->sequenceResult.disposition = record->commandKind == CommandKind::SequenceTransfer && record->copy.rawCopied
                                                     ? ECustomAnimationSequenceDisposition::SourceTransferred
                                                     : ECustomAnimationSequenceDisposition::SourceReleased;
        }
        Publish();
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::ResumePendingCleanups(std::uintptr_t image, std::uint64_t generation,
                                                                          NativePrivateAnimationHierarchyController& hierarchyOwner, std::size_t budget,
                                                                          CheckOwner owner, CleanupProgress& output) noexcept
    {
        const auto requested = generation;
        output = {};
        if (!requested || !owner)
            return ClockResult::InvalidArgument;
        std::vector<NativeCustomAnimationCleanupToken> pending;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            const CustomAnimSequenceControllerDetail::Operation held{operation};
            if (ownerGeneration && ownerGeneration != requested)
                return ClockResult::OwnerConflict;
            const auto checked = owner();
            if (checked != ClockResult::Ok)
            {
                output.ownerStatus = checked;
                return checked;
            }
            PruneCleanupHeld();
            try
            {
                pending.reserve((std::min)(budget, cleanups.size()));
                const auto limit = (std::min)(budget, cleanups.size());
                auto       item = cleanups.begin();
                for (std::size_t scanned = 0; scanned != limit; ++scanned)
                {
                    const auto current = item++;
                    const auto record = *current;
                    ++output.scanned;
                    if (!record->complete && record->token.generation == requested && record->hierarchyOwner == &hierarchyOwner)
                        pending.push_back(record->token);
                    // Rotate known blocked work; unrelated H retirement can
                    // release shared holds before this command's next turn.
                    cleanups.splice(cleanups.end(), cleanups, current);
                }
            }
            catch (...)
            {
                return ClockResult::WriteFailure;
            }
            Publish();
        }
        ClockResult first = ClockResult::Ok;
        for (const auto& token : pending)
        {
            CommandKind kind = CommandKind::AnimationCleanup;
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            {
                const CustomAnimSequenceControllerDetail::Operation held{operation};
                for (const auto& record : cleanups)
                    if (record->token.command == token.command && record->token.generation == token.generation)
                        kind = record->commandKind;
            }
            ++output.attempted;
            ClockResult status;
            if (kind == CommandKind::AnimationCleanup)
            {
                SCustomAnimationCleanupResult result;
                status = ResumeCleanup(image, token, hierarchyOwner, 1, owner, result);
                if (result.status == ECustomAnimationCleanupStatus::Complete)
                    ++output.released;
            }
            else
            {
                SCustomAnimationSequenceResult result;
                status = ResumeSequenceOperation(image, token, hierarchyOwner, 1, owner, result);
                if (result.status == ECustomAnimationSequenceStatus::Complete)
                    ++output.released;
            }
            if (status != ClockResult::Ok && first == ClockResult::Ok)
            {
                first = status;
                output.ownerStatus = status;
            }
            if (status != ClockResult::Ok)
                break;
        }
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        PruneCleanupHeld();
        output.retained = cleanups.size();
        output.pending = !cleanups.empty();
        for (const auto& record : cleanups)
            output.unknown = output.unknown || record->refused || record->lifetime->offCreatorDrop.load(std::memory_order_acquire);
        Publish();
        // Ordinary shared-holder waiting stays Ok/Pending. Root's End drainer
        // may retire unclaimed H/Q records before resuming this ticket again.
        return first;
    }

    ClockResult NativeCustomAnimSequenceController::AllocateStandaloneSequence(std::uintptr_t image, std::uint64_t generation, CheckOwner owner,
                                                                               NativeAnimationSequenceReferenceToken& output, Receipt* receipt,
                                                                               InvokeMalloc invoke) noexcept
    {
        std::uintptr_t tag = 0;
        const auto     result =
            AllocateResource(image, generation, 1, Storage::Kind::SequenceArray, owner, tag, receipt, invoke, SequencePurpose::StandaloneSequence);
        if (result == ClockResult::Ok)
            output = {generation, tag, 0};
        return result;
    }
    ClockResult NativeCustomAnimSequenceController::ResolveStandaloneSequence(std::uintptr_t image, std::uint64_t generation, std::uintptr_t locator,
                                                                              CheckOwner owner, NativeAnimationSequenceReferenceToken& output) noexcept
    {
        const auto requestedGeneration = generation;
        const auto requestedLocator = locator;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (!requestedGeneration || requestedGeneration != ownerGeneration || !owner)
            return ClockResult::OwnerConflict;
        const auto entry = Find(requestedGeneration, requestedLocator);
        if (entry == entries.end() || entry->purpose != SequencePurpose::StandaloneSequence || entry->cleanupOwner || entry->operationOwner ||
            entry->storage.GetKind() != Storage::Kind::SequenceArray || entry->storage.Capacity() != 1)
            return ClockResult::BindingChanged;
        std::size_t capacity = 0;
        const auto  result = NativeAnimationSequenceFactoryCalls::ReadCapacity(image, entry->storage, requestedGeneration, owner, capacity);
        if (result != ClockResult::Ok)
            return result;
        if (capacity != 1)
            return ClockResult::BindingChanged;
        output = {requestedGeneration, requestedLocator, 0};
        return ClockResult::Ok;
    }
    ClockResult NativeCustomAnimSequenceController::ReserveFrameObligation(Entry& array, Entry& frame, std::shared_ptr<CleanupResource>& output,
                                                                           bool& added) noexcept
    {
        added = false;
        if (!frame.issued || frame.storage.GetKind() != Storage::Kind::KeyFrames || frame.storage.GetState() != Storage::State::LiveUnpublished ||
            frame.storage.OwnerGeneration() != array.storage.OwnerGeneration())
            return ClockResult::BindingChanged;
        try
        {
            auto ack = frame.cleanupAck;
            if (!ack)
            {
                ack = std::make_shared<CleanupResource>();
                ack->live = &frame;
                ack->kind = Storage::Kind::KeyFrames;
                ack->generation = frame.storage.OwnerGeneration();
                ack->tag = frame.storage.IssuedTag();
            }
            if (ack->stage != CleanupStage::Prepared || ack->live != &frame)
                return ClockResult::OwnerConflict;
            if (std::find(array.frameObligations.begin(), array.frameObligations.end(), ack) == array.frameObligations.end())
            {
                array.frameObligations.push_back(ack);
                added = true;
            }
            frame.cleanupAck = ack;
            output = std::move(ack);
            return ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
    }
    void NativeCustomAnimSequenceController::RecordResourceReceipt(Entry& entry, const Receipt& receipt) noexcept
    {
        if (!entry.cleanupAck || (!receipt.entered && !receipt.released))
            return;
        auto& ack = *entry.cleanupAck;
        ack.receipt = receipt;
        if (receipt.released)
        {
            ack.stage = CleanupStage::Returned;
            ack.live = nullptr;
        }
        else if (receipt.entered)
            ack.stage = CleanupStage::Unknown;
        // A returned actual primitive is the proof, even on lost postflight.
        // Historical row obligations never infer consumption from a map miss.
    }
    ClockResult NativeCustomAnimSequenceController::BeginSequenceCleanup(std::uintptr_t image, const NativeAnimationSequenceReferenceToken& value,
                                                                         NativePrivateAnimationHierarchyController& hierarchyOwner, CheckOwner owner,
                                                                         NativeCustomAnimationSequenceOperationBeginReceipt& output) noexcept
    {
        const auto source = value;
        if (!source.ownerGeneration || !source.firstTag || source.index)
            return ClockResult::InvalidArgument;
        NativeCustomAnimationCleanupRequest request;
        request.generation = source.ownerGeneration;
        request.originalArray = source.firstTag;
        request.capacity = 1;
        return BeginCleanupImpl(image, request, hierarchyOwner, owner, output, SequencePurpose::StandaloneSequence, CommandKind::SequenceCleanup);
    }

    ClockResult NativeCustomAnimSequenceController::BeginSequenceTransfer(std::uintptr_t, const NativeAnimationSequenceReferenceToken& source,
                                                                          const NativeAnimationSequenceReferenceToken&        target,
                                                                          NativePrivateAnimationHierarchyController&          h, CheckOwner,
                                                                          NativeCustomAnimationSequenceOperationBeginReceipt& output) noexcept
    {
        return BeginSequenceCommand(CommandKind::SequenceTransfer, source, target, 0, false, false, 0, h, output);
    }
    ClockResult NativeCustomAnimSequenceController::BeginFrameAttachment(std::uintptr_t, const NativeAnimationSequenceReferenceToken& target, std::size_t count,
                                                                         bool root, bool compressed, std::uintptr_t frames,
                                                                         NativePrivateAnimationHierarchyController&          h, CheckOwner,
                                                                         NativeCustomAnimationSequenceOperationBeginReceipt& output) noexcept
    {
        return BeginSequenceCommand(CommandKind::FrameAttachment, {}, target, count, root, compressed, frames, h, output);
    }
    ClockResult NativeCustomAnimSequenceController::BeginSequenceCommand(CommandKind kind, const NativeAnimationSequenceReferenceToken& sourceValue,
                                                                         const NativeAnimationSequenceReferenceToken& targetValue, std::size_t count, bool root,
                                                                         bool compressed, std::uintptr_t frames, NativePrivateAnimationHierarchyController& h,
                                                                         NativeCustomAnimationSequenceOperationBeginReceipt& output) noexcept
    {
        const auto source = sourceValue;
        const auto target = targetValue;
        output = {};
        if (!target.ownerGeneration || !target.firstTag || (kind != CommandKind::SequenceTransfer && kind != CommandKind::FrameAttachment))
            return ClockResult::InvalidArgument;
        if (kind == CommandKind::SequenceTransfer &&
            (source.ownerGeneration != target.ownerGeneration || !source.firstTag || source.index || source.firstTag == target.firstTag))
            return ClockResult::InvalidArgument;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        const CustomAnimSequenceControllerDetail::Operation held{operation};
        if (ownerGeneration != target.ownerGeneration || h.sequences != this || stopping)
            return ClockResult::OwnerConflict;
        auto destination = Find(target.ownerGeneration, target.firstTag);
        if (destination == entries.end() || destination->storage.GetKind() != Storage::Kind::SequenceArray ||
            destination->storage.GetState() != Storage::State::LiveUnpublished || target.index >= destination->storage.Capacity() ||
            destination->cleanupOwner || destination->privateDependency)
            return ClockResult::BindingChanged;
        if (destination->operationOwner)
        {
            for (const auto& previous : cleanups)
                if (previous->token.command == destination->operationOwner && previous->commandKind == kind &&
                    previous->source.ownerGeneration == source.ownerGeneration && previous->source.firstTag == source.firstTag &&
                    previous->source.index == source.index && previous->target.firstTag == target.firstTag && previous->target.index == target.index &&
                    previous->frameCount == count && previous->rootFrames == root && previous->compressedFrames == compressed &&
                    previous->originalFrames == frames && !previous->complete)
                {
                    if (previous->lifetime->views.load(std::memory_order_acquire) == (std::numeric_limits<std::size_t>::max)())
                        return ClockResult::OwnerConflict;
                    previous->lifetime->views.fetch_add(1, std::memory_order_acq_rel);
                    output.view = NativeCustomAnimationCleanupView(previous->lifetime);
                    output.accepted = true;
                    return ClockResult::Ok;
                }
            return ClockResult::OwnerConflict;
        }
        auto original = kind == CommandKind::SequenceTransfer ? Find(source.ownerGeneration, source.firstTag) : entries.end();
        if (kind == CommandKind::SequenceTransfer &&
            (original == entries.end() || original->purpose != SequencePurpose::StandaloneSequence ||
             original->storage.GetState() != Storage::State::LiveUnpublished || original->cleanupOwner || original->operationOwner ||
             original->privateDependency || destination->purpose != SequencePurpose::ArrayRows))
            return ClockResult::BindingChanged;
        RowState* targetState = nullptr;
        RowState* sourceState = nullptr;
        auto      checked = SelectRow(target, destination, targetState);
        if (checked != ClockResult::Ok)
            return checked;
        if (kind == CommandKind::SequenceTransfer && (checked = SelectRow(source, original, sourceState)) != ClockResult::Ok)
            return checked;
        auto frame = entries.end();
        if (kind == CommandKind::FrameAttachment)
        {
            const std::size_t stride = compressed ? (root ? 16u : 10u) : (root ? 32u : 20u);
            frame = Find(target.ownerGeneration, frames);
            if (!frames || !count || frame == entries.end() || frame->storage.GetKind() != Storage::Kind::KeyFrames ||
                frame->storage.GetState() != Storage::State::LiveUnpublished || count > frame->storage.ByteSize() / stride ||
                (frame->cleanupAck && frame->cleanupAck->claims))
                return ClockResult::InvalidArgument;
        }
        else if (sourceState->frames.record)
        {
            frame = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return &e.storage == sourceState->frames.record; });
            if (frame == entries.end() || !CurrentFrameDescriptor(sourceState->frames))
                return ClockResult::BindingChanged;
        }
        std::shared_ptr<CleanupResource> obligation;
        bool                             added = false;
        if (frame != entries.end() && (checked = ReserveFrameObligation(*destination, *frame, obligation, added)) != ClockResult::Ok)
            return checked;
        std::shared_ptr<CleanupRecord> record;
        if (kind == CommandKind::SequenceTransfer)
        {
            NativeCustomAnimationCleanupRequest request;
            request.generation = source.ownerGeneration;
            request.originalArray = source.firstTag;
            request.capacity = 1;
            checked = BeginCleanupImpl(0, request, h, nullptr, output, SequencePurpose::StandaloneSequence, kind, true);
            if (!output.accepted)
            {
                if (added)
                    destination->frameObligations.pop_back();
                return checked;
            }
            for (const auto& current : cleanups)
                if (current->token.command == output.view.Token().command)
                    record = current;
        }
        else
        {
            if (nextCleanupCommand == (std::numeric_limits<std::uint64_t>::max)())
            {
                if (added)
                    destination->frameObligations.pop_back();
                return ClockResult::OwnerConflict;
            }
            try
            {
                record = std::make_shared<CleanupRecord>();
                record->commandKind = kind;
                record->token = {target.ownerGeneration, nextCleanupCommand + 1};
                record->lifetime = std::shared_ptr<NativeCustomAnimationCleanupLifetime>(new NativeCustomAnimationCleanupLifetime(record->token));
                record->array = &*destination;
                record->arrayIssue = destination->issue;
                record->request.generation = target.ownerGeneration;
                record->request.originalArray = target.firstTag;
                record->request.capacity = destination->storage.Capacity();
                record->frames.push_back(obligation);
                cleanups.push_back(record);
            }
            catch (...)
            {
                if (added)
                    destination->frameObligations.pop_back();
                return ClockResult::WriteFailure;
            }
            ++nextCleanupCommand;
            ++obligation->claims;
            output.view = NativeCustomAnimationCleanupView(record->lifetime);
            output.accepted = true;
        }
        record->hierarchyOwner = &h;
        record->source = source;
        record->target = target;
        record->targetEntry = &*destination;
        record->frameCount = count;
        record->rootFrames = root;
        record->compressedFrames = compressed;
        record->originalFrames = frames;
        record->targetObligationAdded = added;
        record->transferFrame = obligation;
        destination->operationOwner = record->token.command;
        if (original != entries.end())
            original->operationOwner = record->token.command;
        Publish();
        return checked;
    }
    ClockResult NativeCustomAnimSequenceController::ResumeSequenceOperation(std::uintptr_t image, const NativeCustomAnimationSequenceOperationToken& value,
                                                                            NativePrivateAnimationHierarchyController& h, std::size_t budget, CheckOwner owner,
                                                                            SCustomAnimationSequenceResult& output) noexcept
    {
        const auto token = value;
        output = {};
        std::shared_ptr<CleanupRecord> record;
        {
            if (operation.test_and_set(std::memory_order_acquire))
                return ClockResult::Busy;
            const CustomAnimSequenceControllerDetail::Operation held{operation};
            if (!token.generation || ownerGeneration != token.generation || !token.command)
                return ClockResult::OwnerConflict;
            for (const auto& current : cleanups)
                if (current->token.generation == token.generation && current->token.command == token.command)
                    record = current;
            if (!record || record->hierarchyOwner != &h || record->commandKind == CommandKind::AnimationCleanup)
                return ClockResult::BindingChanged;
            if (record->complete)
            {
                output = record->sequenceResult;
                return ClockResult::Ok;
            }
            if (record->refused)
            {
                output.status = ECustomAnimationSequenceStatus::Refused;
                return ClockResult::OwnerConflict;
            }
            if (!owner)
                return ClockResult::NotInitialized;
            const auto qualified = owner();
            if (qualified != ClockResult::Ok)
                return qualified;
            if (record->resuming.exchange(true, std::memory_order_acq_rel))
                return ClockResult::Busy;
            struct Scope
            {
                std::atomic<bool>& value;
                ~Scope() { value.store(false, std::memory_order_release); }
            } scope{record->resuming};
            if (record->commandKind != CommandKind::SequenceCleanup && !record->writerAcknowledged)
            {
                auto* target = record->targetEntry;
                if (!target || target->operationOwner != token.command || target->issue == 0 || target->privateDependency)
                    return ClockResult::BindingChanged;
                RowState* targetState = nullptr;
                for (auto& row : target->rows)
                    if (row.index == record->target.index)
                        targetState = &row;
                if (!targetState)
                    return ClockResult::BindingChanged;
                const bool binding = record->commandKind == CommandKind::FrameAttachment;
                const bool entered = binding ? record->attachment.entered : record->copy.entered;
                const bool accepted = binding ? record->attachment.accepted : record->copy.rawCopied;
                if (record->cancelRequested && !entered)
                {
                    if (!binding && record->targetObligationAdded && record->transferFrame)
                        target->frameObligations.erase(std::remove(target->frameObligations.begin(), target->frameObligations.end(), record->transferFrame),
                                                       target->frameObligations.end());
                    target->operationOwner = 0;
                    record->writerAcknowledged = true;
                    if (binding)
                    {
                        // Cancellation follows an explicit source cleanup
                        // intent, never CPU-view destruction or raw pointer0.
                        --record->frames.front()->claims;
                        record->complete = true;
                        record->sequenceResult.status = ECustomAnimationSequenceStatus::Refused;
                        output = record->sequenceResult;
                        Publish();
                        return ClockResult::Ok;
                    }
                }
                else if (entered && !accepted)
                {
                    record->refused = true;
                    output.status = ECustomAnimationSequenceStatus::Refused;
                    Publish();
                    return ClockResult::OwnerConflict;
                }
                else
                {
                    ClockResult status = ClockResult::Ok;
                    if (!entered && binding)
                    {
                        const auto frame = Find(token.generation, record->originalFrames);
                        if (frame == entries.end() || frame->cleanupAck != record->frames.front())
                            return ClockResult::BindingChanged;
                        status = record->compressedFrames
                                     ? NativeAnimationSequenceFrameBindCalls::AttachCompressed(image, target->storage, token.generation, record->target.index,
                                                                                               frame->storage, record->frameCount, record->rootFrames, owner,
                                                                                               &record->attachment)
                                     : NativeAnimationSequenceFrameBindCalls::AttachUncompressed(image, target->storage, token.generation, record->target.index,
                                                                                                 frame->storage, record->frameCount, record->rootFrames, owner,
                                                                                                 &record->attachment);
                        if (record->attachment.accepted)
                            targetState->frames = {record->compressedFrames ? FrameFormat::Compressed : FrameFormat::Uncompressed, &frame->storage,
                                                   record->originalFrames, record->frameCount, record->rootFrames};
                    }
                    else if (!entered)
                    {
                        RowState* sourceState = nullptr;
                        for (auto& row : record->array->rows)
                            if (row.index == 0)
                                sourceState = &row;
                        if (!sourceState)
                            return ClockResult::BindingChanged;
                        const auto logical = sourceState->logical;
                        const auto format = sourceState->frames;
                        status = NativeAnimationSequenceCopyCalls::Copy(image, target->storage, record->target.index, record->array->storage, 0,
                                                                        token.generation, owner, &record->copy);
                        if (record->copy.rawCopied)
                        {
                            targetState->logical = logical;
                            targetState->frames = format.Matches(record->copy.sourceValue) && CurrentFrameDescriptor(format) ? format : FrameDescriptor{};
                            if (record->copy.sourceValue.keyFrames && !targetState->frames.record)
                            {
                                record->refused = true;
                                Publish();
                                return ClockResult::BindingChanged;
                            }
                            // The exact accepted copy transfers only its
                            // actual current frame, not every historical
                            // buffer previously owned by this source row.
                            for (auto item = record->frames.begin(); item != record->frames.end();)
                                if ((*item)->tag == record->copy.sourceValue.keyFrames)
                                {
                                    const auto ack = *item;
                                    --ack->claims;
                                    record->array->frameObligations.erase(
                                        std::remove(record->array->frameObligations.begin(), record->array->frameObligations.end(), ack),
                                        record->array->frameObligations.end());
                                    item = record->frames.erase(item);
                                }
                                else
                                    ++item;
                        }
                    }
                    const bool nowEntered = binding ? record->attachment.entered : record->copy.entered;
                    const bool nowAccepted = binding ? record->attachment.accepted : record->copy.rawCopied;
                    if (nowEntered && !nowAccepted)
                        record->refused = true;
                    if (status != ClockResult::Ok)
                    {
                        if (record->refused)
                            output.status = ECustomAnimationSequenceStatus::Refused;
                        Publish();
                        return status;
                    }
                    if (!nowAccepted)
                        return ClockResult::Ok;
                    if (entered)
                    {
                        // Only a previously returned complete CPU store reaches
                        // this read-only exact80-byte acknowledgement. Unknown
                        // writes never acquire replay/rollback permission.
                        if (record->transferFrame && record->transferFrame->live)
                        {
                            const auto& frame = record->transferFrame->live->storage;
                            status = NativeAnimationSequenceRowCalls::CheckFramePointer(image, frame, token.generation, record->transferFrame->tag, owner);
                            if (status != ClockResult::Ok)
                                return status;
                        }
                        const auto& intent = binding ? record->attachment.intended : record->copy.copiedRecord;
                        status = NativeAnimationSequenceOperationCalls::AcknowledgeAcceptedRow(image, target->storage, token.generation, record->target.index,
                                                                                               intent, binding, owner);
                        if (status != ClockResult::Ok)
                            return status;
                    }
                    record->writerAcknowledged = true;
                    target->operationOwner = 0;
                    if (binding)
                    {
                        --record->frames.front()->claims;
                        record->complete = true;
                        record->sequenceResult.status = ECustomAnimationSequenceStatus::Complete;
                        record->sequenceResult.disposition = ECustomAnimationSequenceDisposition::FramesAttached;
                        output = record->sequenceResult;
                        Publish();
                        return ClockResult::Ok;
                    }
                }
                if (record->array)
                    record->array->operationOwner = 0;
                Publish();
            }
        }
        // Sequence cleanup and a completed/cancelled copy use exactly the
        // original per-resource cleanup journal. No new raw lookup, source
        // getter, Frame free or generation-wide retirement is fabricated.
        SCustomAnimationCleanupResult cleanup;
        const auto                    status = ResumeCleanup(image, token, h, budget, owner, cleanup);
        if (cleanup.status == ECustomAnimationCleanupStatus::Complete)
            output = record->sequenceResult;
        else if (cleanup.status == ECustomAnimationCleanupStatus::Refused)
            output.status = ECustomAnimationSequenceStatus::Refused;
        return status;
    }

}
