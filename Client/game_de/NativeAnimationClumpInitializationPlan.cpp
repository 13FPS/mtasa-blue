/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share current clump inputs without shape-specific initialization or claims
 ****************************************************************************/
#include "NativeAnimationClumpInitializationPlan.h"
#include "NativeStaticAssociationInitializeProfile.h"
#include "NativeAnimationHierarchyMemory.h"
#include "NativeAnimationManagerProfile.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativePedRenderMemory.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelReferenceHeap.h"
#include "NativeModelTypeLeaf.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeStartGameCalls.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>
namespace mta::game_de::NativeAnimationClumpInitializationPlan
{
    namespace M = NativeModelReferenceMap;
    namespace I = NativeImageAccess;
    template <class T, std::size_t N>
    T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
    {
        T value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
    bool QualifyProfile(std::uintptr_t image) noexcept
    {
        std::array<unsigned char, P::MaximumPinBytes> actual{};
        for (const auto& pin : P::Pins)
            if (pin.rva != P::EntryRva &&
                (!pin.size || pin.size > actual.size() || !I::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                 std::memcmp(actual.data(), pin.bytes, pin.size)))
                return false;
        for (const auto& pin : S::Code)
            if (!pin.size || pin.size > actual.size() || !I::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        std::array<unsigned char, NativeAnimationManagerProfile::NameFold.size()> fold{};
        std::uintptr_t                                                            imported = 0;
        const auto                                                                runtimeModule = GetModuleHandleW(L"VCRUNTIME140.dll");
        const auto strstrAddress = runtimeModule ? reinterpret_cast<std::uintptr_t>(GetProcAddress(runtimeModule, "strstr")) : 0;
        // Pin the actual import identity without calling it. Native FFB630
        // keeps its substring test and final-six-byte removal unchanged.
        return D::Profile(image) && H::QualifyProfile(image) && strstrAddress && I::ReadImage(image, image + P::StrstrImportRva, &imported, sizeof(imported)) &&
               imported == strstrAddress && I::ReadImage(image, image + NativeAnimationManagerProfile::NameFoldRva, fold.data(), fold.size()) &&
               fold == NativeAnimationManagerProfile::NameFold;
    }
    bool HeapSlots(std::uintptr_t image, const H::Value& heap) noexcept
    {
        const auto matches = [&](std::uintptr_t table, std::uintptr_t allocate, std::uintptr_t release)
        {
            std::uintptr_t a = 0, b = 0;
            return H::ReadOnlyImage(image, table, 0x40) && I::ReadImage(image, table + 0x10, &a, sizeof(a)) &&
                   I::ReadImage(image, table + 0x30, &b, sizeof(b)) && a == image + allocate && b == image + release;
        };
        return matches(heap.authorityVtable, S::AuthorityMallocRva, S::AuthorityFreeRva) &&
               (heap.current == heap.authority || matches(heap.currentVtable, S::ProxyMallocRva, S::ProxyFreeRva));
    }
    bool Same(const BoneMap& a, const BoneMap& b) noexcept
    {
        return a.header == b.header && a.rowsAddress == b.rowsAddress && a.bitsAddress == b.bitsAddress && a.bucketsAddress == b.bucketsAddress &&
               a.rows == b.rows && a.bits == b.bits && a.buckets == b.buckets;
    }
    ClockResult CaptureMap(std::uintptr_t container, std::uint16_t frameCount, BoneMap& output)
    {
        BoneMap    value;
        const auto base = container + P::MapOffset;
        if (!M::Copy(base, value.header.data(), value.header.size()))
            return ClockResult::ReadFailure;
        const auto num = Field<std::int32_t>(value.header, 8), capacity = Field<std::int32_t>(value.header, 0x0c);
        const auto numBits = Field<std::int32_t>(value.header, 0x28), maxBits = Field<std::int32_t>(value.header, 0x2c);
        const auto freeHead = Field<std::int32_t>(value.header, 0x30), freeCount = Field<std::int32_t>(value.header, 0x34);
        const auto buckets = Field<std::int32_t>(value.header, 0x48);
        if (num < 0 || num > P::MaximumMapRows || capacity < num || capacity > P::MaximumMapRows || numBits != num || maxBits < num || maxBits % 32 ||
            maxBits > P::MaximumMapRows || freeCount < 0 || freeCount > num || buckets < 0 || buckets > P::MaximumMapBuckets || (buckets & (buckets - 1)))
            return ClockResult::BindingChanged;
        value.rowsAddress = Field<std::uintptr_t>(value.header, 0);
        const auto externalBits = Field<std::uintptr_t>(value.header, 0x20), externalBuckets = Field<std::uintptr_t>(value.header, 0x40);
        if ((!externalBits && maxBits > 128) || (!externalBuckets && buckets > 1) || (!buckets && externalBuckets) || (num != freeCount && !buckets))
            return ClockResult::BindingChanged;
        if (!num)
        {
            if (value.rowsAddress || capacity || externalBits || freeHead != -1 || freeCount || (maxBits && maxBits != 128) || buckets > 1 || externalBuckets ||
                (buckets == 1 && Field<std::int32_t>(value.header, 0x38) != -1))
                return ClockResult::BindingChanged;
            output = std::move(value);
            return ClockResult::Ok;
        }
        value.bitsAddress = externalBits ? externalBits : base + 0x10;
        value.bucketsAddress = externalBuckets ? externalBuckets : base + 0x38;
        const auto rowBytes = static_cast<std::size_t>(num) * sizeof(BoneRow);
        const auto bitBytes = ((static_cast<std::size_t>(num) + 31) / 32) * 4;
        const auto bucketBytes = static_cast<std::size_t>(buckets) * 4;
        if (!value.rowsAddress || value.rowsAddress % 8 || value.bitsAddress % 4 || value.bucketsAddress % 4 ||
            !M::PrivateWritable(value.rowsAddress, static_cast<std::size_t>(capacity) * sizeof(BoneRow)) ||
            !M::Disjoint(value.rowsAddress, rowBytes, container, P::ContainerBytes) || !M::Disjoint(value.rowsAddress, rowBytes, value.bitsAddress, bitBytes) ||
            (bucketBytes && (!M::Disjoint(value.rowsAddress, rowBytes, value.bucketsAddress, bucketBytes) ||
                             !M::Disjoint(value.bitsAddress, bitBytes, value.bucketsAddress, bucketBytes))) ||
            (externalBits && !M::Disjoint(value.bitsAddress, bitBytes, container, P::ContainerBytes)) ||
            (externalBuckets && bucketBytes && !M::Disjoint(value.bucketsAddress, bucketBytes, container, P::ContainerBytes)))
            return ClockResult::BindingChanged;
        value.rows.resize(static_cast<std::size_t>(num));
        value.bits.resize(bitBytes / 4);
        value.buckets.resize(static_cast<std::size_t>(buckets));
        if (!M::Copy(value.rowsAddress, value.rows.data(), rowBytes) || !M::Copy(value.bitsAddress, value.bits.data(), bitBytes) ||
            (bucketBytes && !M::Copy(value.bucketsAddress, value.buckets.data(), bucketBytes)))
            return ClockResult::ReadFailure;
        const auto                 occupied = [&](std::size_t index) { return (value.bits[index / 32] & (std::uint32_t{1} << (index % 32))) != 0; };
        std::vector<unsigned char> visited(static_cast<std::size_t>(num));
        std::vector<std::int32_t>  keys;
        std::size_t                active = 0;
        for (std::int32_t bucket = 0; bucket != buckets; ++bucket)
            for (auto index = value.buckets[static_cast<std::size_t>(bucket)]; index != -1;)
            {
                if (index < 0 || index >= num || !occupied(static_cast<std::size_t>(index)) || visited[static_cast<std::size_t>(index)])
                    return ClockResult::BindingChanged;
                visited[static_cast<std::size_t>(index)] = 1;
                const auto& row = value.rows[static_cast<std::size_t>(index)];
                if (row.frame < 0 || row.frame >= frameCount || row.bucket != bucket ||
                    (static_cast<std::uint32_t>(row.key) & static_cast<std::uint32_t>(buckets - 1)) != static_cast<std::uint32_t>(bucket))
                    return ClockResult::BindingChanged;
                keys.push_back(row.key);
                ++active;
                index = row.next;
            }
        std::sort(keys.begin(), keys.end());
        if (std::adjacent_find(keys.begin(), keys.end()) != keys.end())
            return ClockResult::BindingChanged;
        std::int32_t previous = -1, freeSeen = 0;
        for (auto index = freeHead; index != -1;)
        {
            if (index < 0 || index >= num || occupied(static_cast<std::size_t>(index)) || visited[static_cast<std::size_t>(index)] || freeSeen >= freeCount)
                return ClockResult::BindingChanged;
            const auto& row = value.rows[static_cast<std::size_t>(index)];
            if (row.key != previous)
                return ClockResult::BindingChanged;
            visited[static_cast<std::size_t>(index)] = 1;
            previous = index;
            index = row.frame;
            ++freeSeen;
        }
        if (freeSeen != freeCount || active + static_cast<std::size_t>(freeCount) != static_cast<std::size_t>(num) ||
            std::find(visited.begin(), visited.end(), 0) != visited.end())
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    bool SameMembership(const NativeUObjectMembershipAccess::Snapshot& a, const NativeUObjectMembershipAccess::Snapshot& b) noexcept
    {
        return a.objectAddress == b.objectAddress && a.vtableAddress == b.vtableAddress && a.classAddress == b.classAddress &&
               a.outerAddress == b.outerAddress && a.itemAddress == b.itemAddress && a.index == b.index && a.serial == b.serial;
    }
    bool Same(const Snapshot& a, const Snapshot& b) noexcept
    {
        return a.metadata.modelIndex == b.metadata.modelIndex && a.metadata.entityType == b.metadata.entityType && a.model.slotAddress == b.model.slotAddress &&
               a.model.nativeModelInfo == b.model.nativeModelInfo && a.loaded.rawFlags == b.loaded.rawFlags && a.loaded.rawLoadState == b.loaded.rawLoadState &&
               C::Same(a.modelContext, b.modelContext) && SameMembership(a.modelContext.rootMembership, b.modelContext.rootMembership) &&
               SameMembership(a.modelContext.providerMembership, b.modelContext.providerMembership) &&
               a.hierarchy.interfaceAddress == b.hierarchy.interfaceAddress && a.hierarchy.hash == b.hierarchy.hash &&
               a.hierarchy.totalTimeBits == b.hierarchy.totalTimeBits && a.hierarchy.sequenceCount == b.hierarchy.sequenceCount &&
               a.hierarchy.runningCompressed == b.hierarchy.runningCompressed && a.hierarchy.blockId == b.hierarchy.blockId && a.typeBytes == b.typeBytes &&
               a.modelType == b.modelType && a.container == b.container && a.cleanup == b.cleanup && a.renderVtable == b.renderVtable &&
               a.actorClass == b.actorClass && a.renderClass == b.renderClass && a.classRows == b.classRows && a.classCell == b.classCell &&
               a.classDepth == b.classDepth && a.renderDepth == b.renderDepth && a.eventMethod == b.eventMethod && a.nameMethod == b.nameMethod &&
               a.skeletonMethod == b.skeletonMethod && a.component == b.component && a.skeleton == b.skeleton && a.fallbackControl == b.fallbackControl &&
               a.frameCount == b.frameCount && a.frameKeys == b.frameKeys && a.nameIndices == b.nameIndices && a.sequences == b.sequences &&
               a.sequenceCapacity == b.sequenceCapacity && a.sequenceAllocationBytes == b.sequenceAllocationBytes && a.sequenceRows == b.sequenceRows &&
               Same(a.map, b.map) && H::Same(a.heap, b.heap) && D::Same(a.world, b.world, true);
    }
    ClockResult Access::CaptureInHeldScope(const Origin& origin, Snapshot& output)
    {
        const auto  image = origin.image;
        const auto& input = origin.input;
        const auto  owner = origin.owner;
        Snapshot    value;
        auto        result = D::Owner(image, owner);
        if (result != ClockResult::Ok)
            return result;
        if (!QualifyProfile(image))
            return ClockResult::BindingChanged;
        if ((result = D::Capture(image, value.world)) != ClockResult::Ok)
            return result;
        std::uintptr_t render = 0;
        if (origin.temporaryModel)
        {
            if ((result = (origin.recheck ? origin.recheck(origin.context) : ClockResult::NotInitialized)) != ClockResult::Ok)
                return result;
            value.model = *origin.temporaryModel;
            if ((result = C::Capture(image, value.model, value.modelContext)) != ClockResult::Ok)
                return result;
            render = input.clump;
        }
        else
        {
            if ((result = NativePedPoolReader({image}, owner).Recheck(input.ped)) != ClockResult::Ok ||
                (result = NativePedMetadataMemory({image}, owner).Read(input.ped, value.metadata)) != ClockResult::Ok)
                return result;
            if (value.metadata.entityType != 3)
                return ClockResult::UnsupportedBuild;
            if ((result = NativeModelBankReader({image}, owner).Read(value.metadata.modelIndex, value.model)) != ClockResult::Ok ||
                (result = NativeModelLoadedReader({image}, owner).Read(value.metadata.modelIndex, value.loaded)) != ClockResult::Ok)
                return result;
            if (!value.model.nativeModelInfo || !value.loaded.streamedIn)
                return ClockResult::NotInitialized;
            if ((result = C::Capture(image, value.model, value.modelContext)) != ClockResult::Ok)
                return result;
            if (!NativeModelTypeLeaf::Read(image, value.modelContext.typeMethod, value.typeBytes, value.modelType) || value.modelType != 5)
                return ClockResult::BindingChanged;
            if ((result = NativePedRenderMemory({image}, owner).Read(input.ped, render)) != ClockResult::Ok)
                return result;
        }
        if (!render || render != input.clump || render % 8 || !M::PrivateWritable(render, 0x310) ||
            (!origin.temporaryModel && !M::Disjoint(render, 0x310, input.ped.nativePed, NativePedPoolProfile::ObjectStride)) ||
            !D::Add(image, render, value.world) || !D::At(render, 0, value.renderVtable) || !D::At(render, 0x10, value.renderClass) ||
            !H::ReadOnlyImage(image, value.renderVtable, 0x6d0) ||
            !I::ReadImage(image, value.renderVtable + 0x218, &value.eventMethod, sizeof(value.eventMethod)) ||
            !I::ReadImage(image, value.renderVtable + 0x698, &value.nameMethod, sizeof(value.nameMethod)) ||
            !I::ReadImage(image, value.renderVtable + 0x6c8, &value.skeletonMethod, sizeof(value.skeletonMethod)) ||
            !I::ImagePages(image, value.eventMethod, 1, false, true) || value.nameMethod != image + P::NameProviderRva ||
            value.skeletonMethod != image + P::SkeletonProviderRva ||
            !I::ReadImage(image, image + P::ActorClassSlotRva, &value.actorClass, sizeof(value.actorClass), true) || !value.actorClass ||
            !D::Add(image, value.actorClass, value.world) || !D::Add(image, value.renderClass, value.world) ||
            !D::At(value.actorClass, 0x38, value.classDepth) || !D::At(value.renderClass, 0x38, value.renderDepth) || value.classDepth < 0 ||
            value.renderDepth < value.classDepth || value.renderDepth > 1024 || !D::At(value.renderClass, 0x30, value.classRows) ||
            !D::At(value.classRows, static_cast<std::size_t>(value.classDepth) * 8, value.classCell) || value.classCell != value.actorClass + 0x30)
            return ClockResult::BindingChanged;
        // Source ancestry admits the current model-produced actor. Exact
        // provider slots additionally refuse an unqualified subclass override;
        // the original event dispatcher remains inside the genuine call.
        if (!D::At(render, 0x298, value.component) ||
            (value.component &&
             (!D::Add(image, value.component, value.world) || !M::PrivateWritable(value.component, 0x480) || !D::At(value.component, 0x478, value.skeleton))) ||
            (value.skeleton && !D::Add(image, value.skeleton, value.world)) || !D::At(render, 0x300, value.container) || !D::At(render, 0x308, value.cleanup) ||
            value.cleanup != image + P::CleanupRva || !value.container || value.container % 8 || !M::PrivateWritable(value.container, P::ContainerBytes) ||
            !M::Disjoint(value.container, P::ContainerBytes, render, 0x310) ||
            (!origin.temporaryModel && !M::Disjoint(value.container, P::ContainerBytes, input.ped.nativePed, NativePedPoolProfile::ObjectStride)))
            return ClockResult::BindingChanged;
        std::uintptr_t backpointer = 0, containerOwner = 0;
        std::int32_t   count = 0, frameArrayCount = 0, frameCapacity = 0;
        if (!D::At(value.container, 0x18, backpointer) || backpointer != render || !D::At(value.container, 0x20, containerOwner) ||
            containerOwner != (origin.temporaryModel ? std::uintptr_t{0} : input.ped.nativePed + 0x6e4) || !D::At(value.container, 0x10, count) ||
            ((origin.temporaryModel || origin.allowEmptyFrames) ? count < 0 : count <= 0) || count > static_cast<std::int32_t>(P::FrameCapacity) ||
            !D::At(value.container, 0x988, frameArrayCount) || frameArrayCount != count || !D::At(value.container, 0x98c, frameCapacity) ||
            frameCapacity != static_cast<std::int32_t>(P::FrameCapacity))
            return ClockResult::BindingChanged;
        value.frameCount = static_cast<std::uint16_t>(count);
        for (std::size_t index = 0; index != value.frameCount; ++index)
            if (!D::At(value.container, P::FrameOffset + index * P::FrameStride + 0x18, value.frameKeys[index]) ||
                !D::At(value.container, 0x11cc + index * 4, value.nameIndices[index]) || value.nameIndices[index] < -1)
                return ClockResult::BindingChanged;
        if ((result = CaptureMap(value.container, value.frameCount, value.map)) != ClockResult::Ok ||
            (result = H::Capture(image, value.heap)) != ClockResult::Ok)
            return result;
        if (!HeapSlots(image, value.heap) ||
            !I::ReadImage(image, image + P::NameFallbackControlRva, &value.fallbackControl, sizeof(value.fallbackControl), true))
            return ClockResult::BindingChanged;
        if (origin.readHierarchy)
        {
            if ((result = origin.readHierarchy(origin.context, value.hierarchy, value.sequences)) != ClockResult::Ok)
                return result;
        }
        else
        {
            NativeAnimationHierarchyMemory hierarchyMemory({image}, owner);
            if ((result = hierarchyMemory.Read(input.hierarchy, value.hierarchy)) != ClockResult::Ok ||
                (result = hierarchyMemory.ReadSequences(input.hierarchy, value.sequences)) != ClockResult::Ok)
                return result;
        }
        // FEB5F0 tests the logical count as signed WORD. Bound that actual
        // logical prefix by the independent full allocation cookie; no native
        // capacity is inferred from the count and no names are recomputed.
        if (value.hierarchy.sequenceCount > static_cast<std::uint16_t>((std::numeric_limits<std::int16_t>::max)()))
            return ClockResult::BindingChanged;
        if (!value.sequences)
        {
            if (value.hierarchy.sequenceCount)
                return ClockResult::BindingChanged;
        }
        else
        {
            if (value.sequences < 8 || value.sequences % 8 || !M::Copy(value.sequences - 8, &value.sequenceCapacity, sizeof(value.sequenceCapacity)) ||
                value.sequenceCapacity < value.hierarchy.sequenceCount ||
                value.sequenceCapacity > ((std::numeric_limits<std::size_t>::max)() - 8) / S::RowBytes)
                return ClockResult::BindingChanged;
            value.sequenceAllocationBytes = 8 + static_cast<std::size_t>(value.sequenceCapacity) * S::RowBytes;
            if (!M::PrivateWritable(value.sequences - 8, value.sequenceAllocationBytes) ||
                !M::Disjoint(value.sequences - 8, value.sequenceAllocationBytes, value.container, P::ContainerBytes) ||
                !M::Disjoint(value.sequences - 8, value.sequenceAllocationBytes, render, 0x310) ||
                !M::Disjoint(value.sequences - 8, value.sequenceAllocationBytes, input.association, origin.receiverBytes))
                return ClockResult::BindingChanged;
            value.sequenceRows.resize(value.hierarchy.sequenceCount);
            if (!value.sequenceRows.empty() && !M::Copy(value.sequences, value.sequenceRows.data(), value.sequenceRows.size() * S::RowBytes))
                return ClockResult::ReadFailure;
            for (const auto& sequence : value.sequenceRows)
                if (!(sequence[S::FlagsOffset] & 0x10) && std::find(sequence.begin(), sequence.begin() + 64, 0) == sequence.begin() + 64)
                    return ClockResult::BindingChanged;
        }
        // Typed callers separately verify their static32 or dynamic104
        // receiver. Only the exclusion span belongs to this input plan.
        if (!origin.receiverBytes || !M::Disjoint(input.association, origin.receiverBytes, value.container, P::ContainerBytes) ||
            !M::Disjoint(input.association, origin.receiverBytes, render, 0x310))
            return ClockResult::BindingChanged;
        // Readable actor/type bytes are insufficient once an item enters
        // retirement or its slot is reused. Extend the existing current
        // membership/serial graph before dereferences and preserve those
        // additions through both call bookends; no new GC lease is issued.
        for (std::size_t index = 0; index != value.world.count; ++index)
            if (!D::Add(image, value.world.members[index].classAddress, value.world))
                return ClockResult::BindingChanged;
        if (origin.recheck && (result = origin.recheck(origin.context)) != ClockResult::Ok)
            return result;
        if (!origin.temporaryModel && (result = NativePedPoolReader({image}, owner).Recheck(input.ped)) != ClockResult::Ok)
            return result;
        if (!D::Fence(image, value.world) || !QualifyProfile(image))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return result;
        output = std::move(value);
        return ClockResult::Ok;
    }
    ClockResult Access::RecheckInHeldScope(const Origin& origin, const Snapshot& expected)
    {
        Snapshot   current;
        const auto result = CaptureInHeldScope(origin, current);
        return result != ClockResult::Ok ? result : Same(expected, current) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}
