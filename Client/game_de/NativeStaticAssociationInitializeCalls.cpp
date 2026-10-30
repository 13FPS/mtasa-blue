/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow actual clump storage for one original static-association Init
 ****************************************************************************/
#include "NativeStaticAssociationInitializeCalls.h"
#include "NativeStaticAssociationInitializeCore.h"
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
        using TemporaryOrigin = NativeStaticAssociationTemporaryOrigin;
        template <class T, std::size_t N>
        T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, P::Initializer.size()> actual{};
            return NativeAnimationClumpInitializationPlan::QualifyProfile(image) &&
                   I::ReadImage(image, image + P::EntryRva, actual.data(), actual.size(), false, true) && actual == P::Initializer;
        }
        namespace SharedPlan = NativeAnimationClumpInitializationPlan;
        using BoneRow = SharedPlan::BoneRow;
        using BoneMap = SharedPlan::BoneMap;
        struct Value : SharedPlan::Snapshot
        {
            Row row{};
        };
        bool Same(const Value& a, const Value& b) noexcept
        {
            return SharedPlan::Same(a, b);
        }
        ClockResult Capture(std::uintptr_t image, const Arguments& input, CheckOwner owner, Value& output, const TemporaryOrigin* origin = nullptr,
                            bool after = false)
        {
            Value value;
            auto  result = NativeStaticAssociationInitializeCoreAccess::CapturePlan(image, input, owner, origin, value);
            if (result != ClockResult::Ok)
                return result;
            const auto render = input.clump;
            if (!M::Copy(input.association, value.row.data(), value.row.size()) || Field<std::uintptr_t>(value.row, 0) != image + P::StaticVtableRva ||
                Field<std::uintptr_t>(value.row, 0x18) != (origin && !after ? std::uintptr_t{0} : input.hierarchy) ||
                !M::Disjoint(input.association, 32, value.container, P::ContainerBytes) || !M::Disjoint(input.association, 32, render, 0x310))
                return ClockResult::BindingChanged;
            // Readable actor/type bytes are insufficient once an item enters
            // retirement or its slot is reused. Extend the existing current
            // membership/serial graph before dereferences and preserve those
            // additions through both call bookends; no new GC lease is issued.
            for (std::size_t index = 0; index != value.world.count; ++index)
                if (!D::Add(image, value.world.members[index].classAddress, value.world))
                    return ClockResult::BindingChanged;
            if (origin)
            {
                if ((result = origin->Recheck()) != ClockResult::Ok)
                    return result;
            }
            else if ((result = NativePedPoolReader({image}, owner).Recheck(input.ped)) != ClockResult::Ok)
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
        ClockResult ObserveNodes(const Arguments& input, const Value& state, std::vector<std::uintptr_t>& output, bool allowEmpty = false)
        {
            const auto nodes = Field<std::uintptr_t>(state.row, 0x10);
            const auto bytes = static_cast<std::size_t>(state.frameCount) * sizeof(std::uintptr_t);
            if (allowEmpty && !state.frameCount)
            {
                // Original malloc(0) may return null or its own opaque tag.
                // Preserve that observed tag without a zero-extent read or a
                // fabricated allocation/free claim; the group owns the row.
                if (Field<std::uint16_t>(state.row, 8) != 0)
                    return ClockResult::BindingChanged;
                output.clear();
                return ClockResult::Ok;
            }
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
    static ClockResult InitializeCore(std::uintptr_t image, const Arguments& source, CheckOwner owner, RecheckArguments recheck, void* context, Receipt* output,
                                      Invoke invoke, const TemporaryOrigin* origin) noexcept
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
        try
        {
            Value before, confirmed;
            auto  result = recheck(input, false, context);
            if (result != ClockResult::Ok || (result = Capture(image, input, owner, before, origin, false)) != ClockResult::Ok)
                return finish(result);
            receipt.container = before.container;
            receipt.heap = before.heap.current;
            receipt.frameCount = before.frameCount;
            receipt.beforeNodes = Field<std::uintptr_t>(before.row, 0x10);
            // FEB3F0 does not define WORD+8. Its null-clump Init publishes H
            // without allocating nodes; only the real pointer0 phase is fresh.
            if (receipt.beforeNodes)
                return finish(ClockResult::InvalidArgument);
            if ((result = recheck(input, false, context)) != ClockResult::Ok ||
                (result = Capture(image, input, owner, confirmed, origin, false)) != ClockResult::Ok)
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
            if (origin)
            {
                if ((result = origin->Claim()) != ClockResult::Ok)
                    return finish(result);
                origin->Entered();
            }
            receipt.entered = true;
            result = entry(reinterpret_cast<NativeFunction>(image + P::EntryRva), input.association, input.clump, input.hierarchy, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.returned = true;
            ObserveReceipt(input, receipt);
            if ((result = Capture(image, input, owner, confirmed, origin, true)) != ClockResult::Ok)
                return finish(result);
            const bool rowPreserved = origin ? std::memcmp(before.row.data(), confirmed.row.data(), 8) == 0 &&
                                                   std::memcmp(before.row.data() + 0x0a, confirmed.row.data() + 0x0a, 6) == 0 &&
                                                   Field<std::uintptr_t>(before.row, 0x18) == 0 && Field<std::uintptr_t>(confirmed.row, 0x18) == input.hierarchy
                                             : DefinedRowPreserved(before.row, confirmed.row);
            if (!Same(before, confirmed) || !rowPreserved)
                return finish(ClockResult::BindingChanged);
            std::vector<std::uintptr_t> nodes, finalNodes;
            if ((result = ObserveNodes(input, confirmed, nodes, origin != nullptr)) != ClockResult::Ok ||
                (result = recheck(input, true, context)) != ClockResult::Ok || (result = Capture(image, input, owner, before, origin, true)) != ClockResult::Ok)
                return finish(result);
            if (!Same(confirmed, before) || before.row != confirmed.row ||
                (result = ObserveNodes(input, before, finalNodes, origin != nullptr)) != ClockResult::Ok)
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
        const auto refused = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (!owner || !recheck)
            return refused(ClockResult::NotInitialized);
        if (!input.clump || !input.association || !input.hierarchy || input.association % 8 || input.hierarchy % 8)
            return refused(ClockResult::InvalidArgument);
        if (!D::TryEnter())
            return refused(ClockResult::Busy);
        D::Scope scope;
        // Registered-ped callers retain their original public gate, Arguments,
        // admission predicates and before/after H-preservation semantics.
        return InitializeCore(image, input, owner, recheck, context, output, invoke, nullptr);
    }
}

namespace mta::game_de
{
    ClockResult NativeStaticAssociationInitializeCoreAccess::Initialize(const NativeStaticAssociationTemporaryOrigin&    origin,
                                                                        NativeStaticAssociationInitializeCalls::Receipt* output,
                                                                        NativeStaticAssociationInitializeCalls::Invoke   invoke) noexcept
    {
        return NativeStaticAssociationInitializeCalls::InitializeCore(origin.image, origin.input, origin.owner, origin.recheckArguments,
                                                                      const_cast<void*>(origin.context), output, invoke, &origin);
    }
}

namespace mta::game_de
{
    ClockResult NativeStaticAssociationInitializeCoreAccess::CapturePlan(std::uintptr_t image, const NativeStaticAssociationInitializeCalls::Arguments& input,
                                                                         NativeStaticAssociationInitializeCalls::CheckOwner owner,
                                                                         const NativeStaticAssociationTemporaryOrigin*      temporary,
                                                                         NativeAnimationClumpInitializationPlan::Snapshot&  output)
    {
        NativeAnimationClumpInitializationPlan::Origin origin;
        origin.image = image;
        origin.input = input;
        origin.owner = owner;
        origin.receiverBytes = 32;
        if (temporary)
        {
            origin.context = temporary;
            origin.temporaryModel = &temporary->Model();
            origin.recheck = [](const void* value) noexcept { return static_cast<const NativeStaticAssociationTemporaryOrigin*>(value)->Recheck(); };
        }
        return NativeAnimationClumpInitializationPlan::Access::CaptureInHeldScope(origin, output);
    }
}
