/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Causal ownership of genuinely constructed unpublished associations
 ****************************************************************************/
#include "NativeAnimationAssociationStorage.h"
#include "NativeAnimationAssociationClumpCalls.h"
#include "NativeAnimationAssociationClumpOrigin.h"
#include "NativeAnimationAssociationClumpStorageState.h"
#include "NativePrivateAnimationHierarchyController.h"
#include "NativeAnimationHierarchyMemory.h"
#include "NativeAnimationAssociationGroupCopy.h"
#include "NativeAnimationAssociationGroupCopyProfile.h"
#include "NativeAnimationAssociationOuterStorageCalls.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativeAnimationAssociationProfile.h"
#include "NativeAnimationAssociationProgressDetail.h"
#include "NativeAnimationHierarchyReadProfile.h"
#include "NativeAnimationHierarchyOrigin.h"
#include "NativeAnimationBlockRefMemory.h"
#include "NativeStreamingCompletionContext.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace mta::game_de
{
    namespace AnimationAssociationStorageDetail
    {
        // This implementation is also included by the focused owner fixture.
        // Qualify the layout and owner types so unrelated aggregate-TU aliases
        // cannot silently select a different native profile or callback type.
        namespace M = NativeModelReferenceMap;
        namespace Image = NativeImageAccess;
        namespace Context = NativeStreamingCompletionContext;
        namespace Origin = NativeAnimationHierarchyOrigin;
        using namespace NativeAnimationAssociationCalls;
        using Bytes = std::array<unsigned char, NativeAnimationAssociationProfile::ReceiverBytes>;
        template <class T, class A>
        T Field(const A& bytes, std::size_t offset) noexcept
        {
            T value;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        template <class T, class A>
        void Put(A& bytes, std::size_t offset, T value) noexcept
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }

        // These retained guarded delegates are also the fixture interception
        // seam. The owner state machine never lives in an injected callee.
        ClockResult InvokeConstructor(ConstructorFunction function, std::uintptr_t receiver, std::uintptr_t input, std::uintptr_t& returned,
                                      std::uint32_t& exception) noexcept
        {
            __try
            {
                returned = function(receiver, input);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult InvokeProgress(ProgressFunction function, std::uintptr_t receiver, float absoluteTime, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(receiver, absoluteTime);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult InvokeDestructor(DestructorFunction function, std::uintptr_t receiver, std::uint32_t flags, std::uintptr_t& returned,
                                     std::uint32_t& exception) noexcept
        {
            __try
            {
                returned = function(receiver, flags);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult InvokeNodeFree(NativeCustomAnimSequenceStorageCalls::FreeFunction function, std::uintptr_t heap, std::uintptr_t original,
                                   std::uint32_t& exception) noexcept
        {
            __try
            {
                function(heap, original);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult ExchangeScalar(std::uintptr_t address, std::size_t width, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed,
                                   bool& accepted, std::uint32_t& exception) noexcept
        {
            __try
            {
                if (width == 2)
                    observed = static_cast<std::uint16_t>(
                        InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), static_cast<SHORT>(target), static_cast<SHORT>(expected)));
                else if (width == 4)
                    observed = static_cast<std::uint32_t>(
                        InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(target), static_cast<LONG>(expected)));
                else
                    return ClockResult::InvalidArgument;
                accepted = observed == expected;
                return accepted ? ClockResult::Ok : ClockResult::BindingChanged;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        // A pointer clear is the actual source operation. Keep this guarded
        // delegate available to owner fixtures instead of duplicating stores.
        ClockResult ExchangeNode(std::uintptr_t address, std::uintptr_t expected, std::uintptr_t target, std::uintptr_t& observed, bool& accepted,
                                 std::uint32_t& exception) noexcept
        {
            accepted = false;
            __try
            {
                observed = reinterpret_cast<std::uintptr_t>(InterlockedCompareExchangePointer(
                    reinterpret_cast<void* volatile*>(address), reinterpret_cast<void*>(target), reinterpret_cast<void*>(expected)));
                accepted = observed == expected;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        bool QualifyAssociationProfile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : NativeAnimationAssociationProfile::Code)
            {
                std::array<unsigned char, NativeAnimationAssociationProfile::MaximumPinBytes> bytes{};
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            std::uintptr_t destructor = 0;
            return NativeModelReferenceHeap::ReadOnlyImage(image, image + NativeAnimationAssociationProfile::VtableRva, sizeof(destructor)) &&
                   Image::ReadImage(image, image + NativeAnimationAssociationProfile::VtableRva, &destructor, sizeof(destructor)) &&
                   destructor == image + NativeAnimationAssociationProfile::DestructorRva;
        }
        bool QualifyAssociationLengthProfile(std::uintptr_t image, std::uintptr_t hierarchyRva) noexcept
        {
            namespace LengthLayout = NativeAnimationHierarchyReadProfile;
            if (!Origin::Sections({image}, hierarchyRva, 0, LengthLayout::Code.data(), LengthLayout::Code.size()))
                return false;
            for (const auto& pin : LengthLayout::Code)
            {
                std::array<unsigned char, LengthLayout::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        struct Hierarchy
        {
            std::uintptr_t sequences = 0;
            std::int32_t   block = -1;
            bool           operator==(const Hierarchy& b) const noexcept { return sequences == b.sequences && block == b.block; }
        };
        ClockResult ObserveHierarchy(Origin::Binding binding, std::uintptr_t address, Hierarchy& output) noexcept
        {
            Hierarchy value;
            if (!Image::ReadImage(binding.image, address + 8, &value.sequences, sizeof(value.sequences), true) ||
                !Image::ReadImage(binding.image, address + 0x14, &value.block, sizeof(value.block), true))
                return ClockResult::ReadFailure;
            output = value;
            return ClockResult::Ok;
        }
        struct Bank
        {
            std::uintptr_t                 hierarchy = 0;
            Origin::Location               location{};
            Origin::Observation<Hierarchy> observation{};
            NativeAnimationBlockIdentity   identity{};
            std::uint16_t                  refs = 0;
            std::uint8_t                   loaded = 0;
        };
        bool SameBank(const Bank& a, const Bank& b) noexcept
        {
            return a.hierarchy == b.hierarchy && Origin::Same(a.observation.origin, b.observation.origin) && a.observation.payload == b.observation.payload &&
                   a.identity.index == b.identity.index && a.identity.name == b.identity.name && a.loaded == b.loaded;
        }
        ClockResult CaptureBank(std::uintptr_t image, NativeAnimationAssociationCalls::CheckOwner owner, std::uintptr_t hierarchy, Bank& output) noexcept
        {
            Bank next;
            next.hierarchy = hierarchy;
            auto result = Origin::Locate({image}, hierarchy, next.location);
            if (result != ClockResult::Ok)
                return result;
            if ((result = Origin::Capture<Hierarchy>({image}, owner, next.location.ordinal, next.location.rva, &ObserveHierarchy, next.observation)) !=
                ClockResult::Ok)
                return result;
            if (next.observation.payload.block < 0 || next.observation.payload.block >= 180)
                return ClockResult::BindingChanged;
            NativeAnimationBlockRefMemory refs({image}, owner);
            if ((result = refs.ReadCounterIdentity(static_cast<std::uint32_t>(next.observation.payload.block), next.identity)) != ClockResult::Ok ||
                (result = refs.ReadCounter(next.identity, next.refs)) != ClockResult::Ok)
                return result;
            const auto address = image + NativeAnimationBlockProfile::BankRva + next.identity.index * NativeAnimationBlockProfile::RowBytes;
            if (!Image::ReadImage(image, address + 0x10, &next.loaded, sizeof(next.loaded), true))
                return ClockResult::ReadFailure;
            if (!next.loaded)
                return ClockResult::NotInitialized;
            NativeAnimationBlockIdentity after;
            if ((result = refs.ReadCounterIdentity(next.identity.index, after)) != ClockResult::Ok)
                return result;
            if (after.name != next.identity.name)
                return ClockResult::BindingChanged;
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool SequenceSpan(const Bank& bank, std::uint64_t capacity) noexcept
        {
            const auto    sequences = bank.observation.payload.sequences;
            std::uint64_t current = 0;
            return sequences >= 8 && sequences % 8 == 0 && capacity && capacity <= ((std::numeric_limits<std::size_t>::max)() - 8) / 80 &&
                   M::PrivateWritable(sequences - 8, 8 + static_cast<std::size_t>(capacity) * 80) && M::Copy(sequences - 8, &current, 8) && current == capacity;
        }
        struct Prepared
        {
            std::array<unsigned char, 32>                  source{};
            NativeAnimationStaticAssociationReferenceToken reference{};
            std::vector<std::uintptr_t>                    map;
            Bank                                           bank{};
            std::uint64_t                                  sequenceCapacity = 0;
            std::uint16_t                                  count = 0;
        };
        ClockResult InvokeGroupCopy(GroupCopyFunction function, std::int32_t group, std::uint32_t animation, std::uintptr_t& returned,
                                    std::uint32_t& exception) noexcept
        {
            __try
            {
                returned = function(group, animation);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        bool QualifyCopyProfile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : NativeAnimationAssociationGroupCopyProfile::Code)
            {
                std::array<unsigned char, NativeAnimationAssociationGroupCopyProfile::MaximumPinBytes> bytes{};
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            // FF1120 and its node producer call the same current/authority
            // allocator routes as the existing outer owner. Qualify those
            // exact bodies before entry; array80 construction is not consumed.
            namespace HeapProfile = NativeCustomAnimSequenceStorageProfile;
            const HeapProfile::Pin allocatorPins[] = {{HeapProfile::AuthorityMallocRva, HeapProfile::AuthorityMalloc, sizeof(HeapProfile::AuthorityMalloc)},
                                                      {HeapProfile::AuthorityFreeRva, HeapProfile::AuthorityFree, sizeof(HeapProfile::AuthorityFree)},
                                                      {HeapProfile::ProxyMallocRva, HeapProfile::ProxyMalloc, sizeof(HeapProfile::ProxyMalloc)},
                                                      {HeapProfile::ProxyFreeRva, HeapProfile::ProxyFree, sizeof(HeapProfile::ProxyFree)}};
            for (const auto& pin : allocatorPins)
            {
                std::array<unsigned char, HeapProfile::MaximumPinBytes> bytes{};
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return QualifyAssociationProfile(image) && NativeModelReferenceHeap::QualifyProfile(image);
        }
        ClockResult CaptureCopyHeap(std::uintptr_t image, NativeAnimationAssociationCalls::CheckOwner owner, NativeModelReferenceHeap::Value& heap) noexcept
        {
            auto result = Context::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!QualifyCopyProfile(image))
                return ClockResult::BindingChanged;
            if ((result = NativeModelReferenceHeap::Capture(image, heap)) != ClockResult::Ok)
                return result;
            namespace P = NativeCustomAnimSequenceStorageProfile;
            const auto slots = [&](std::uintptr_t table, std::uintptr_t allocateRva, std::uintptr_t releaseRva) noexcept
            {
                std::uintptr_t allocate = 0, release = 0;
                return NativeModelReferenceHeap::ReadOnlyImage(image, table, 0x38) && Image::ReadImage(image, table + 0x10, &allocate, 8) &&
                       Image::ReadImage(image, table + 0x30, &release, 8) && allocate == image + allocateRva && release == image + releaseRva;
            };
            if (!slots(heap.authorityVtable, P::AuthorityMallocRva, P::AuthorityFreeRva) ||
                (heap.current != heap.authority && !slots(heap.currentVtable, P::ProxyMallocRva, P::ProxyFreeRva)))
                return ClockResult::BindingChanged;
            return Context::Owner(image, owner);
        }
        bool SameCopySource(const NativeAnimationManagerMemory::AssociationGroupCopySource& a,
                            const NativeAnimationManagerMemory::AssociationGroupCopySource& b) noexcept
        {
            return a.group.index == b.group.index && a.group.locator == b.group.locator && a.selected.groupIndex == b.selected.groupIndex &&
                   a.selected.index == b.selected.index && a.selected.locator == b.selected.locator && a.groups == b.groups && a.children == b.children &&
                   a.groupCapacity == b.groupCapacity && a.childCapacity == b.childCapacity && a.groupCount == b.groupCount && a.rawIdOffset == b.rawIdOffset &&
                   a.parent32 == b.parent32;
        }
        // This bookend uses the prepared physical row and fixed-size map
        // chunks. No post-entry all-groups resolver or CPU allocation occurs.
        ClockResult RecheckCopySource(std::uintptr_t image, NativeAnimationAssociationCalls::CheckOwner owner,
                                      const NativeAnimationManagerMemory::AssociationGroupCopySource& expected, std::uint32_t animation,
                                      const Prepared& prepared, const std::array<unsigned char, 40>& hierarchy, std::uint16_t refs) noexcept
        {
            NativeAnimationManagerMemory                             manager({image}, owner);
            NativeAnimationManagerMemory::AssociationGroupCopySource current;
            auto                                                     result = manager.ReadAssociationGroupCopySource(expected.group, animation, current);
            if (result != ClockResult::Ok)
                return result;
            if (!SameCopySource(expected, current))
                return ClockResult::BindingChanged;
            std::array<unsigned char, 32> row{};
            std::array<unsigned char, 40> hierarchyAfter{};
            if (!M::Copy(expected.selected.locator, row.data(), row.size()) ||
                !Image::ReadImage(image, prepared.bank.hierarchy, hierarchyAfter.data(), hierarchyAfter.size(), true))
                return ClockResult::ReadFailure;
            if (row != prepared.source || hierarchyAfter != hierarchy)
                return ClockResult::BindingChanged;
            const auto                     map = Field<std::uintptr_t>(row, 0x10);
            std::array<std::uintptr_t, 32> chunk{};
            for (std::size_t first = 0; first < prepared.map.size(); first += chunk.size())
            {
                const auto count = (std::min)(chunk.size(), prepared.map.size() - first);
                if (!M::Copy(map + first * 8, chunk.data(), count * 8))
                    return ClockResult::ReadFailure;
                if (!std::equal(chunk.begin(), chunk.begin() + count, prepared.map.begin() + first))
                    return ClockResult::BindingChanged;
            }
            Bank bank;
            if ((result = CaptureBank(image, owner, prepared.bank.hierarchy, bank)) != ClockResult::Ok)
                return result;
            if (!SameBank(prepared.bank, bank) || bank.refs != refs || (prepared.sequenceCapacity && !SequenceSpan(bank, prepared.sequenceCapacity)))
                return ClockResult::BindingChanged;
            return Context::Owner(image, owner);
        }
        ClockResult PrepareCopySource(std::uintptr_t image, NativeAnimationAssociationCalls::CheckOwner owner,
                                      const NativeAnimationAssociationGroupReferenceToken& group, std::uint32_t animation,
                                      NativeAnimationManagerMemory::AssociationGroupCopySource& source, Prepared& prepared,
                                      std::array<unsigned char, 40>& hierarchy) noexcept
        {
            NativeAnimationManagerMemory manager({image}, owner);
            auto                         result = manager.ReadAssociationGroupCopySource(group, animation, source);
            if (result != ClockResult::Ok)
                return result;
            prepared.reference = source.selected;
            if (!M::Copy(prepared.reference.locator, prepared.source.data(), prepared.source.size()))
                return ClockResult::ReadFailure;
            prepared.count = Field<std::uint16_t>(prepared.source, 8);
            if (prepared.count > 0x7fff || (Field<std::uint16_t>(prepared.source, 0xe) & 0x4000))
                return ClockResult::InvalidArgument;
            if ((result = CaptureBank(image, owner, Field<std::uintptr_t>(prepared.source, 0x18), prepared.bank)) != ClockResult::Ok)
                return result;
            if (prepared.bank.refs == 0xffff)
                return ClockResult::OwnerConflict;
            if (!Image::ReadImage(image, prepared.bank.hierarchy, hierarchy.data(), hierarchy.size(), true))
                return ClockResult::ReadFailure;
            if (!AnimationAssociationProgressDetail::IsWarmNoEffect(hierarchy))
                return ClockResult::OwnerConflict;
            if (prepared.count)
            {
                const auto map = Field<std::uintptr_t>(prepared.source, 0x10);
                const auto size = static_cast<std::size_t>(prepared.count) * 8;
                if (!map || map % 8 || !M::PrivateWritable(map, size))
                    return ClockResult::BindingChanged;
                try
                {
                    prepared.map.resize(prepared.count);
                }
                catch (...)
                {
                    return ClockResult::WriteFailure;
                }
                if (!M::Copy(map, prepared.map.data(), size))
                    return ClockResult::ReadFailure;
                bool needsSequences = false;
                for (auto value : prepared.map)
                    needsSequences |= value != 0;
                if (needsSequences)
                {
                    const auto sequences = prepared.bank.observation.payload.sequences;
                    if (sequences < 8 || sequences % 8 || !M::PrivateWritable(sequences - 8, 8) || !M::Copy(sequences - 8, &prepared.sequenceCapacity, 8) ||
                        !SequenceSpan(prepared.bank, prepared.sequenceCapacity))
                        return ClockResult::BindingChanged;
                    for (auto value : prepared.map)
                        if (value && (value < sequences || (value - sequences) % 80 || (value - sequences) / 80 >= prepared.sequenceCapacity))
                            return ClockResult::BindingChanged;
                }
            }
            return RecheckCopySource(image, owner, source, animation, prepared, hierarchy, prepared.bank.refs);
        }
        ClockResult CapturePrepared(std::uintptr_t image, NativeAnimationAssociationCalls::CheckOwner owner, std::uintptr_t raw, Prepared& output) noexcept
        {
            NativeAnimationManagerMemory manager({image}, owner);
            Prepared                     next;
            auto                         result = manager.ResolveStaticAssociationReference(raw, next.reference);
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationStaticAssociationValues values;
            if ((result = manager.ReadStaticAssociation(next.reference, values)) != ClockResult::Ok)
                return result;
            if (!M::Copy(raw, next.source.data(), next.source.size()))
                return ClockResult::ReadFailure;
            next.count = Field<std::uint16_t>(next.source, 8);
            if (next.count > 0x7fff || (Field<std::uint16_t>(next.source, 0xe) & 0x4000))
                return ClockResult::InvalidArgument;
            if (next.count != values.numBlendNodes || Field<std::uintptr_t>(next.source, 0x18) != values.hierarchyAddress ||
                Field<std::int16_t>(next.source, 0xa) != values.animId || Field<std::int16_t>(next.source, 0xc) != values.animGroup ||
                Field<std::int16_t>(next.source, 0xe) != values.flags)
                return ClockResult::BindingChanged;
            if ((result = CaptureBank(image, owner, values.hierarchyAddress, next.bank)) != ClockResult::Ok)
                return result;
            if (next.bank.refs == 0xffff)
                return ClockResult::OwnerConflict;
            // Zero does not access the source map or a sequence payload. The
            // genuine helper still allocates its 64-byte aligned header area.
            if (next.count)
            {
                const auto map = Field<std::uintptr_t>(next.source, 0x10);
                const auto size = static_cast<std::size_t>(next.count) * sizeof(std::uintptr_t);
                if (!map || map % alignof(std::uintptr_t) || !M::PrivateWritable(map, size))
                    return ClockResult::BindingChanged;
                try
                {
                    next.map.resize(next.count);
                }
                catch (...)
                {
                    return ClockResult::WriteFailure;
                }
                if (!M::Copy(map, next.map.data(), size))
                    return ClockResult::ReadFailure;
                bool needsSequences = false;
                for (const auto value : next.map)
                    needsSequences |= value != 0;
                if (needsSequences)
                {
                    const auto sequences = next.bank.observation.payload.sequences;
                    if (sequences < 8 || sequences % 8 || !M::PrivateWritable(sequences - 8, 8) || !M::Copy(sequences - 8, &next.sequenceCapacity, 8) ||
                        next.sequenceCapacity > ((std::numeric_limits<std::size_t>::max)() - 8) / 80 || !next.sequenceCapacity ||
                        !M::PrivateWritable(sequences - 8, 8 + static_cast<std::size_t>(next.sequenceCapacity) * 80))
                        return ClockResult::BindingChanged;
                    for (const auto value : next.map)
                        if (value && (value < sequences || (value - sequences) % 80 || (value - sequences) / 80 >= next.sequenceCapacity))
                            return ClockResult::BindingChanged;
                }
                std::vector<std::uintptr_t> after;
                try
                {
                    after.resize(next.count);
                }
                catch (...)
                {
                    return ClockResult::WriteFailure;
                }
                if (!M::Copy(map, after.data(), size))
                    return ClockResult::ReadFailure;
                if (after != next.map)
                    return ClockResult::BindingChanged;
            }
            std::array<unsigned char, 32> after{};
            if (!M::Copy(raw, after.data(), after.size()))
                return ClockResult::ReadFailure;
            if (after != next.source)
                return ClockResult::BindingChanged;
            NativeAnimationStaticAssociationReferenceToken confirmed;
            if ((result = manager.ResolveStaticAssociationReference(raw, confirmed)) != ClockResult::Ok)
                return result;
            if (confirmed.groupIndex != next.reference.groupIndex || confirmed.index != next.reference.index || confirmed.locator != raw)
                return ClockResult::BindingChanged;
            Bank bankAfter;
            if ((result = CaptureBank(image, owner, values.hierarchyAddress, bankAfter)) != ClockResult::Ok)
                return result;
            if (!SameBank(next.bank, bankAfter) || next.bank.refs != bankAfter.refs)
                return ClockResult::BindingChanged;
            output = std::move(next);
            return ClockResult::Ok;
        }
    }

    struct NativeAnimationAssociationStorage::Data
    {
        NativeCustomAnimSequenceStorage             outer;
        std::uint64_t                               generation = 0;
        std::uintptr_t                              receiver = 0, nodes = 0, nodeRaw = 0;
        std::size_t                                 request = 0;
        AnimationAssociationStorageDetail::Prepared source;
        std::vector<unsigned char>                  suppressed;
        // Cache definedness follows exact returned native effects. Borrowed
        // frame entries record only accepted quaternion writes, never ownership.
        std::vector<AnimationAssociationProgressDetail::Cache>        cache;
        std::vector<AnimationAssociationProgressDetail::FrameJournal> frameJournal;
        bool                                                          progressUnknown = false;
        std::unique_ptr<AnimationAssociationProgressDetail::Plan>     pendingProgress;
        bool knownConstructed = false, constructorEntered = false, destructorEntered = false, retirementVerified = false;
        // These are causal command receipts, not facts inferred from a stale
        // pointer or a readable allocation header after native release.
        bool           nodeFreeEntered = false, nodeFreeReturned = false, nodeFreeVerified = false;
        bool           nodeClearEntered = false, nodePointerCleared = false, nodeClearVerified = false;
        std::uintptr_t nodeFreeEntryRva = 0;
        std::unique_ptr<AnimationAssociationClumpStorageDetail::State<AnimationAssociationStorageDetail::Bank>> clump;
        // A no-ref deleting-dtor ACK consumes outer/nodes independently of
        // later CPU hierarchy-hold release. Cleanup must never replay it.
        bool clumpRetiredAck = false;
    };
    NativeAnimationAssociationStorage::NativeAnimationAssociationStorage() noexcept = default;
    NativeAnimationAssociationStorage::~NativeAnimationAssociationStorage() = default;

    struct NativeAnimationAssociationStorageAccess
    {
        using S = NativeAnimationAssociationStorage;
        using State = S::State;
        using Data = S::Data;
        using C = NativeAnimationAssociationCalls::CheckOwner;
        using Receipt = NativeAnimationAssociationCalls::Receipt;
        using Scalar = NativeAnimationAssociationCalls::Scalar;
        static bool ClumpPrivate(const Data& d) noexcept { return d.clump && d.clump->acceptedSource && d.clump->privateHierarchy; }
        static bool ClumpNoRef(const Data& d) noexcept { return d.clump && d.clump->acceptedSource && !d.clump->copyReference; }
        // These are members of the existing privileged storage access class,
        // not new raw-storage adoption entry points or independent owners.
#include "NativeAnimationAssociationClumpSnapshot.inl"
#include "NativeAnimationAssociationClumpStorage.inl"
        static void Fill(const Data& d, Receipt& r) noexcept
        {
            r.receiver = d.receiver;
            r.nodes = d.nodes;
            r.nodeRaw = d.nodeRaw;
            r.nodeRequestBytes = d.request;
            r.nodeCount = d.source.count;
            r.hierarchy = d.source.bank.hierarchy;
            r.constructed = d.knownConstructed;
            r.progressUnknown = d.progressUnknown;
            r.nodeFreeEntryRva = d.nodeFreeEntryRva;
            r.nodeFreeEntered = d.nodeFreeEntered;
            r.nodesReleased = d.nodeFreeReturned;
            r.nodeFreeVerified = d.nodeFreeVerified;
            r.nodePointerClearEntered = d.nodeClearEntered;
            r.nodePointerCleared = d.nodePointerCleared;
            r.nodePointerClearVerified = d.nodeClearVerified;
            r.refVerified = d.knownConstructed && (!d.destructorEntered || d.retirementVerified);
        }
        static ClockResult Pin(const S& s, ClockResult result) noexcept
        {
            auto state = s.GetState();
            while (state != State::Released && state != State::Empty && state != State::PublicationPinned &&
                   !s.state.compare_exchange_weak(state, State::PublicationPinned))
            {
            }
            return result;
        }
        static ClockResult Qualify(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, NativeModelReferenceHeap::Value& heap) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            if (!s.data || !generation || s.data->generation != generation)
                return ClockResult::OwnerConflict;
            if (!QualifyAssociationProfile(image))
                return ClockResult::BindingChanged;
            auto result = NativeAnimationAssociationOuterStorageCalls::Qualify(image, s.data->outer, generation, owner, heap);
            if (result != ClockResult::Ok)
                return result;
            return Context::Owner(image, owner);
        }
        static bool        MetadataReadable(State state) noexcept { return state == State::LiveUnpublished || state == State::NodesReleased; }
        static ClockResult Snapshot(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, AnimationAssociationStorageDetail::Bytes& bytes,
                                    NativeModelReferenceHeap::Value& heap, const AnimationAssociationProgressDetail::Plan* transition = nullptr) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            if (s.data && ClumpPrivate(*s.data))
                return ClumpSnapshot(image, s, generation, owner, bytes, heap, transition);
            const auto state = s.GetState();
            if (state == State::PublicationPinned)
                return ClockResult::OwnerConflict;
            if (!s.data || !s.data->knownConstructed || s.data->destructorEntered ||
                (state != State::LiveUnpublished && state != State::FaultDebt && state != State::NodesReleased))
                return ClockResult::NotInitialized;
            const auto& d = *s.data;
            if ((d.nodeFreeEntered && !d.nodeFreeReturned) || (d.nodeClearEntered && !d.nodePointerCleared))
                return ClockResult::OwnerConflict;
            auto result = Qualify(image, s, generation, owner, heap);
            if (result != ClockResult::Ok)
                return result;
            if (!M::Copy(d.receiver, bytes.data(), bytes.size()))
                return ClockResult::ReadFailure;
            if (Field<std::uintptr_t>(bytes, 0) != image + NativeAnimationAssociationProfile::VtableRva || Field<std::uintptr_t>(bytes, 8) ||
                Field<std::uintptr_t>(bytes, 0x10) || Field<std::uint32_t>(bytes, 0x48) ||
                Field<std::uintptr_t>(bytes, 0x20) != (d.nodePointerCleared ? std::uintptr_t{0} : d.nodes) ||
                Field<std::uintptr_t>(bytes, 0x28) != d.source.bank.hierarchy || Field<std::uint16_t>(bytes, 0x18) != d.source.count ||
                static_cast<bool>(Field<std::uint16_t>(bytes, 0x46) & 0x4000) != !ClumpNoRef(d))
                return Pin(s, ClockResult::BindingChanged);
            Bank bank;
            if ((result = CaptureBank(image, owner, d.source.bank.hierarchy, bank)) != ClockResult::Ok)
                return result;
            if (!SameBank(d.source.bank, bank) || (!ClumpNoRef(d) && !bank.refs))
                return Pin(s, ClockResult::BindingChanged);
            if (d.source.sequenceCapacity && !SequenceSpan(bank, d.source.sequenceCapacity))
                return Pin(s, ClockResult::BindingChanged);
            if (d.nodeFreeReturned)
            {
                if (transition)
                    return ClockResult::NotInitialized;
                // The primary node-only release intentionally leaves pointer
                // and count stale. Causal return, never page readability, is
                // the reason receiver-only reads skip all freed-node access.
                for (const auto& frame : d.frameJournal)
                    if (!AnimationAssociationProgressDetail::CopyJournal(frame))
                        return Pin(s, ClockResult::BindingChanged);
            }
            else
            {
                if (!M::PrivateWritable(d.nodeRaw, d.request) || d.nodes < d.nodeRaw + 8 || d.nodes % 64 || d.nodes - d.nodeRaw > d.request ||
                    static_cast<std::size_t>(d.source.count) * NativeAnimationAssociationProfile::NodeBytes > d.request - (d.nodes - d.nodeRaw))
                    return Pin(s, ClockResult::BindingChanged);
                std::uintptr_t original = 0;
                if (!M::Copy(d.nodes - 8, &original, sizeof(original)))
                    return ClockResult::ReadFailure;
                if (original != d.nodeRaw)
                    return Pin(s, ClockResult::BindingChanged);
                if (d.suppressed.size() != d.source.count || d.cache.size() != d.source.count || (d.progressUnknown && !transition) ||
                    (transition && transition->afterNodes.size() != d.source.count))
                    return Pin(s, ClockResult::BindingChanged);
                for (std::size_t index = 0; index != d.source.count; ++index)
                {
                    std::array<unsigned char, 40> node{};
                    if (!M::Copy(d.nodes + index * 40, node.data(), node.size()))
                        return ClockResult::ReadFailure;
                    if (Field<std::uintptr_t>(node, 0x10) != (d.suppressed[index] ? std::uintptr_t{0} : d.source.map[index]) ||
                        Field<std::uintptr_t>(node, 0x18) != d.receiver || Field<std::uint32_t>(node, 0x20) != 0xffffffff)
                        return Pin(s, ClockResult::BindingChanged);
                    if (transition)
                    {
                        if (node != transition->afterNodes[index])
                            return Pin(s, ClockResult::BindingChanged);
                    }
                    else
                        for (std::size_t word = 0; word != 4; ++word)
                            if ((d.cache[index].defined & (1u << word)) && Field<std::uint32_t>(node, word * 4) != d.cache[index].words[word])
                                return Pin(s, ClockResult::BindingChanged);
                }
                if (transition)
                {
                    if (!AnimationAssociationProgressDetail::Verify(*transition, true))
                        return Pin(s, ClockResult::BindingChanged);
                }
                else
                    for (const auto& frame : d.frameJournal)
                        if (!AnimationAssociationProgressDetail::CopyJournal(frame))
                            return Pin(s, ClockResult::BindingChanged);
            }
            Bank bankAfter;
            if ((result = CaptureBank(image, owner, d.source.bank.hierarchy, bankAfter)) != ClockResult::Ok)
                return result;
            if (!SameBank(bank, bankAfter) || (d.source.sequenceCapacity && !SequenceSpan(bankAfter, d.source.sequenceCapacity)))
                return Pin(s, ClockResult::BindingChanged);
            NativeModelReferenceHeap::Value after;
            if ((result = Qualify(image, s, generation, owner, after)) != ClockResult::Ok)
                return result;
            if (!NativeModelReferenceHeap::Same(heap, after))
                return ClockResult::BindingChanged;
            return ClockResult::Ok;
        }
        static ClockResult CopyFromGroup(std::uintptr_t image, S& s, const NativeAnimationAssociationReferenceToken& issued,
                                         const NativeAnimationAssociationGroupReferenceToken& group, std::uint32_t animation, C owner, std::uintptr_t& output,
                                         NativeAnimationAssociationCalls::GroupCopyReceipt* receipt,
                                         NativeAnimationAssociationCalls::InvokeGroupCopy   invoke) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            NativeAnimationAssociationCalls::GroupCopyReceipt r;
            r.issued = issued;
            r.group = group;
            r.animationId = animation;
            auto finish = [&](ClockResult result)
            {
                if (s.data)
                    Fill(*s.data, r.resources);
                r.resources.pinned = s.GetState() == State::PublicationPinned;
                if (receipt)
                    *receipt = r;
                return result;
            };
            if (!issued.ownerGeneration || !issued.serial || group.ownerGeneration != issued.ownerGeneration)
                return finish(ClockResult::OwnerConflict);
            if (s.GetState() != State::Empty || s.data)
                return finish(ClockResult::OwnerConflict);
            if (!Context::TryEnter())
                return finish(ClockResult::Busy);
            Context::Scope                  gate;
            NativeModelReferenceHeap::Value heap;
            auto                            result = CaptureCopyHeap(image, owner, heap);
            if (result != ClockResult::Ok)
                return finish(result);
            NativeAnimationManagerMemory::AssociationGroupCopySource source;
            Prepared                                                 prepared;
            std::array<unsigned char, 40>                            hierarchy{};
            if ((result = PrepareCopySource(image, owner, group, animation, source, prepared, hierarchy)) != ClockResult::Ok)
                return finish(result);
            // All durable state and acceptance buffers precede FF1120. The
            // exact original function owns allocation and construction.
            try
            {
                s.data = std::make_unique<Data>();
                s.data->suppressed.assign(prepared.count, 0);
                s.data->cache.assign(prepared.count, AnimationAssociationProgressDetail::Cache{});
            }
            catch (...)
            {
                s.data.reset();
                return finish(ClockResult::WriteFailure);
            }
            auto& d = *s.data;
            d.generation = issued.ownerGeneration;
            d.source = std::move(prepared);
            r.sourcePrepared = true;
            r.hierarchyWarm = true;
            r.rawIdOffset = source.rawIdOffset;
            r.selected = source.selected;
            r.resources.heap = heap.current;
            if ((result = RecheckCopySource(image, owner, source, animation, d.source, hierarchy, d.source.bank.refs)) != ClockResult::Ok)
            {
                s.data.reset();
                return finish(result);
            }
            NativeModelReferenceHeap::Value before;
            if ((result = CaptureCopyHeap(image, owner, before)) != ClockResult::Ok)
            {
                s.data.reset();
                return finish(result);
            }
            if (!NativeModelReferenceHeap::Same(heap, before))
            {
                s.data.reset();
                return finish(ClockResult::BindingChanged);
            }
#if defined(MTA_DE_ANIMATION_ASSOCIATION_GROUP_COPY_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_ANIMATION_ASSOCIATION_GROUP_COPY_INVOKE;
#else
            const auto entry = invoke ? invoke : &AnimationAssociationStorageDetail::InvokeGroupCopy;
#endif
            d.constructorEntered = true;
            s.state.store(State::CopyEntered);
            r.entered = r.resources.entered = true;
            std::uintptr_t returned = 0;
            result = entry(reinterpret_cast<NativeAnimationAssociationCalls::GroupCopyFunction>(image + r.entryRva), static_cast<std::int32_t>(group.index),
                           animation, returned, r.resources.exceptionCode);
            r.returned = r.resources.returned = result == ClockResult::Ok;
            s.state.store(State::FaultDebt);
            if (!r.returned)
                return finish(result);
            r.returnedInterface = returned;
            r.returnedNull = returned == 0;
            if (!returned)
            {
                // Acknowledged null owns no outer, nodes or reference even
                // when the later owner/heap acknowledgement is unavailable.
                s.state.store(State::Released);
                NativeModelReferenceHeap::Value after;
                if ((result = CaptureCopyHeap(image, owner, after)) != ClockResult::Ok)
                    return finish(result);
                if (!NativeModelReferenceHeap::Same(heap, after))
                    return finish(ClockResult::BindingChanged);
                if ((result = RecheckCopySource(image, owner, source, animation, d.source, hierarchy, d.source.bank.refs)) != ClockResult::Ok)
                    return finish(result);
                r.verified = r.resources.verified = true;
                output = 0;
                return finish(ClockResult::Ok);
            }
            d.receiver = returned;
            NativeAnimationAssociationCopyReturnTicket ticket(d.outer, issued, heap, returned);
            if (!d.outer.BindReturnedDynamicCopy(ticket))
                return finish(ClockResult::BindingChanged);
            r.resources.allocated = r.resources.accepted = true;
            NativeModelReferenceHeap::Value after;
            if ((result = Qualify(image, s, issued.ownerGeneration, owner, after)) != ClockResult::Ok)
                return finish(result);
            if (!NativeModelReferenceHeap::Same(heap, after))
                return finish(ClockResult::BindingChanged);
            Bytes bytes{};
            if (!M::Copy(returned, bytes.data(), bytes.size()))
                return finish(ClockResult::ReadFailure);
            // Verify only fields written by FEB970/FEBC10. Callback payloads
            // and padding without native stores remain opaque, not zeroed.
            if (Field<std::uintptr_t>(bytes, 0) != image + NativeAnimationAssociationProfile::VtableRva || Field<std::uintptr_t>(bytes, 8) ||
                Field<std::uintptr_t>(bytes, 0x10) || Field<std::uint16_t>(bytes, 0x18) != d.source.count ||
                Field<std::uint16_t>(bytes, 0x1a) != Field<std::uint16_t>(d.source.source, 0xc) ||
                Field<std::uintptr_t>(bytes, 0x28) != d.source.bank.hierarchy || Field<std::uint64_t>(bytes, 0x30) != 0x3f800000 ||
                Field<std::uint32_t>(bytes, 0x38) || Field<std::uint64_t>(bytes, 0x3c) != 0x3f800000 ||
                Field<std::uint16_t>(bytes, 0x44) != Field<std::uint16_t>(d.source.source, 0xa) ||
                Field<std::uint16_t>(bytes, 0x46) != (Field<std::uint16_t>(d.source.source, 0xe) | 0x4000) || Field<std::uint32_t>(bytes, 0x48) || bytes[0x60])
                return finish(ClockResult::BindingChanged);
            d.nodes = Field<std::uintptr_t>(bytes, 0x20);
            const auto amount = static_cast<std::uint64_t>(d.source.count) * 40;
            d.request = static_cast<std::size_t>(((amount - 1) & ~std::uint64_t{63}) + 128);
            if (d.nodes < 8 || d.nodes % 64 || !M::Copy(d.nodes - 8, &d.nodeRaw, 8) || !d.nodeRaw ||
                d.nodeRaw > (std::numeric_limits<std::uintptr_t>::max)() - 64 || ((d.nodeRaw + 64) & ~std::uintptr_t{63}) != d.nodes ||
                !M::PrivateWritable(d.nodeRaw, d.request) || !M::Disjoint(d.nodeRaw, d.request, returned, 104) ||
                !M::Disjoint(returned, 104, d.source.reference.locator, 32) || !M::Disjoint(d.nodeRaw, d.request, d.source.reference.locator, 32))
                return finish(ClockResult::BindingChanged);
            const auto map = Field<std::uintptr_t>(d.source.source, 0x10);
            if ((d.source.count && (!M::Disjoint(d.nodeRaw, d.request, map, static_cast<std::size_t>(d.source.count) * 8) ||
                                    !M::Disjoint(returned, 104, map, static_cast<std::size_t>(d.source.count) * 8))) ||
                (d.source.sequenceCapacity &&
                 (!M::Disjoint(d.nodeRaw, d.request, d.source.bank.observation.payload.sequences - 8,
                               8 + static_cast<std::size_t>(d.source.sequenceCapacity) * 80) ||
                  !M::Disjoint(returned, 104, d.source.bank.observation.payload.sequences - 8, 8 + static_cast<std::size_t>(d.source.sequenceCapacity) * 80))))
                return finish(ClockResult::BindingChanged);
            if ((result = RecheckCopySource(image, owner, source, animation, d.source, hierarchy, static_cast<std::uint16_t>(d.source.bank.refs + 1))) !=
                ClockResult::Ok)
                return finish(result);
            d.knownConstructed = true;
            s.state.store(State::LiveUnpublished);
            Bytes                           confirmed{};
            NativeModelReferenceHeap::Value finalHeap;
            if ((result = Snapshot(image, s, issued.ownerGeneration, owner, confirmed, finalHeap)) != ClockResult::Ok)
            {
                if (s.GetState() != State::PublicationPinned)
                    s.state.store(State::FaultDebt);
                return finish(result);
            }
            if (confirmed != bytes || !NativeModelReferenceHeap::Same(heap, finalHeap))
            {
                s.state.store(State::FaultDebt);
                return finish(ClockResult::BindingChanged);
            }
            if ((result = RecheckCopySource(image, owner, source, animation, d.source, hierarchy, static_cast<std::uint16_t>(d.source.bank.refs + 1))) !=
                    ClockResult::Ok ||
                (result = CaptureCopyHeap(image, owner, finalHeap)) != ClockResult::Ok)
            {
                s.state.store(State::FaultDebt);
                return finish(result);
            }
            if (!NativeModelReferenceHeap::Same(heap, finalHeap))
            {
                s.state.store(State::FaultDebt);
                return finish(ClockResult::BindingChanged);
            }
            r.verified = r.resources.verified = true;
            output = returned;
            return finish(ClockResult::Ok);
        }
        static ClockResult Create(std::uintptr_t image, S& s, std::uint64_t generation, C owner, Receipt* output,
                                  NativeCustomAnimSequenceStorageCalls::InvokeMalloc invoke) noexcept
        {
            Receipt r;
            auto    finish = [&](ClockResult result)
            {
                if (s.data)
                    Fill(*s.data, r);
                if (output)
                    *output = r;
                return result;
            };
            if (s.GetState() != State::Empty || s.data)
                return finish(ClockResult::OwnerConflict);
            try
            {
                s.data = std::make_unique<Data>();
            }
            catch (...)
            {
                return finish(ClockResult::WriteFailure);
            }
            s.data->generation = generation;
            NativeCustomAnimSequenceStorageCalls::Receipt native;
            const auto result = NativeAnimationAssociationOuterStorageCalls::Allocate(image, s.data->outer, generation, owner, &native, invoke);
            r.entered = native.entered;
            r.returned = native.returned;
            r.accepted = native.allocated;
            r.allocated = native.allocated;
            r.verified = native.verified;
            r.heap = native.heap;
            r.exceptionCode = native.exceptionCode;
            s.data->receiver = native.base;
            if (s.data->outer.CanDiscard())
            {
                s.data.reset();
                return finish(result);
            }
            s.state.store(result == ClockResult::Ok ? State::Unconstructed : State::FaultDebt);
            return finish(result);
        }
        static ClockResult Construct(std::uintptr_t image, S& s, std::uint64_t generation, std::uintptr_t raw, C owner, std::uintptr_t& output,
                                     Receipt* receipt, NativeAnimationAssociationCalls::InvokeConstructor invoke) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            Receipt r;
            auto    finish = [&](ClockResult result)
            {
                if (s.data)
                    Fill(*s.data, r);
                r.pinned = s.GetState() == State::PublicationPinned;
                if (receipt)
                    *receipt = r;
                return result;
            };
            if (s.GetState() != State::Unconstructed)
                return finish(ClockResult::OwnerConflict);
            if (!Context::TryEnter())
                return finish(ClockResult::Busy);
            Context::Scope gate;
            if (s.GetState() != State::Unconstructed)
                return finish(ClockResult::OwnerConflict);
            NativeModelReferenceHeap::Value heap;
            auto                            result = Qualify(image, s, generation, owner, heap);
            if (result != ClockResult::Ok)
                return finish(result);
            Prepared prepared;
            if ((result = CapturePrepared(image, owner, raw, prepared)) != ClockResult::Ok)
                return finish(result);
            auto& d = *s.data;
            // Reserve acceptance metadata before the constructor can acquire
            // native resources. This never changes the original map journal.
            try
            {
                d.suppressed.assign(prepared.count, 0);
                d.cache.assign(prepared.count, AnimationAssociationProgressDetail::Cache{});
            }
            catch (...)
            {
                return finish(ClockResult::WriteFailure);
            }
            d.source = std::move(prepared);
            r.heap = heap.current;
            alignas(16) std::array<unsigned char, 32> input{};
            Put(input, 8, d.source.count);
            Put(input, 0xa, Field<std::uint16_t>(d.source.source, 0xa));
            Put(input, 0xc, Field<std::uint16_t>(d.source.source, 0xc));
            Put(input, 0xe, Field<std::uint16_t>(d.source.source, 0xe));
            Put(input, 0x10, d.source.count ? reinterpret_cast<std::uintptr_t>(d.source.map.data()) : std::uintptr_t{0});
            Put(input, 0x18, d.source.bank.hierarchy);
            // All native-retained dependencies have already been copied or
            // qualified. A returned constructor is the causal node origin,
            // never a readable raw-pointer header found on an arbitrary page.
            if ((result = Qualify(image, s, generation, owner, heap)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_ANIMATION_ASSOCIATION_CONSTRUCTOR_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_ANIMATION_ASSOCIATION_CONSTRUCTOR_INVOKE;
#else
            const auto entry = invoke ? invoke : &AnimationAssociationStorageDetail::InvokeConstructor;
#endif
            d.constructorEntered = true;
            s.state.store(State::ConstructEntered);
            r.entered = true;
            std::uintptr_t returned = 0;
            result = entry(reinterpret_cast<NativeAnimationAssociationCalls::ConstructorFunction>(image + NativeAnimationAssociationProfile::ConstructorRva),
                           d.receiver, reinterpret_cast<std::uintptr_t>(input.data()), returned, r.exceptionCode);
            r.returned = result == ClockResult::Ok;
            r.accepted = r.returned && returned == d.receiver;
            s.state.store(State::FaultDebt);
            if (result != ClockResult::Ok)
                return finish(result);
            if (!r.accepted)
                return finish(ClockResult::BindingChanged);
            NativeModelReferenceHeap::Value causalHeap;
            if ((result = Qualify(image, s, generation, owner, causalHeap)) != ClockResult::Ok)
                return finish(result);
            if (!NativeModelReferenceHeap::Same(heap, causalHeap))
                return finish(ClockResult::BindingChanged);
            Bytes bytes{};
            if (!M::Copy(d.receiver, bytes.data(), bytes.size()))
                return finish(ClockResult::ReadFailure);
            d.nodes = Field<std::uintptr_t>(bytes, 0x20);
            const auto amount = static_cast<std::uint64_t>(d.source.count) * 40;
            d.request = static_cast<std::size_t>(((amount - 1) & ~std::uint64_t{63}) + 128);
            if (d.nodes < 8 || d.nodes % 64 || !M::Copy(d.nodes - 8, &d.nodeRaw, sizeof(d.nodeRaw)) || !d.nodeRaw ||
                d.nodeRaw > (std::numeric_limits<std::uintptr_t>::max)() - 64 || ((d.nodeRaw + 64) & ~std::uintptr_t{63}) != d.nodes ||
                !M::PrivateWritable(d.nodeRaw, d.request) || !M::Disjoint(d.nodeRaw, d.request, d.receiver, NativeAnimationAssociationProfile::ReceiverBytes))
                return finish(ClockResult::BindingChanged);
            const auto originalMap = Field<std::uintptr_t>(d.source.source, 0x10);
            if (!M::Disjoint(d.nodeRaw, d.request, d.source.reference.locator, 32) ||
                (d.source.count && !M::Disjoint(d.nodeRaw, d.request, originalMap, static_cast<std::size_t>(d.source.count) * 8)) ||
                (d.source.sequenceCapacity && !M::Disjoint(d.nodeRaw, d.request, d.source.bank.observation.payload.sequences - 8,
                                                           8 + static_cast<std::size_t>(d.source.sequenceCapacity) * 80)))
                return finish(ClockResult::BindingChanged);
            Bank after;
            if ((result = CaptureBank(image, owner, d.source.bank.hierarchy, after)) != ClockResult::Ok)
                return finish(result);
            if (!SameBank(d.source.bank, after) || after.refs != static_cast<std::uint16_t>(d.source.bank.refs + 1))
                return finish(ClockResult::BindingChanged);
            d.knownConstructed = true;
            r.refVerified = true;
            s.state.store(State::LiveUnpublished);
            Bytes                           confirmed{};
            NativeModelReferenceHeap::Value current;
            if ((result = Snapshot(image, s, generation, owner, confirmed, current)) != ClockResult::Ok)
            {
                if (s.GetState() != State::PublicationPinned)
                    s.state.store(State::FaultDebt);
                return finish(result);
            }
            if (bytes != confirmed || !NativeModelReferenceHeap::Same(heap, current))
            {
                s.state.store(State::FaultDebt);
                return finish(ClockResult::BindingChanged);
            }
            r.verified = true;
            output = d.receiver;
            return finish(ClockResult::Ok);
        }
        static bool MatchesIssuedInterface(const S& s, std::uint64_t generation, std::uintptr_t candidate) noexcept
        {
            const auto state = s.GetState();
            return candidate && generation && s.data && s.data->generation == generation && s.data->receiver == candidate &&
                   s.data->outer.IssuedTag() == candidate && !s.data->destructorEntered &&
                   (state == State::Unconstructed || state == State::LiveUnpublished || state == State::NodesReleased);
        }
        static ClockResult ReadInterface(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, std::uintptr_t& output) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            if (s.GetState() != State::Unconstructed)
            {
                // Preserve every existing constructed/node-release snapshot
                // guard instead of making the raw factory a weaker reader.
                NativeAnimationAssociationValues next;
                const auto                       result = Read(image, s, generation, owner, next);
                if (result == ClockResult::Ok)
                    output = next.interfaceAddress;
                return result;
            }
            if (!Context::TryEnter())
                return ClockResult::Busy;
            Context::Scope gate;
            if (!s.data || s.GetState() != State::Unconstructed || s.data->constructorEntered || s.data->knownConstructed || s.data->destructorEntered ||
                !MatchesIssuedInterface(s, generation, s.data->receiver))
                return ClockResult::OwnerConflict;
            const auto                      receiver = s.data->receiver;
            NativeModelReferenceHeap::Value first, last;
            auto                            result = NativeAnimationAssociationOuterStorageCalls::Qualify(image, s.data->outer, generation, owner, first);
            if (result != ClockResult::Ok)
                return result;
            // The real allocator receipt owns these104 bytes, but none of
            // their vtable/H/node fields exist yet. Bookend only that outer
            // identity and its original heap, never an invented constructor.
            if ((result = NativeAnimationAssociationOuterStorageCalls::Qualify(image, s.data->outer, generation, owner, last)) != ClockResult::Ok)
                return result;
            if (!NativeModelReferenceHeap::Same(first, last) || s.GetState() != State::Unconstructed || s.data->constructorEntered ||
                s.data->knownConstructed || s.data->destructorEntered || !MatchesIssuedInterface(s, generation, receiver))
                return ClockResult::BindingChanged;
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = receiver;
            return ClockResult::Ok;
        }
        static ClockResult Read(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, NativeAnimationAssociationValues& output) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            if (!MetadataReadable(s.GetState()))
                return ClockResult::NotInitialized;
            if (!Context::TryEnter())
                return ClockResult::Busy;
            Context::Scope gate;
            if (!MetadataReadable(s.GetState()))
                return ClockResult::NotInitialized;
            Bytes                           before{}, after{};
            NativeModelReferenceHeap::Value heap, current;
            auto                            result = Snapshot(image, s, generation, owner, before, heap);
            if (result != ClockResult::Ok)
                return result;
            if ((result = Snapshot(image, s, generation, owner, after, current)) != ClockResult::Ok)
                return result;
            if (before != after || !NativeModelReferenceHeap::Same(heap, current))
                return ClockResult::BindingChanged;
            NativeAnimationAssociationValues next;
            next.interfaceAddress = s.data->receiver;
            next.animId = Field<std::int16_t>(before, 0x44);
            next.animGroup = Field<std::int16_t>(before, 0x1a);
            next.flags = Field<std::int16_t>(before, 0x46);
            next.blendAmountBits = Field<std::uint32_t>(before, 0x30);
            next.speedBits = Field<std::uint32_t>(before, 0x3c);
            // Source progress is the absolute current time, not a ratio.
            next.currentTimeBits = Field<std::uint32_t>(before, 0x38);
            output = next;
            return ClockResult::Ok;
        }
        static ClockResult ReadLength(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, std::uint32_t& output) noexcept
        {
            namespace A = AnimationAssociationStorageDetail;
            if (!MetadataReadable(s.GetState()))
                return ClockResult::NotInitialized;
            if (!A::Context::TryEnter())
                return ClockResult::Busy;
            A::Context::Scope gate;
            if (!MetadataReadable(s.GetState()))
                return ClockResult::NotInitialized;
            if (s.data && ClumpPrivate(*s.data))
                return ClumpReadLength(image, s, generation, owner, output);
            A::Bytes                        before{}, middle{}, after{};
            NativeModelReferenceHeap::Value heap, middleHeap, afterHeap;
            auto                            result = Snapshot(image, s, generation, owner, before, heap);
            if (result != ClockResult::Ok)
                return result;
            const auto hierarchy = s.data->source.bank.hierarchy, rva = s.data->source.bank.location.rva;
            if (!A::QualifyAssociationLengthProfile(image, rva))
                return ClockResult::BindingChanged;
            std::uint32_t first = 0, second = 0;
            if (!NativeImageAccess::ReadImage(image, hierarchy + 0x18, &first, sizeof(first), true))
                return ClockResult::ReadFailure;
            if ((result = Snapshot(image, s, generation, owner, middle, middleHeap)) != ClockResult::Ok)
                return result;
            if (before != middle || !NativeModelReferenceHeap::Same(heap, middleHeap) || !A::QualifyAssociationLengthProfile(image, rva))
                return ClockResult::BindingChanged;
            if (!NativeImageAccess::ReadImage(image, hierarchy + 0x18, &second, sizeof(second), true))
                return ClockResult::ReadFailure;
            if ((result = Snapshot(image, s, generation, owner, after, afterHeap)) != ClockResult::Ok)
                return result;
            if (before != after || first != second || !NativeModelReferenceHeap::Same(heap, afterHeap) || !A::QualifyAssociationLengthProfile(image, rva))
                return ClockResult::BindingChanged;
            if ((result = A::Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            // Zero, signed zero and NaN payloads are legitimate field bits.
            // Guard failure never becomes a fabricated numeric duration.
            output = second;
            return ClockResult::Ok;
        }
        static ClockResult Set(std::uintptr_t image, S& s, std::uint64_t generation, Scalar scalar, std::uint32_t bits, C owner, Receipt* output,
                               NativeAnimationAssociationCalls::Exchange invoke) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            Receipt r;
            auto    finish = [&](ClockResult result)
            {
                if (s.data)
                    Fill(*s.data, r);
                r.pinned = s.GetState() == State::PublicationPinned;
                if (output)
                    *output = r;
                return result;
            };
            // Source node-only free leaves this owned receiver usable. Scalar
            // stores need current receiver authority, not live node payload.
            if (!MetadataReadable(s.GetState()))
                return finish(ClockResult::NotInitialized);
            std::size_t offset = 0, width = 2;
            switch (scalar)
            {
                case Scalar::BlendAmount:
                    offset = 0x30;
                    width = 4;
                    break;
                case Scalar::CurrentSpeed:
                    offset = 0x3c;
                    width = 4;
                    break;
                case Scalar::AnimID:
                    offset = 0x44;
                    break;
                case Scalar::AnimGroup:
                    offset = 0x1a;
                    break;
                case Scalar::Flags:
                    offset = 0x46;
                    break;
                default:
                    return finish(ClockResult::InvalidArgument);
            }
            if (width == 2)
                bits &= 0xffff;
            if (!Context::TryEnter())
                return finish(ClockResult::Busy);
            Context::Scope                  gate;
            Bytes                           before{}, after{};
            NativeModelReferenceHeap::Value heap, current;
            auto                            result = Snapshot(image, s, generation, owner, before, heap);
            if (result != ClockResult::Ok)
                return finish(result);
            const auto previous = width == 2 ? static_cast<std::uint32_t>(Field<std::uint16_t>(before, offset)) : Field<std::uint32_t>(before, offset);
            if (scalar == Scalar::Flags && ((bits ^ previous) & 0x4000))
                return finish(ClockResult::OwnerConflict);
#if defined(MTA_DE_ANIMATION_ASSOCIATION_EXCHANGE)
            const auto entry = invoke ? invoke : &MTA_DE_ANIMATION_ASSOCIATION_EXCHANGE;
#else
            const auto entry = invoke ? invoke : &ExchangeScalar;
#endif
            if ((result = Qualify(image, s, generation, owner, current)) != ClockResult::Ok)
                return finish(result);
            r.entered = true;
            std::uint32_t observed = 0;
            result = entry(s.data->receiver + offset, width, previous, bits, observed, r.accepted, r.exceptionCode);
            r.returned = result == ClockResult::Ok;
            if (result != ClockResult::Ok || !r.accepted)
            {
                s.state.store(State::FaultDebt);
                return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
            }
            if ((result = Snapshot(image, s, generation, owner, after, current)) != ClockResult::Ok)
            {
                if (s.GetState() != State::PublicationPinned)
                    s.state.store(State::FaultDebt);
                return finish(result);
            }
            if (width == 2)
                Put(before, offset, static_cast<std::uint16_t>(bits));
            else
                Put(before, offset, bits);
            if (before != after || !NativeModelReferenceHeap::Same(heap, current))
            {
                s.state.store(State::FaultDebt);
                return finish(ClockResult::BindingChanged);
            }
            r.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult SetCurrentProgress(std::uintptr_t image, S& s, std::uint64_t generation, std::uint32_t bits, C owner, Receipt* output,
                                              NativeAnimationAssociationCalls::InvokeProgress invoke) noexcept
        {
            namespace Progress = AnimationAssociationProgressDetail;
            using namespace AnimationAssociationStorageDetail;
            Receipt r;
            r.progressBits = bits;
            auto finish = [&](ClockResult result)
            {
                if (s.data)
                    Fill(*s.data, r);
                r.pinned = s.GetState() == State::PublicationPinned;
                if (output)
                    *output = r;
                return result;
            };
            if (s.GetState() != State::LiveUnpublished)
                return finish(ClockResult::NotInitialized);
            if (!Context::TryEnter())
                return finish(ClockResult::Busy);
            Context::Scope                  gate;
            Bytes                           before{}, confirmed{}, after{};
            NativeModelReferenceHeap::Value heap, current;
            auto                            result = Snapshot(image, s, generation, owner, before, heap);
            if (result != ClockResult::Ok)
                return finish(result);
            Progress::Math math, mathAfter;
            if (!Progress::Qualify(image, math))
                return finish(ClockResult::BindingChanged);
            auto&                           d = *s.data;
            std::unique_ptr<Progress::Plan> prepared;
            try
            {
                prepared = std::make_unique<Progress::Plan>();
            }
            catch (...)
            {
                return finish(ClockResult::WriteFailure);
            }
            auto& plan = *prepared;
            try
            {
                result = Progress::Build(image, d.source.bank.hierarchy, d.source.bank.observation.payload.sequences, d.source.sequenceCapacity, d.receiver,
                                         d.nodeRaw, d.request, d.source.count, bits, d.frameJournal, math, plan);
            }
            catch (...)
            {
                return finish(ClockResult::WriteFailure);
            }
            if (result != ClockResult::Ok)
                return finish(result);
            r.absoluteTimeBits = plan.absoluteBits;
            r.preparedFrames = plan.preparedFrames;
            r.modifiedFrames = plan.modifiedFrames;
            if (plan.receiverBefore != before)
                return finish(ClockResult::BindingChanged);
            if ((result = Snapshot(image, s, generation, owner, confirmed, current)) != ClockResult::Ok)
                return finish(result);
            if (before != confirmed || !NativeModelReferenceHeap::Same(heap, current) || !Progress::Verify(plan, false))
                return finish(ClockResult::BindingChanged);
            // The known cache fields were checked by Snapshot; also bookend the
            // previously undefined raw cache/padding bytes copied into this plan.
            for (std::size_t index = 0; index < plan.beforeNodes.size(); ++index)
            {
                Progress::Node node{};
                if (!M::Copy(d.nodes + index * 40, node.data(), node.size()))
                    return finish(ClockResult::ReadFailure);
                if (node != plan.beforeNodes[index])
                    return finish(ClockResult::BindingChanged);
            }
            if (!Progress::Qualify(image, mathAfter) || !(math == mathAfter))
                return finish(ClockResult::BindingChanged);
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_ANIMATION_ASSOCIATION_PROGRESS_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_ANIMATION_ASSOCIATION_PROGRESS_INVOKE;
#else
            const auto entry = invoke ? invoke : &AnimationAssociationStorageDetail::InvokeProgress;
#endif
            // Reserve the unknown state before the first38 store. An exception
            // may already have modified bank frame quaternions and owned caches;
            // neither visible bytes nor a later callback may invent their ACK.
            // The owner, not this stack frame, retains prospective originals,
            // predicted effects and frame/node origins if execution is unknown.
            d.pendingProgress = std::move(prepared);
            d.progressUnknown = true;
            s.state.store(State::FaultDebt);
            r.entered = true;
            result = entry(reinterpret_cast<NativeAnimationAssociationCalls::ProgressFunction>(image + NativeAnimationAssociationProgressProfile::SetTimeRva),
                           d.receiver, Progress::Float(plan.absoluteBits), r.exceptionCode);
            r.returned = result == ClockResult::Ok;
            if (result != ClockResult::Ok)
                return finish(result);
            // Snapshot accepts a fully specified alternate transition, not a
            // waiver of node checks: every40B node and every borrowed payload
            // byte must match the precomputed result before any journal changes.
            if ((result = Snapshot(image, s, generation, owner, after, current, &plan)) != ClockResult::Ok)
                return finish(result);
            auto expected = before;
            Put(expected, 0x38, plan.finalTimeBits);
            if (after != expected || !NativeModelReferenceHeap::Same(heap, current) || !Progress::Qualify(image, mathAfter) || !(math == mathAfter))
                return finish(ClockResult::BindingChanged);
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            // These vectors and their capacity were reserved before entry. Only
            // actually written cache words become defined; ignored native bytes
            // keep their prior state. No post-entry allocation can lose effects.
            for (std::size_t index = 0; index < d.cache.size(); ++index)
            {
                auto& cache = d.cache[index];
                for (std::size_t word = 0; word != 4; ++word)
                    if (plan.written[index] & (1u << word))
                        cache.words[word] = Progress::At<std::uint32_t>(plan.afterNodes[index], word * 4);
                cache.defined |= plan.written[index];
            }
            d.frameJournal.swap(plan.journal);
            d.pendingProgress.reset();
            d.progressUnknown = false;
            s.state.store(State::LiveUnpublished);
            r.accepted = r.verified = r.progressed = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult FreeNodeArray(std::uintptr_t image, S& s, std::uint64_t generation, C owner, Receipt* output,
                                         NativeCustomAnimSequenceStorageCalls::InvokeFree invoke) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            Receipt    r;
            const auto finish = [&](ClockResult result)
            {
                if (s.data)
                    Fill(*s.data, r);
                r.pinned = s.GetState() == State::PublicationPinned;
                if (output)
                    *output = r;
                return result;
            };
            if (s.GetState() != State::LiveUnpublished)
                return finish(ClockResult::NotInitialized);
            if (!Context::TryEnter())
                return finish(ClockResult::Busy);
            Context::Scope gate;
            if (s.GetState() != State::LiveUnpublished || !s.data || s.data->nodeFreeEntered)
                return finish(ClockResult::OwnerConflict);
            Bytes                           before{}, confirmed{}, after{};
            NativeModelReferenceHeap::Value heap, current;
            auto                            result = Snapshot(image, s, generation, owner, before, heap);
            if (result != ClockResult::Ok)
                return finish(result);
            Bank bank;
            if ((!ClumpPrivate(*s.data) && (result = CaptureBank(image, owner, s.data->source.bank.hierarchy, bank)) != ClockResult::Ok) ||
                (result = Snapshot(image, s, generation, owner, confirmed, current)) != ClockResult::Ok)
                return finish(result);
            if (before != confirmed || !NativeModelReferenceHeap::Same(heap, current))
                return finish(ClockResult::BindingChanged);
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            auto& d = *s.data;
            d.nodeFreeEntryRva = heap.current == heap.authority ? NativeCustomAnimSequenceStorageProfile::AuthorityFreeRva
                                                                : NativeCustomAnimSequenceStorageProfile::ProxyFreeRva;
#if defined(MTA_DE_ANIMATION_ASSOCIATION_NODE_FREE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_ANIMATION_ASSOCIATION_NODE_FREE_INVOKE;
#else
            const auto entry = invoke ? invoke : &AnimationAssociationStorageDetail::InvokeNodeFree;
#endif
            d.nodeFreeEntered = true;
            s.state.store(State::NodeFreeEntered);
            r.entered = true;
            r.heap = heap.current;
            // Even count zero has the genuine helper's nonnull 64-byte raw
            // allocation. Free only that causally acquired original base.
            result = entry(reinterpret_cast<NativeCustomAnimSequenceStorageCalls::FreeFunction>(image + d.nodeFreeEntryRva), heap.current, d.nodeRaw,
                           r.exceptionCode);
            if (result != ClockResult::Ok)
            {
                s.state.store(State::FaultDebt);
                return finish(result);
            }
            // Publish consumption before any fallible postflight, preserving
            // all 104 physical bytes and never reading freed nodes/header.
            d.nodeFreeReturned = true;
            s.state.store(State::NodesReleased);
            r.returned = r.accepted = true;
            if ((result = Snapshot(image, s, generation, owner, after, current)) != ClockResult::Ok)
                return finish(result);
            Bank bankAfter;
            if (!ClumpPrivate(d) && (result = CaptureBank(image, owner, d.source.bank.hierarchy, bankAfter)) != ClockResult::Ok)
                return finish(result);
            if (before != after || !NativeModelReferenceHeap::Same(heap, current) ||
                (!ClumpPrivate(d) && (!SameBank(bank, bankAfter) || bank.refs != bankAfter.refs)))
                return finish(Pin(s, ClockResult::BindingChanged));
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            d.nodeFreeVerified = true;
            r.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult Retire(std::uintptr_t image, S& s, std::uint64_t generation, C owner, Receipt* output,
                                  NativeAnimationAssociationCalls::InvokeDestructor invoke, NativeCustomAnimSequenceStorageCalls::InvokeFree free,
                                  NativeAnimationAssociationCalls::ExchangeNode clear) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            Receipt r;
            auto    finish = [&](ClockResult result)
            {
                if (s.data)
                    Fill(*s.data, r);
                r.pinned = s.GetState() == State::PublicationPinned;
                if (output)
                    *output = r;
                return result;
            };
            if (!s.data || s.data->generation != generation)
                return finish(ClockResult::OwnerConflict);
            auto& d = *s.data;
            if (d.clumpRetiredAck && s.GetState() != State::Released)
            {
                // Only CPU dependent release may resume after the original
                // returned. Neither the freed receiver nor its nodes is read.
                const auto result = ClumpReleaseDependencies(image, s, generation, owner);
                if (result == ClockResult::Ok)
                    s.state.store(State::Released);
                r.retired = true;
                r.verified = d.retirementVerified;
                return finish(result);
            }
            if (s.GetState() == State::Released)
                return finish(ClockResult::OwnerConflict);
            if (!d.constructorEntered)
            {
                // A source custom prefix is already an entered operation even
                // when the later original Init was never called. Preserve its
                // exact receiver/default/H journals instead of treating this
                // partially changed allocation as untouched raw storage.
                if (d.clump && d.clump->pending && (d.clump->pending->receipt.hadEffects || d.clump->pending->receipt.enteredUnknown))
                    return finish(ClockResult::OwnerConflict);
                NativeCustomAnimSequenceStorageCalls::Receipt native;
                const auto result = NativeAnimationAssociationOuterStorageCalls::FreeUnconstructed(image, d.outer, generation, owner, &native, free);
                r.entered = native.entered;
                r.returned = native.returned;
                r.accepted = native.released;
                r.retired = native.released;
                r.verified = native.verified;
                r.exceptionCode = native.exceptionCode;
                s.state.store(d.outer.CanDiscard() ? State::Released : State::FaultDebt);
                if (native.released && d.clump)
                {
                    d.clumpRetiredAck = true;
                    const auto released = ClumpReleaseDependencies(image, s, generation, owner);
                    if (released != ClockResult::Ok)
                    {
                        s.state.store(State::FaultDebt);
                        return finish(released);
                    }
                }
                return finish(result);
            }
            if (!d.knownConstructed || d.destructorEntered || (d.nodeFreeEntered && !d.nodeFreeReturned) || (d.nodeClearEntered && !d.nodePointerCleared))
                return finish(ClockResult::OwnerConflict);
            if (!Context::TryEnter())
                return finish(ClockResult::Busy);
            Context::Scope                  gate;
            Bytes                           before{};
            NativeModelReferenceHeap::Value heap;
            auto                            result = Snapshot(image, s, generation, owner, before, heap);
            if (result != ClockResult::Ok)
                return finish(result);
            if (d.nodeFreeReturned)
            {
                if (!d.nodePointerCleared)
                {
                    NativeModelReferenceHeap::Value current;
                    if ((result = Qualify(image, s, generation, owner, current)) != ClockResult::Ok)
                        return finish(result);
                    if (!NativeModelReferenceHeap::Same(heap, current))
                        return finish(ClockResult::BindingChanged);
#if defined(MTA_DE_ANIMATION_ASSOCIATION_NODE_POINTER_CLEAR)
                    const auto pointerClear = clear ? clear : &MTA_DE_ANIMATION_ASSOCIATION_NODE_POINTER_CLEAR;
#else
                    const auto pointerClear = clear ? clear : &AnimationAssociationStorageDetail::ExchangeNode;
#endif
                    // This is a cleanup effect with its own command receipt,
                    // not the public FreeNodes operation. Without this known
                    // clear the genuine destructor would double-free nodes.
                    d.nodeClearEntered = true;
                    s.state.store(State::NodeClearEntered);
                    r.entered = true;
                    std::uintptr_t observed = 0;
                    bool           accepted = false;
                    result = pointerClear(d.receiver + 0x20, d.nodes, 0, observed, accepted, r.exceptionCode);
                    r.returned = result == ClockResult::Ok;
                    d.nodePointerCleared = accepted && observed == d.nodes;
                    if (!d.nodePointerCleared)
                    {
                        s.state.store(State::FaultDebt);
                        return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
                    }
                    s.state.store(State::NodesReleased);
                    r.accepted = true;
                    if (result != ClockResult::Ok)
                        return finish(result);
                    Put(before, 0x20, std::uintptr_t{0});
                }
                // A known accepted clear may resume through read-only
                // acknowledgement after owner loss; never issue its CAS again.
                Bytes                           confirmed{};
                NativeModelReferenceHeap::Value current;
                if ((result = Snapshot(image, s, generation, owner, confirmed, current)) != ClockResult::Ok)
                    return finish(result);
                if (before != confirmed || !NativeModelReferenceHeap::Same(heap, current))
                    return finish(Pin(s, ClockResult::BindingChanged));
                if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                    return finish(result);
                d.nodeClearVerified = true;
            }
            Bank bank;
            if (!ClumpPrivate(d) && (result = CaptureBank(image, owner, d.source.bank.hierarchy, bank)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_ANIMATION_ASSOCIATION_DESTRUCTOR_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_ANIMATION_ASSOCIATION_DESTRUCTOR_INVOKE;
#else
            const auto entry = invoke ? invoke : &AnimationAssociationStorageDetail::InvokeDestructor;
#endif
            d.destructorEntered = true;
            s.state.store(State::RetireEntered);
            r.entered = true;
            std::uintptr_t returned = 0;
            result = entry(reinterpret_cast<NativeAnimationAssociationCalls::DestructorFunction>(image + NativeAnimationAssociationProfile::DestructorRva),
                           d.receiver, 1, returned, r.exceptionCode);
            r.returned = result == ClockResult::Ok;
            r.accepted = r.returned && returned == d.receiver;
            if (!r.accepted)
            {
                s.state.store(State::FaultDebt);
                return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
            }
            // A returned deleting destructor consumes the outer/node storage
            // before any fallible acknowledgement. Keep the distinct original
            // bank-reference journal until its postflight is verified: losing
            // that acknowledgement must not erase debt or replay the dtor.
            // Never inspect the freed receiver or free its outer again.
            d.outer.state.store(NativeCustomAnimSequenceStorage::State::Released);
            if (ClumpNoRef(d))
                d.clumpRetiredAck = true;
            s.state.store(State::FaultDebt);
            r.retired = true;
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            if (!QualifyAssociationProfile(image) || (result = NativeModelReferenceHeap::Recheck(image, heap)) != ClockResult::Ok)
                return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
            if (!ClumpPrivate(d))
            {
                Bank after;
                if ((result = CaptureBank(image, owner, d.source.bank.hierarchy, after)) != ClockResult::Ok)
                    return finish(result);
                const auto expectedRefs = ClumpNoRef(d) ? bank.refs : static_cast<std::uint16_t>(bank.refs - 1);
                if (!SameBank(bank, after) || after.refs != expectedRefs)
                    return finish(ClockResult::BindingChanged);
            }
            // A clump-origin constructor never acquired a native bank ref.
            // Its genuine dtor therefore balances no such ref; private H uses
            // the separate actual CPU dependency, released only after this ACK.
            if (d.clump)
            {
                // Copy-backed Init also owns this clump journal. Its native
                // retirement and reference balance are now both acknowledged;
                // only the remaining CPU dependency release may follow.
                d.clumpRetiredAck = true;
                const auto released = ClumpReleaseDependencies(image, s, generation, owner);
                if (released != ClockResult::Ok)
                    return finish(released);
            }
            d.retirementVerified = true;
            s.state.store(State::Released);
            r.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult ResolveHierarchy(std::uintptr_t image, const S& s, std::uint64_t generation, C owner,
                                            NativeAnimationHierarchyReferenceToken& output) noexcept
        {
            namespace A = AnimationAssociationStorageDetail;
            if (!MetadataReadable(s.GetState()))
                return ClockResult::NotInitialized;
            if (!A::Context::TryEnter())
                return ClockResult::Busy;
            A::Context::Scope gate;
            if (!MetadataReadable(s.GetState()))
                return ClockResult::NotInitialized;
            if (s.data && ClumpPrivate(*s.data))
                return ClumpResolveHierarchy(image, s, generation, owner, output);
            A::Bytes                        first{}, second{};
            NativeModelReferenceHeap::Value heap, after;
            auto                            result = Snapshot(image, s, generation, owner, first, heap);
            if (result != ClockResult::Ok)
                return result;
            if ((result = Snapshot(image, s, generation, owner, second, after)) != ClockResult::Ok)
                return result;
            if (first != second || !NativeModelReferenceHeap::Same(heap, after))
                return ClockResult::BindingChanged;
            if ((result = A::Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            // Snapshot already proves the original bank row, owning block,
            // highwater, hierarchy and actual retained reference together.
            NativeAnimationHierarchyReferenceToken next;
            next.ownerGeneration = generation;
            next.locator = s.data->source.bank.hierarchy;
            output = next;
            return ClockResult::Ok;
        }
        struct RestrictionSource
        {
            NativeAnimationStaticAssociationReferenceToken reference{};
            std::array<unsigned char, 32>                  row{};
            std::vector<std::uintptr_t>                    prefix;
            bool                                           noOp = false;
        };
        static ClockResult CaptureRestrictionSource(std::uintptr_t image, C owner, std::uintptr_t raw, std::uint16_t targetCount,
                                                    RestrictionSource& output) noexcept
        {
            namespace A = AnimationAssociationStorageDetail;
            RestrictionSource next;
            if (!raw)
            {
                next.noOp = true;
                output = std::move(next);
                return ClockResult::Ok;
            }
            NativeAnimationManagerMemory manager({image}, owner);
            auto                         result = manager.ResolveStaticAssociationReference(raw, next.reference);
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationStaticAssociationValues values;
            if ((result = manager.ReadStaticAssociation(next.reference, values)) != ClockResult::Ok)
                return result;
            if (!A::M::Copy(raw, next.row.data(), next.row.size()))
                return ClockResult::ReadFailure;
            const auto sourceCount = A::Field<std::uint16_t>(next.row, 8);
            const auto map = A::Field<std::uintptr_t>(next.row, 0x10);
            if (sourceCount != values.numBlendNodes)
                return ClockResult::BindingChanged;
            // MTA tests the pointer before using the unsigned source count.
            // Preserve that real no-op even for a raw high-bit count. A zero
            // target likewise never reads an original map element.
            next.noOp = !map || !targetCount;
            if (!next.noOp && sourceCount)
            {
                // Source count is an unsigned WORD. Only the prefix actually
                // consulted by the target loop needs a map span; targetCount
                // already bounds this work without rejecting a high-bit source.
                const auto needed = (std::min)(static_cast<std::size_t>(sourceCount), static_cast<std::size_t>(targetCount));
                const auto bytes = needed * sizeof(std::uintptr_t);
                if (map % alignof(std::uintptr_t) || !A::M::PrivateWritable(map, bytes) || !A::M::Disjoint(map, bytes, raw, 32))
                    return ClockResult::BindingChanged;
                try
                {
                    next.prefix.resize(needed);
                }
                catch (...)
                {
                    return ClockResult::WriteFailure;
                }
                if (!A::M::Copy(map, next.prefix.data(), bytes))
                    return ClockResult::ReadFailure;
                std::vector<std::uintptr_t> confirmed;
                try
                {
                    confirmed.resize(needed);
                }
                catch (...)
                {
                    return ClockResult::WriteFailure;
                }
                if (!A::M::Copy(map, confirmed.data(), bytes))
                    return ClockResult::ReadFailure;
                if (confirmed != next.prefix)
                    return ClockResult::BindingChanged;
            }
            std::array<unsigned char, 32> after{};
            if (!A::M::Copy(raw, after.data(), after.size()))
                return ClockResult::ReadFailure;
            if (after != next.row)
                return ClockResult::BindingChanged;
            NativeAnimationStaticAssociationReferenceToken confirmed;
            if ((result = manager.ResolveStaticAssociationReference(raw, confirmed)) != ClockResult::Ok)
                return result;
            if (confirmed.groupIndex != next.reference.groupIndex || confirmed.index != next.reference.index || confirmed.locator != raw)
                return ClockResult::BindingChanged;
            output = std::move(next);
            return ClockResult::Ok;
        }
        static bool SameRestrictionSource(const RestrictionSource& a, const RestrictionSource& b) noexcept
        {
            return a.reference.groupIndex == b.reference.groupIndex && a.reference.index == b.reference.index && a.reference.locator == b.reference.locator &&
                   a.row == b.row && a.prefix == b.prefix && a.noOp == b.noOp;
        }
        static ClockResult Restrict(std::uintptr_t image, S& s, std::uint64_t generation, bool fromSource, std::uint64_t mask, std::uintptr_t original, C owner,
                                    Receipt* output, NativeAnimationAssociationCalls::ExchangeNode invoke) noexcept
        {
            namespace A = AnimationAssociationStorageDetail;
            Receipt r;
            auto    finish = [&](ClockResult result)
            {
                if (s.data)
                    Fill(*s.data, r);
                r.pinned = s.GetState() == State::PublicationPinned;
                if (output)
                    *output = r;
                return result;
            };
            if (s.GetState() != State::LiveUnpublished)
                return finish(ClockResult::NotInitialized);
            if (!A::Context::TryEnter())
                return finish(ClockResult::Busy);
            A::Context::Scope gate;
            if (s.GetState() != State::LiveUnpublished)
                return finish(ClockResult::NotInitialized);
            A::Bytes                        first{}, confirmed{};
            NativeModelReferenceHeap::Value heap, current;
            auto                            result = Snapshot(image, s, generation, owner, first, heap);
            if (result != ClockResult::Ok)
                return finish(result);
            auto&             d = *s.data;
            RestrictionSource source;
            if (fromSource)
            {
                if ((result = CaptureRestrictionSource(image, owner, original, d.source.count, source)) != ClockResult::Ok)
                    return finish(result);
                if (!source.prefix.empty())
                {
                    const auto map = A::Field<std::uintptr_t>(source.row, 0x10), bytes = source.prefix.size() * sizeof(std::uintptr_t);
                    if (!A::M::Disjoint(map, bytes, d.nodeRaw, d.request) ||
                        !A::M::Disjoint(map, bytes, d.receiver, NativeAnimationAssociationProfile::ReceiverBytes))
                        return finish(ClockResult::BindingChanged);
                }
            }
            if ((result = Snapshot(image, s, generation, owner, confirmed, current)) != ClockResult::Ok)
                return finish(result);
            if (first != confirmed || !NativeModelReferenceHeap::Same(heap, current))
                return finish(ClockResult::BindingChanged);
            if (fromSource && source.noOp)
            {
                RestrictionSource after;
                if ((result = CaptureRestrictionSource(image, owner, original, d.source.count, after)) != ClockResult::Ok)
                    return finish(result);
                if (!SameRestrictionSource(source, after))
                    return finish(ClockResult::BindingChanged);
                if ((result = A::Context::Owner(image, owner)) != ClockResult::Ok)
                    return finish(result);
                r.noOp = true;
                r.verified = true;
                return finish(ClockResult::Ok);
            }
#if defined(MTA_DE_ANIMATION_ASSOCIATION_NODE_EXCHANGE)
            const auto entry = invoke ? invoke : &MTA_DE_ANIMATION_ASSOCIATION_NODE_EXCHANGE;
#else
            const auto entry = invoke ? invoke : &AnimationAssociationStorageDetail::ExchangeNode;
#endif
            const auto failed = [&](ClockResult failure)
            {
                if (r.entered && s.GetState() != State::PublicationPinned)
                    s.state.store(State::FaultDebt);
                return finish(failure);
            };
            for (std::size_t index = 0; index != d.source.count; ++index)
            {
                ++r.visitedNodes;
                const bool keep = fromSource ? (index < source.prefix.size() && source.prefix[index] != 0) : (index < 64 && ((mask >> index) & 1) != 0);
                if (keep || d.suppressed[index] || !d.source.map[index])
                    continue;
                const auto                    node = d.nodes + index * NativeAnimationAssociationProfile::NodeBytes;
                std::array<unsigned char, 40> before{}, after{};
                if (!A::M::Copy(node, before.data(), before.size()))
                    return failed(ClockResult::ReadFailure);
                const auto expected = d.source.map[index];
                if (A::Field<std::uintptr_t>(before, 0x10) != expected || A::Field<std::uintptr_t>(before, 0x18) != d.receiver)
                    return failed(Pin(s, ClockResult::BindingChanged));
                if ((result = A::Context::Owner(image, owner)) != ClockResult::Ok)
                    return failed(result);
                std::uintptr_t observed = 0;
                bool           accepted = false;
                r.entered = true;
                ++r.attemptedNodes;
                result = entry(node + 0x10, expected, 0, observed, accepted, r.exceptionCode);
                r.returned = result == ClockResult::Ok;
                if (accepted)
                {
                    // Persist the accepted intent before any later callback
                    // or read can fail. The immutable baseline remains intact.
                    d.suppressed[index] = 1;
                    ++r.acceptedNodes;
                    r.accepted = true;
                }
                if (result != ClockResult::Ok || !accepted || observed != expected)
                    return failed(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
                if ((result = A::Context::Owner(image, owner)) != ClockResult::Ok)
                    return failed(result);
                if (!A::M::Copy(node, after.data(), after.size()))
                    return failed(ClockResult::ReadFailure);
                A::Put(before, 0x10, std::uintptr_t{0});
                if (before != after)
                    return failed(ClockResult::BindingChanged);
            }
            if (fromSource)
            {
                RestrictionSource after;
                if ((result = CaptureRestrictionSource(image, owner, original, d.source.count, after)) != ClockResult::Ok)
                    return failed(result);
                if (!SameRestrictionSource(source, after))
                    return failed(ClockResult::BindingChanged);
            }
            if ((result = Snapshot(image, s, generation, owner, confirmed, current)) != ClockResult::Ok)
                return failed(result);
            if (first != confirmed || !NativeModelReferenceHeap::Same(heap, current))
                return failed(ClockResult::BindingChanged);
            r.noOp = r.attemptedNodes == 0;
            r.verified = true;
            return finish(ClockResult::Ok);
        }
    };
    namespace NativeAnimationAssociationCalls
    {
        ClockResult CopyFromGroup(std::uintptr_t image, Storage& storage, const NativeAnimationAssociationReferenceToken& issued,
                                  const NativeAnimationAssociationGroupReferenceToken& group, std::uint32_t animationId, CheckOwner owner,
                                  std::uintptr_t& output, GroupCopyReceipt* receipt, InvokeGroupCopy invoke) noexcept
        {
            const auto request = group;
            const auto ticket = issued;
            return NativeAnimationAssociationStorageAccess::CopyFromGroup(image, storage, ticket, request, animationId, owner, output, receipt, invoke);
        }
        ClockResult Create(std::uintptr_t i, Storage& s, std::uint64_t g, CheckOwner o, Receipt* r,
                           NativeCustomAnimSequenceStorageCalls::InvokeMalloc v) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Create(i, s, g, o, r, v);
        }
        ClockResult Construct(std::uintptr_t i, Storage& s, std::uint64_t g, std::uintptr_t source, CheckOwner o, std::uintptr_t& result, Receipt* r,
                              InvokeConstructor v) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Construct(i, s, g, source, o, result, r, v);
        }
        bool MatchesIssuedInterface(const Storage& storage, std::uint64_t generation, std::uintptr_t candidate) noexcept
        {
            return NativeAnimationAssociationStorageAccess::MatchesIssuedInterface(storage, generation, candidate);
        }
        ClockResult ReadInterface(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner, std::uintptr_t& output) noexcept
        {
            return NativeAnimationAssociationStorageAccess::ReadInterface(image, storage, generation, owner, output);
        }
        ClockResult Read(std::uintptr_t i, const Storage& s, std::uint64_t g, CheckOwner o, NativeAnimationAssociationValues& result) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Read(i, s, g, o, result);
        }
        ClockResult ReadLength(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner, std::uint32_t& output) noexcept
        {
            return NativeAnimationAssociationStorageAccess::ReadLength(image, storage, generation, owner, output);
        }
        ClockResult Set(std::uintptr_t i, Storage& s, std::uint64_t g, Scalar f, std::uint32_t b, CheckOwner o, Receipt* r, Exchange v) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Set(i, s, g, f, b, o, r, v);
        }
        ClockResult SetCurrentProgress(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::uint32_t bits, CheckOwner owner, Receipt* receipt,
                                       InvokeProgress invoke) noexcept
        {
            return NativeAnimationAssociationStorageAccess::SetCurrentProgress(image, storage, generation, bits, owner, receipt, invoke);
        }
        ClockResult FreeNodeArray(std::uintptr_t i, Storage& s, std::uint64_t g, CheckOwner o, Receipt* r,
                                  NativeCustomAnimSequenceStorageCalls::InvokeFree v) noexcept
        {
            return NativeAnimationAssociationStorageAccess::FreeNodeArray(i, s, g, o, r, v);
        }
        ClockResult Retire(std::uintptr_t i, Storage& s, std::uint64_t g, CheckOwner o, Receipt* r, InvokeDestructor v,
                           NativeCustomAnimSequenceStorageCalls::InvokeFree f, ExchangeNode c) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Retire(i, s, g, o, r, v, f, c);
        }
        ClockResult ResolveHierarchy(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner,
                                     NativeAnimationHierarchyReferenceToken& output) noexcept
        {
            return NativeAnimationAssociationStorageAccess::ResolveHierarchy(image, storage, generation, owner, output);
        }
        ClockResult RestrictToBones(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::uint64_t mask, CheckOwner owner, Receipt* receipt,
                                    ExchangeNode invoke) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Restrict(image, storage, generation, false, mask, 0, owner, receipt, invoke);
        }
        ClockResult RestrictToBonesOf(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::uintptr_t original, CheckOwner owner,
                                      Receipt* receipt, ExchangeNode invoke) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Restrict(image, storage, generation, true, 0, original, owner, receipt, invoke);
        }
    }
}

namespace mta::game_de::NativeAnimationAssociationCalls
{
    ClockResult ApplyClump(std::uintptr_t image, NativeAnimationAssociationStorage& storage, std::uint64_t generation,
                           NativeAnimationAssociationClumpOperation operation, const NativeAnimationAssociationClumpOrigin& origin,
                           ClockResult (*owner)() noexcept, std::uintptr_t& returned, ClumpReceipt* receipt, const ClumpInvocations* invocations) noexcept
    {
        return NativeAnimationAssociationStorageAccess::ApplyClumpCore(image, storage, generation, operation, origin, owner, returned, receipt, invocations);
    }
}
