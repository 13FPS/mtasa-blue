/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Causal ownership of genuinely constructed unpublished associations
 ****************************************************************************/
#include "NativeAnimationAssociationStorage.h"
#include "NativeAnimationAssociationOuterStorageCalls.h"
#include "NativeAnimationAssociationProfile.h"
#include "NativeAnimationHierarchyOrigin.h"
#include "NativeAnimationBlockRefMemory.h"
#include "NativeStreamingCompletionContext.h"
#include <array>
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
        bool                                        knownConstructed = false, constructorEntered = false, destructorEntered = false, retirementVerified = false;
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
        static void Fill(const Data& d, Receipt& r) noexcept
        {
            r.receiver = d.receiver;
            r.nodes = d.nodes;
            r.nodeRaw = d.nodeRaw;
            r.nodeRequestBytes = d.request;
            r.nodeCount = d.source.count;
            r.hierarchy = d.source.bank.hierarchy;
            r.constructed = d.knownConstructed;
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
        static ClockResult Snapshot(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, AnimationAssociationStorageDetail::Bytes& bytes,
                                    NativeModelReferenceHeap::Value& heap) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            const auto state = s.GetState();
            if (state == State::PublicationPinned)
                return ClockResult::OwnerConflict;
            if (!s.data || !s.data->knownConstructed || s.data->destructorEntered || (state != State::LiveUnpublished && state != State::FaultDebt))
                return ClockResult::NotInitialized;
            auto result = Qualify(image, s, generation, owner, heap);
            if (result != ClockResult::Ok)
                return result;
            const auto& d = *s.data;
            if (!M::Copy(d.receiver, bytes.data(), bytes.size()))
                return ClockResult::ReadFailure;
            if (Field<std::uintptr_t>(bytes, 0) != image + NativeAnimationAssociationProfile::VtableRva || Field<std::uintptr_t>(bytes, 8) ||
                Field<std::uintptr_t>(bytes, 0x10) || Field<std::uint32_t>(bytes, 0x48) || Field<std::uintptr_t>(bytes, 0x20) != d.nodes ||
                Field<std::uintptr_t>(bytes, 0x28) != d.source.bank.hierarchy || Field<std::uint16_t>(bytes, 0x18) != d.source.count ||
                !(Field<std::uint16_t>(bytes, 0x46) & 0x4000))
                return Pin(s, ClockResult::BindingChanged);
            Bank bank;
            if ((result = CaptureBank(image, owner, d.source.bank.hierarchy, bank)) != ClockResult::Ok)
                return result;
            if (!SameBank(d.source.bank, bank) || !bank.refs)
                return Pin(s, ClockResult::BindingChanged);
            if (!M::PrivateWritable(d.nodeRaw, d.request) || d.nodes < d.nodeRaw + 8 || d.nodes % 64 || d.nodes - d.nodeRaw > d.request ||
                static_cast<std::size_t>(d.source.count) * NativeAnimationAssociationProfile::NodeBytes > d.request - (d.nodes - d.nodeRaw))
                return Pin(s, ClockResult::BindingChanged);
            if (d.source.sequenceCapacity && !SequenceSpan(bank, d.source.sequenceCapacity))
                return Pin(s, ClockResult::BindingChanged);
            std::uintptr_t original = 0;
            if (!M::Copy(d.nodes - 8, &original, sizeof(original)))
                return ClockResult::ReadFailure;
            if (original != d.nodeRaw)
                return Pin(s, ClockResult::BindingChanged);
            for (std::size_t index = 0; index != d.source.count; ++index)
            {
                std::array<unsigned char, 40> node{};
                if (!M::Copy(d.nodes + index * 40, node.data(), node.size()))
                    return ClockResult::ReadFailure;
                if (Field<std::uintptr_t>(node, 0x10) != d.source.map[index] || Field<std::uintptr_t>(node, 0x18) != d.receiver ||
                    Field<std::uint32_t>(node, 8) != 0xffffffff || Field<std::uint32_t>(node, 0xc) || Field<std::uint32_t>(node, 0x20) != 0xffffffff)
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
        static ClockResult Read(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, NativeAnimationAssociationValues& output) noexcept
        {
            using namespace AnimationAssociationStorageDetail;
            if (s.GetState() != State::LiveUnpublished)
                return ClockResult::NotInitialized;
            if (!Context::TryEnter())
                return ClockResult::Busy;
            Context::Scope gate;
            if (s.GetState() != State::LiveUnpublished)
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
            output = next;
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
            if (s.GetState() != State::LiveUnpublished)
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
        static ClockResult Retire(std::uintptr_t image, S& s, std::uint64_t generation, C owner, Receipt* output,
                                  NativeAnimationAssociationCalls::InvokeDestructor invoke, NativeCustomAnimSequenceStorageCalls::InvokeFree free) noexcept
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
            if (s.GetState() == State::Released)
                return finish(ClockResult::OwnerConflict);
            if (!d.constructorEntered)
            {
                NativeCustomAnimSequenceStorageCalls::Receipt native;
                const auto result = NativeAnimationAssociationOuterStorageCalls::FreeUnconstructed(image, d.outer, generation, owner, &native, free);
                r.entered = native.entered;
                r.returned = native.returned;
                r.accepted = native.released;
                r.retired = native.released;
                r.verified = native.verified;
                r.exceptionCode = native.exceptionCode;
                s.state.store(d.outer.CanDiscard() ? State::Released : State::FaultDebt);
                return finish(result);
            }
            if (!d.knownConstructed || d.destructorEntered)
                return finish(ClockResult::OwnerConflict);
            if (!Context::TryEnter())
                return finish(ClockResult::Busy);
            Context::Scope                  gate;
            Bytes                           before{};
            NativeModelReferenceHeap::Value heap;
            auto                            result = Snapshot(image, s, generation, owner, before, heap);
            if (result != ClockResult::Ok)
                return finish(result);
            Bank bank;
            if ((result = CaptureBank(image, owner, d.source.bank.hierarchy, bank)) != ClockResult::Ok)
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
            s.state.store(State::FaultDebt);
            r.retired = true;
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            if (!QualifyAssociationProfile(image) || (result = NativeModelReferenceHeap::Recheck(image, heap)) != ClockResult::Ok)
                return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
            Bank after;
            if ((result = CaptureBank(image, owner, d.source.bank.hierarchy, after)) != ClockResult::Ok)
                return finish(result);
            if (!SameBank(bank, after) || after.refs != static_cast<std::uint16_t>(bank.refs - 1))
                return finish(ClockResult::BindingChanged);
            d.retirementVerified = true;
            s.state.store(State::Released);
            r.verified = true;
            return finish(ClockResult::Ok);
        }
    };
    namespace NativeAnimationAssociationCalls
    {
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
        ClockResult Read(std::uintptr_t i, const Storage& s, std::uint64_t g, CheckOwner o, NativeAnimationAssociationValues& result) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Read(i, s, g, o, result);
        }
        ClockResult Set(std::uintptr_t i, Storage& s, std::uint64_t g, Scalar f, std::uint32_t b, CheckOwner o, Receipt* r, Exchange v) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Set(i, s, g, f, b, o, r, v);
        }
        ClockResult Retire(std::uintptr_t i, Storage& s, std::uint64_t g, CheckOwner o, Receipt* r, InvokeDestructor v,
                           NativeCustomAnimSequenceStorageCalls::InvokeFree f) noexcept
        {
            return NativeAnimationAssociationStorageAccess::Retire(i, s, g, o, r, v, f);
        }
    }
}
