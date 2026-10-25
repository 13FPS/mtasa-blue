/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain native sequence storage across failed publication and End
 ****************************************************************************/
#include "NativeCustomAnimSequenceController.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativeAnimationSequenceFactoryCalls.h"
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
        retained.store(!entries.empty(), std::memory_order_release);
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
        if (array == entries.end() || array->storage.GetKind() != Storage::Kind::SequenceArray)
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
        if (entry->privateDependency || entry->privateHolds)
            return ClockResult::OwnerConflict;
        Receipt    local;
        const auto result = kind == Storage::Kind::SequenceArray
                                ? NativeCustomAnimSequenceStorageCalls::FreeStorage(image, entry->storage, generation, owner, &local, invoke)
                                : NativeAnimationKeyFrameStorageCalls::FreeStorage(image, entry->storage, generation, owner, &local, invoke);
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
        const auto  logical = sourceState->logical;
        const auto  frames = sourceState->frames;
        CopyReceipt local;
        result = NativeAnimationSequenceCopyCalls::Copy(image, targetEntry->storage, request.index, sourceEntry->storage, sourceToken.index,
                                                        request.ownerGeneration, owner, &local);
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
        // This restricted builder links existing private records only. It
        // does not implement the general setter, publish to GTA, infer shared
        // bit08 from a pointer or transfer either allocation's free authority.
        FrameBindReceipt local;
        result = requestedCompressed ? NativeAnimationSequenceFrameBindCalls::AttachCompressed(image, array->storage, request.ownerGeneration, request.index,
                                                                                               frames->storage, requestedCount, requestedRoot, owner, &local)
                                     : NativeAnimationSequenceFrameBindCalls::AttachUncompressed(image, array->storage, request.ownerGeneration, request.index,
                                                                                                 frames->storage, requestedCount, requestedRoot, owner, &local);
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
        if (entry == entries.end())
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
            if (entry->privateDependency || entry->privateHolds)
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
}
