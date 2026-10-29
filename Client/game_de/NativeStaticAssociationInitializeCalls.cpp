/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow actual clump storage for one original static-association Init
 ****************************************************************************/
#include "NativeStaticAssociationInitializeCalls.h"
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
namespace mta::game_de::NativeStaticAssociationInitializeCalls
{
    namespace
    {
        namespace P = NativeStaticAssociationInitializeProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace H = NativeModelReferenceHeap;
        namespace M = NativeModelReferenceMap;
        namespace C = NativeStreamingModelContext;
        namespace I = NativeImageAccess;
        namespace S = NativeCustomAnimSequenceStorageProfile;
        using Row = std::array<unsigned char, 32>;
        template <class T, std::size_t N>
        T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            for (const auto& pin : P::Pins)
                if (!pin.size || pin.size > actual.size() || !I::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
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
            return D::Profile(image) && H::QualifyProfile(image) && strstrAddress &&
                   I::ReadImage(image, image + P::StrstrImportRva, &imported, sizeof(imported)) && imported == strstrAddress &&
                   I::ReadImage(image, image + NativeAnimationManagerProfile::NameFoldRva, fold.data(), fold.size()) &&
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
        struct BoneRow
        {
            std::int32_t key = 0, frame = 0, next = -1, bucket = -1;
            bool         operator==(const BoneRow& b) const noexcept { return key == b.key && frame == b.frame && next == b.next && bucket == b.bucket; }
        };
        static_assert(sizeof(BoneRow) == 16);
        struct BoneMap
        {
            std::array<unsigned char, P::MapHeaderBytes> header{};
            std::uintptr_t                               rowsAddress = 0, bitsAddress = 0, bucketsAddress = 0;
            std::vector<BoneRow>                         rows;
            std::vector<std::uint32_t>                   bits;
            std::vector<std::int32_t>                    buckets;
        };
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
                if (value.rowsAddress || capacity || externalBits || freeHead != -1 || freeCount || (maxBits && maxBits != 128) || buckets > 1 ||
                    externalBuckets || (buckets == 1 && Field<std::int32_t>(value.header, 0x38) != -1))
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
                !M::Disjoint(value.rowsAddress, rowBytes, container, P::ContainerBytes) ||
                !M::Disjoint(value.rowsAddress, rowBytes, value.bitsAddress, bitBytes) ||
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
        struct Value
        {
            D::Value                       world;
            H::Value                       heap;
            NativePedMetadata              metadata;
            NativeModelBankReader::Value   model;
            NativeModelLoadedReader::Value loaded;
            C::Value                       modelContext;
            NativeAnimationHierarchyValues hierarchy;
            Row                            row{};
            std::array<unsigned char, 6>   typeBytes{};
            std::uint8_t                   modelType = 0;
            std::uintptr_t                 container = 0, cleanup = 0, renderVtable = 0, actorClass = 0, renderClass = 0, classRows = 0, classCell = 0;
            std::uintptr_t                 eventMethod = 0, nameMethod = 0, skeletonMethod = 0, component = 0, skeleton = 0, fallbackControl = 0;
            std::int32_t                   classDepth = -1, renderDepth = -1;
            std::uint16_t                  frameCount = 0;
            std::array<std::uint32_t, P::FrameCapacity>         frameKeys{};
            std::array<std::int32_t, P::FrameCapacity>          nameIndices{};
            std::uintptr_t                                      sequences = 0;
            std::uint64_t                                       sequenceCapacity = 0;
            std::size_t                                         sequenceAllocationBytes = 0;
            std::vector<std::array<unsigned char, S::RowBytes>> sequenceRows;
            BoneMap                                             map;
        };
        bool SameMembership(const NativeUObjectMembershipAccess::Snapshot& a, const NativeUObjectMembershipAccess::Snapshot& b) noexcept
        {
            return a.objectAddress == b.objectAddress && a.vtableAddress == b.vtableAddress && a.classAddress == b.classAddress &&
                   a.outerAddress == b.outerAddress && a.itemAddress == b.itemAddress && a.index == b.index && a.serial == b.serial;
        }
        bool Same(const Value& a, const Value& b) noexcept
        {
            return a.metadata.modelIndex == b.metadata.modelIndex && a.metadata.entityType == b.metadata.entityType &&
                   a.model.slotAddress == b.model.slotAddress && a.model.nativeModelInfo == b.model.nativeModelInfo && a.loaded.rawFlags == b.loaded.rawFlags &&
                   a.loaded.rawLoadState == b.loaded.rawLoadState && C::Same(a.modelContext, b.modelContext) &&
                   SameMembership(a.modelContext.rootMembership, b.modelContext.rootMembership) &&
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
        ClockResult Capture(std::uintptr_t image, const Arguments& input, CheckOwner owner, Value& output)
        {
            Value value;
            auto  result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            if ((result = D::Capture(image, value.world)) != ClockResult::Ok)
                return result;
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
            std::uintptr_t render = 0;
            if ((result = NativePedRenderMemory({image}, owner).Read(input.ped, render)) != ClockResult::Ok)
                return result;
            if (!render || render != input.clump || render % 8 || !M::PrivateWritable(render, 0x310) ||
                !M::Disjoint(render, 0x310, input.ped.nativePed, NativePedPoolProfile::ObjectStride) || !D::Add(image, render, value.world) ||
                !D::At(render, 0, value.renderVtable) || !D::At(render, 0x10, value.renderClass) || !H::ReadOnlyImage(image, value.renderVtable, 0x6d0) ||
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
                (value.component && (!D::Add(image, value.component, value.world) || !M::PrivateWritable(value.component, 0x480) ||
                                     !D::At(value.component, 0x478, value.skeleton))) ||
                (value.skeleton && !D::Add(image, value.skeleton, value.world)) || !D::At(render, 0x300, value.container) ||
                !D::At(render, 0x308, value.cleanup) || value.cleanup != image + P::CleanupRva || !value.container || value.container % 8 ||
                !M::PrivateWritable(value.container, P::ContainerBytes) || !M::Disjoint(value.container, P::ContainerBytes, render, 0x310) ||
                !M::Disjoint(value.container, P::ContainerBytes, input.ped.nativePed, NativePedPoolProfile::ObjectStride))
                return ClockResult::BindingChanged;
            std::uintptr_t backpointer = 0, containerOwner = 0;
            std::int32_t   count = 0, frameArrayCount = 0, frameCapacity = 0;
            if (!D::At(value.container, 0x18, backpointer) || backpointer != render || !D::At(value.container, 0x20, containerOwner) ||
                containerOwner != input.ped.nativePed + 0x6e4 || !D::At(value.container, 0x10, count) || count <= 0 ||
                count > static_cast<std::int32_t>(P::FrameCapacity) || !D::At(value.container, 0x988, frameArrayCount) || frameArrayCount != count ||
                !D::At(value.container, 0x98c, frameCapacity) || frameCapacity != static_cast<std::int32_t>(P::FrameCapacity))
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
            NativeAnimationHierarchyMemory hierarchyMemory({image}, owner);
            if ((result = hierarchyMemory.Read(input.hierarchy, value.hierarchy)) != ClockResult::Ok ||
                (result = hierarchyMemory.ReadSequences(input.hierarchy, value.sequences)) != ClockResult::Ok)
                return result;
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
                    !M::Disjoint(value.sequences - 8, value.sequenceAllocationBytes, input.association, 32))
                    return ClockResult::BindingChanged;
                value.sequenceRows.resize(value.hierarchy.sequenceCount);
                if (!value.sequenceRows.empty() && !M::Copy(value.sequences, value.sequenceRows.data(), value.sequenceRows.size() * S::RowBytes))
                    return ClockResult::ReadFailure;
                for (const auto& sequence : value.sequenceRows)
                    if (!(sequence[S::FlagsOffset] & 0x10) && std::find(sequence.begin(), sequence.begin() + 64, 0) == sequence.begin() + 64)
                        return ClockResult::BindingChanged;
            }
            if (!M::Copy(input.association, value.row.data(), value.row.size()) || Field<std::uintptr_t>(value.row, 0) != image + P::StaticVtableRva ||
                Field<std::uintptr_t>(value.row, 0x18) != input.hierarchy || !M::Disjoint(input.association, 32, value.container, P::ContainerBytes) ||
                !M::Disjoint(input.association, 32, render, 0x310))
                return ClockResult::BindingChanged;
            // Readable actor/type bytes are insufficient once an item enters
            // retirement or its slot is reused. Extend the existing current
            // membership/serial graph before dereferences and preserve those
            // additions through both call bookends; no new GC lease is issued.
            for (std::size_t index = 0; index != value.world.count; ++index)
                if (!D::Add(image, value.world.members[index].classAddress, value.world))
                    return ClockResult::BindingChanged;
            if ((result = NativePedPoolReader({image}, owner).Recheck(input.ped)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, value.world) || !Profile(image))
                return ClockResult::BindingChanged;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = std::move(value);
            return ClockResult::Ok;
        }
        bool DefinedRowPreserved(const Row& before, const Row& after) noexcept
        {
            return std::memcmp(before.data(), after.data(), 8) == 0 && std::memcmp(before.data() + 0x0a, after.data() + 0x0a, 6) == 0 &&
                   std::memcmp(before.data() + 0x18, after.data() + 0x18, 8) == 0;
        }
        ClockResult ObserveNodes(const Arguments& input, const Value& state, std::vector<std::uintptr_t>& output)
        {
            const auto nodes = Field<std::uintptr_t>(state.row, 0x10);
            const auto bytes = static_cast<std::size_t>(state.frameCount) * sizeof(std::uintptr_t);
            if (Field<std::uint16_t>(state.row, 8) != state.frameCount || !nodes || nodes % 8 || !M::PrivateWritable(nodes, bytes) ||
                !M::Disjoint(nodes, bytes, input.association, 32) || !M::Disjoint(nodes, bytes, input.clump, 0x310) ||
                !M::Disjoint(nodes, bytes, state.container, P::ContainerBytes) ||
                (state.sequences && !M::Disjoint(nodes, bytes, state.sequences - 8, state.sequenceAllocationBytes)) ||
                (!state.map.rows.empty() && !M::Disjoint(nodes, bytes, state.map.rowsAddress, state.map.rows.size() * sizeof(BoneRow))))
                return ClockResult::BindingChanged;
            std::vector<std::uintptr_t> values(state.frameCount);
            if (!M::Copy(nodes, values.data(), bytes))
                return ClockResult::ReadFailure;
            for (const auto sequence : values)
            {
                if (!sequence)
                    continue;
                if (!state.sequences || sequence < state.sequences || (sequence - state.sequences) % S::RowBytes)
                    return ClockResult::BindingChanged;
                const auto index = (sequence - state.sequences) / S::RowBytes;
                if (index >= state.sequenceRows.size() || Field<std::int16_t>(state.sequenceRows[static_cast<std::size_t>(index)], S::FrameCountOffset) <= 0)
                    return ClockResult::BindingChanged;
            }
            output = std::move(values);
            return ClockResult::Ok;
        }
        bool ObserveReceipt(const Arguments& input, Receipt& receipt, Row* observed = nullptr) noexcept
        {
            Row row{};
            receipt.rowObserved = false;
            if (M::Copy(input.association, row.data(), row.size()))
            {
                receipt.observedNodes = Field<std::uintptr_t>(row, 0x10);
                receipt.observedCount = Field<std::uint16_t>(row, 8);
                receipt.rowObserved = true;
                if (observed)
                    *observed = row;
            }
            return receipt.rowObserved;
        }
#if !defined(MTA_DE_STATIC_ASSOCIATION_INITIALIZE_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t association, std::uintptr_t clump, std::uintptr_t hierarchy,
                               std::uint32_t& exception) noexcept
        {
            __try
            {
                function(association, clump, hierarchy);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    ClockResult Initialize(std::uintptr_t image, const Arguments& source, CheckOwner owner, RecheckArguments recheck, void* context, Receipt* output,
                           Invoke invoke) noexcept
    {
        const Arguments input = source;
        Receipt         receipt;
        receipt.entryRva = P::EntryRva;
        receipt.nativePed = input.ped.nativePed;
        receipt.clump = input.clump;
        receipt.association = input.association;
        receipt.hierarchy = input.hierarchy;
        Row        acknowledgedRow{};
        const auto finish = [&](ClockResult result)
        {
            if (receipt.entered)
            {
                Row        observed{};
                const bool copied = ObserveReceipt(input, receipt, &observed);
                if (result == ClockResult::Ok && receipt.verified && (!copied || observed != acknowledgedRow))
                {
                    receipt.verified = false;
                    result = copied ? ClockResult::BindingChanged : ClockResult::ReadFailure;
                }
            }
            if (output)
                *output = receipt;
            return result;
        };
        if (!owner || !recheck)
            return finish(ClockResult::NotInitialized);
        if (!input.clump || !input.association || !input.hierarchy || input.association % 8 || input.hierarchy % 8)
            return finish(ClockResult::InvalidArgument);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        try
        {
            Value before, confirmed;
            auto  result = recheck(input, false, context);
            if (result != ClockResult::Ok || (result = Capture(image, input, owner, before)) != ClockResult::Ok)
                return finish(result);
            receipt.container = before.container;
            receipt.heap = before.heap.current;
            receipt.frameCount = before.frameCount;
            receipt.beforeNodes = Field<std::uintptr_t>(before.row, 0x10);
            // FEB3F0 does not define WORD+8. Its null-clump Init publishes H
            // without allocating nodes; only the real pointer0 phase is fresh.
            if (receipt.beforeNodes)
                return finish(ClockResult::InvalidArgument);
            if ((result = recheck(input, false, context)) != ClockResult::Ok || (result = Capture(image, input, owner, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!Same(before, confirmed) || before.row != confirmed.row)
                return finish(ClockResult::BindingChanged);
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_STATIC_ASSOCIATION_INITIALIZE_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_STATIC_ASSOCIATION_INITIALIZE_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &NativeCall;
#endif
            receipt.entered = true;
            result = entry(reinterpret_cast<NativeFunction>(image + P::EntryRva), input.association, input.clump, input.hierarchy, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.returned = true;
            ObserveReceipt(input, receipt);
            if ((result = Capture(image, input, owner, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!Same(before, confirmed) || !DefinedRowPreserved(before.row, confirmed.row))
                return finish(ClockResult::BindingChanged);
            std::vector<std::uintptr_t> nodes, finalNodes;
            if ((result = ObserveNodes(input, confirmed, nodes)) != ClockResult::Ok || (result = recheck(input, true, context)) != ClockResult::Ok ||
                (result = Capture(image, input, owner, before)) != ClockResult::Ok)
                return finish(result);
            if (!Same(confirmed, before) || before.row != confirmed.row || (result = ObserveNodes(input, before, finalNodes)) != ClockResult::Ok)
                return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
            if (nodes != finalNodes)
                return finish(ClockResult::BindingChanged);
            if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return finish(result);
            acknowledgedRow = before.row;
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
