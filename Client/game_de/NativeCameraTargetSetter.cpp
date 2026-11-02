/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retarget an engine-owned row with acknowledged reference prefixes
 ****************************************************************************/
#include "NativeCameraTargetSetter.h"
#include "NativeCameraDataMemory.h"
#include "NativePedReferenceMemory.h"
#include "NativePedReferenceProfile.h"
#include "NativeImageAccess.h"
#include <intrin.h>
#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace CameraTargetSetterDetail
    {
        namespace D = NativeStreamingCompletionContext;
        namespace R = NativePedReferenceProfile;
        namespace P = NativePedPoolProfile;
        namespace M = NativeCameraDataMemory;
        namespace Calls = NativeCameraTargetSetterCalls;
        using Value = NativePedPoolReader::Value;
        struct Node
        {
            std::uintptr_t next = 0, cell = 0;
        };
        static_assert(sizeof(void*) == 8 && sizeof(Node) == R::ReferenceBytes);
        struct Snapshot
        {
            M::RowTargetWriteSnapshot           row;
            Value                               oldPed, newPed;
            std::uintptr_t                      oldHead = 0, newHead = 0, freeHead = 0;
            std::array<Node, R::ReferenceCount> bank{};
        };
        struct Context
        {
            std::uintptr_t                         image;
            NativeCameraRowBorrow&                 ledger;
            const NativeCameraRowBorrow::Identity& identity;
            const NativeCameraRowBorrow::Token&    row;
            const NativePedReferenceToken*         target;
            Calls::Resolver                        resolver;
            Calls::CheckOwner                      owner, afterApplicationOwner;
            std::uintptr_t                         oldTarget = 0;
        };
        bool SamePed(const Value& a, const Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        bool SameRow(const M::RowTargetWriteSnapshot& a, const M::RowTargetWriteSnapshot& b) noexcept
        {
            return a.cell == b.cell && a.target == b.target && a.bytes == b.bytes && D::Same(a.world, b.world, true);
        }
        bool Same(const Snapshot& a, const Snapshot& b) noexcept
        {
            return SameRow(a.row, b.row) && SamePed(a.oldPed, b.oldPed) && SamePed(a.newPed, b.newPed) && a.oldHead == b.oldHead && a.newHead == b.newHead &&
                   a.freeHead == b.freeHead && !std::memcmp(a.bank.data(), b.bank.data(), sizeof(a.bank));
        }
        bool Index(std::uintptr_t image, std::uintptr_t address, std::size_t& index) noexcept
        {
            const auto begin = image + R::ReferencesRva;
            if (address < begin || address - begin >= R::ReferenceCount * R::ReferenceBytes || (address - begin) % R::ReferenceBytes)
                return false;
            index = static_cast<std::size_t>((address - begin) / R::ReferenceBytes);
            return true;
        }
        bool Graph(std::uintptr_t image, const Snapshot& value, std::uintptr_t registeredTarget) noexcept
        {
            std::array<unsigned char, R::ReferenceCount> paths{};
            for (const auto& node : value.bank)
            {
                std::size_t index = 0;
                if (node.next && !Index(image, node.next, index))
                    return false;
            }
            const auto walk = [&](std::uintptr_t cursor, unsigned char tag, bool free)
            {
                for (std::size_t count = 0; cursor; ++count)
                {
                    std::size_t index = 0;
                    if (count >= R::ReferenceCount || !Index(image, cursor, index) || paths[index] || (value.bank[index].cell == 0) != free)
                        return false;
                    paths[index] = tag;
                    cursor = value.bank[index].next;
                }
                return true;
            };
            if (!walk(value.freeHead, 1, true) || !walk(value.oldHead, 2, false))
                return false;
            const bool samePed = value.oldPed.nativePed && value.oldPed.nativePed == value.newPed.nativePed;
            if ((samePed && value.newHead != value.oldHead) || (!samePed && !walk(value.newHead, 4, false)))
                return false;
            unsigned char expected = 0;
            if (registeredTarget)
            {
                if (registeredTarget == value.oldPed.nativePed)
                    expected = 2;
                else if (registeredTarget == value.newPed.nativePed)
                    expected = 4;
                else
                    return false;
            }
            std::size_t matches = 0;
            for (std::size_t index = 0; index != value.bank.size(); ++index)
                if (value.bank[index].cell == value.row.cell)
                {
                    if (!expected || paths[index] != expected)
                        return false;
                    ++matches;
                }
            return matches == (expected ? 1u : 0u);
        }
        ClockResult Guard(const Context& context) noexcept
        {
            auto result = context.afterApplicationOwner();
            if (result != ClockResult::Ok)
                return result;
            if ((result = D::Owner(context.image, context.owner)) != ClockResult::Ok)
                return result;
            if (!NativePedReferenceMemory::Revalidate({context.image}))
                return ClockResult::UnsupportedBuild;
            return context.ledger.Validate(context.row, context.identity);
        }
        ClockResult Ped(std::uintptr_t image, Calls::CheckOwner owner, std::uintptr_t raw, Value& output) noexcept
        {
            if (!raw)
            {
                output = {};
                return ClockResult::Ok;
            }
            std::uintptr_t pool = 0, objects = 0, flags = 0;
            std::int32_t   capacity = 0;
            if (!NativeImageAccess::ReadImage(image, image + P::RootRva, &pool, sizeof(pool), true) || !D::At(pool, P::ObjectsOffset, objects) ||
                !D::At(pool, P::FlagsOffset, flags) || !D::At(pool, P::CapacityOffset, capacity))
                return ClockResult::ReadFailure;
            if (capacity != static_cast<std::int32_t>(P::NativeCapacity) || !objects || !flags || raw < objects ||
                raw - objects >= P::NativeCapacity * P::ObjectStride || (raw - objects) % P::ObjectStride)
                return ClockResult::BindingChanged;
            const auto    slot = static_cast<std::uint32_t>((raw - objects) / P::ObjectStride);
            unsigned char generation = 0;
            if (!D::At(flags, slot, generation))
                return ClockResult::ReadFailure;
            Value      next;
            const auto result = NativePedPoolReader({image}, owner).Resolve((slot << 8) | generation, next);
            if (result != ClockResult::Ok)
                return result;
            if (next.nativePed != raw || (next.vtable != image + R::PedVtableRva && next.vtable != image + R::PlayerVtableRva))
                return ClockResult::BindingChanged;
            unsigned char type = 0;
            if (!D::At(raw, R::TypeOffset, type))
                return ClockResult::ReadFailure;
            if ((type & 7) != 3)
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
        ClockResult Head(const Value& ped, std::uintptr_t& output) noexcept
        {
            if (!ped.nativePed)
            {
                output = 0;
                return ClockResult::Ok;
            }
            if (ped.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - R::EntityHeadOffset ||
                !D::Writable(ped.nativePed + R::EntityHeadOffset, sizeof(output)))
                return ClockResult::BindingChanged;
            return D::At(ped.nativePed, R::EntityHeadOffset, output) ? ClockResult::Ok : ClockResult::ReadFailure;
        }
        ClockResult Capture(const Context& context, std::uintptr_t registeredTarget, Snapshot& output) noexcept
        {
            auto result = Guard(context);
            if (result != ClockResult::Ok)
                return result;
            if ((result = M::CaptureRowTargetForWrite(context.image, context.ledger, context.identity, context.row, context.owner,
                                                      context.afterApplicationOwner, output.row)) != ClockResult::Ok ||
                (result = Ped(context.image, context.owner, context.oldTarget, output.oldPed)) != ClockResult::Ok)
                return result;
            output.newPed = {};
            if (context.target)
            {
                Value resolved, confirmed;
                if ((result = context.resolver.resolve(context.resolver.owner, *context.target, resolved)) != ClockResult::Ok ||
                    (result = Ped(context.image, context.owner, resolved.nativePed, confirmed)) != ClockResult::Ok)
                    return result;
                if (!resolved.nativePed || !SamePed(resolved, confirmed))
                    return ClockResult::BindingChanged;
                output.newPed = confirmed;
            }
            if ((result = Head(output.oldPed, output.oldHead)) != ClockResult::Ok || (result = Head(output.newPed, output.newHead)) != ClockResult::Ok)
                return result;
            std::uintptr_t freeAfter = 0;
            if (!NativeImageAccess::ReadImage(context.image, context.image + R::FreeHeadRva, &output.freeHead, sizeof(output.freeHead), true) ||
                !NativeImageAccess::ReadImage(context.image, context.image + R::ReferencesRva, output.bank.data(), sizeof(output.bank), true) ||
                !NativeImageAccess::ReadImage(context.image, context.image + R::FreeHeadRva, &freeAfter, sizeof(freeAfter), true))
                return ClockResult::ReadFailure;
            std::uintptr_t oldAfter = 0, newAfter = 0;
            if ((result = Head(output.oldPed, oldAfter)) != ClockResult::Ok || (result = Head(output.newPed, newAfter)) != ClockResult::Ok)
                return result;
            if (freeAfter != output.freeHead || oldAfter != output.oldHead || newAfter != output.newHead || !Graph(context.image, output, registeredTarget))
                return ClockResult::BindingChanged;
            M::RowTargetWriteSnapshot rowAfter;
            if ((result = M::CaptureRowTargetForWrite(context.image, context.ledger, context.identity, context.row, context.owner,
                                                      context.afterApplicationOwner, rowAfter)) != ClockResult::Ok)
                return result;
            if (!SameRow(output.row, rowAfter))
                return ClockResult::BindingChanged;
            return Guard(context);
        }
        bool UnregisterPlan(std::uintptr_t image, Snapshot& value) noexcept
        {
            if (!value.oldPed.nativePed)
                return true;
            std::uintptr_t* previous = &value.oldHead;
            for (std::size_t count = 0; *previous && count != R::ReferenceCount; ++count)
            {
                std::size_t index = 0;
                if (!Index(image, *previous, index))
                    return false;
                auto& node = value.bank[index];
                if (node.cell == value.row.cell)
                {
                    const auto removed = *previous;
                    *previous = node.next;
                    node.next = value.freeHead;
                    value.freeHead = removed;
                    node.cell = 0;
                    return true;
                }
                previous = &node.next;
            }
            return false;
        }
        bool RegisterPlan(std::uintptr_t image, Snapshot& value) noexcept
        {
            if (!value.newPed.nativePed)
                return true;
            std::size_t index = 0;
            if (!Index(image, value.freeHead, index) || value.bank[index].cell)
                return false;
            const auto added = value.freeHead;
            auto&      node = value.bank[index];
            value.freeHead = node.next;
            node.next = value.newHead;
            node.cell = value.row.cell;
            value.newHead = added;
            return true;
        }
#if !defined(MTA_DE_CAMERA_TARGET_UNREGISTER_INVOKE)
        ClockResult UnregisterCall(Calls::UnregisterFunction function, void* ped, void** cell, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(ped, cell);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
#if !defined(MTA_DE_CAMERA_TARGET_REGISTER_INVOKE)
        ClockResult RegisterCall(Calls::RegisterFunction function, void* ped, void** cell, bool force, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(ped, cell, force);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
#if !defined(MTA_DE_CAMERA_TARGET_POINTER_EXCHANGE)
        ClockResult Exchange(std::uintptr_t cell, std::uintptr_t expected, std::uintptr_t target, std::uintptr_t& observed, bool& accepted,
                             std::uint32_t& exception) noexcept
        {
            __try
            {
                observed = reinterpret_cast<std::uintptr_t>(_InterlockedCompareExchangePointer(
                    reinterpret_cast<void* volatile*>(cell), reinterpret_cast<void*>(target), reinterpret_cast<void*>(expected)));
                accepted = observed == expected;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::WriteFailure;
            }
        }
#endif
    }

    struct NativeCameraTargetSetter::Data
    {
        CameraTargetSetterDetail::Snapshot before, unregistered, exchanged, final, observed, bookend;
        NativePedReferenceToken            intent;
        Receipt                            journal;
    };
    NativeCameraTargetSetter::NativeCameraTargetSetter() noexcept = default;
    NativeCameraTargetSetter::~NativeCameraTargetSetter() = default;

    ClockResult NativeCameraTargetSetter::Set(std::uintptr_t image, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                                              const NativeCameraRowBorrow::Token& row, const NativePedReferenceToken* newTarget, Resolver resolver,
                                              CheckOwner owner, CheckOwner afterApplicationOwner, Receipt* output,
                                              NativeCameraTargetSetterCalls::InvokeUnregister unregisterInvoke,
                                              NativeCameraTargetSetterCalls::ExchangePointer  exchange,
                                              NativeCameraTargetSetterCalls::InvokeRegister   registerInvoke) noexcept
    {
        namespace C = CameraTargetSetterDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace R = NativePedReferenceProfile;
        namespace Calls = NativeCameraTargetSetterCalls;
        // Fix CPU intent before owner/resolver callbacks can run. A token
        // identity is still only a request to this same real owner.
        const bool                    hasTarget = newTarget != nullptr;
        const NativePedReferenceToken requested = hasTarget ? *newTarget : NativePedReferenceToken{};
        Receipt                       receipt;
        receipt.image = image;
        receipt.generation = identity.generation;
        receipt.admissionSerial = identity.admissionSerial;
        receipt.rowTokenId = row.Id();
        receipt.row = row.Row();
        bool       ownsBusy = false;
        const auto finish = [&](ClockResult result)
        {
            receipt.effectsEntered = receipt.unregisterCall.entered || receipt.registerCall.entered || receipt.pointerWrite.accepted ||
                                     (receipt.pointerWrite.entered && !receipt.pointerWrite.returned);
            if (receipt.effectsEntered && !receipt.effectsVerified)
            {
                debt.store(true);
                receipt.debt = true;
                if (data)
                {
                    receipt.result = result;
                    data->journal = receipt;
                }
            }
            receipt.debt = debt.load();
            receipt.result = result;
            if (ownsBusy && !receipt.debt)
                data.reset();
            if (output)
                *output = receipt;
            return result;
        };
        if (busy.exchange(true))
            return finish(ClockResult::Busy);
        ownsBusy = true;
        struct BusyScope
        {
            std::atomic<bool>& busy;
            ~BusyScope() { busy.store(false); }
        } busyScope{busy};
        if (debt.load())
            return finish(ClockResult::RestoreFailure);
        if (!owner || !afterApplicationOwner || (hasTarget && (!resolver.owner || !resolver.resolve)))
            return finish(ClockResult::NotInitialized);
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - NativePedPoolProfile::ImageSize)
            return finish(ClockResult::UnsupportedBuild);
        if (identity.image != image)
            return finish(ClockResult::OwnerConflict);
        if (hasTarget && (!requested.version || !requested.ownerGeneration || requested.slot == MtaGame::CPoolSlotPolicy::InvalidSlot))
            return finish(ClockResult::InvalidArgument);
        if (hasTarget && requested.ownerGeneration != identity.generation)
            return finish(ClockResult::OwnerConflict);
        auto result = afterApplicationOwner();
        if (result != ClockResult::Ok)
            return finish(result);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        try
        {
            data = std::make_unique<Data>();
        }
        catch (...)
        {
            return finish(ClockResult::WriteFailure);
        }
        data->intent = requested;
        C::Context context{image, ledger, identity, row, hasTarget ? &data->intent : nullptr, resolver, owner, afterApplicationOwner};
        if ((result = NativeCameraDataMemory::CaptureRowTargetForWrite(image, ledger, identity, row, owner, afterApplicationOwner, data->before.row)) !=
            ClockResult::Ok)
            return finish(result);
        context.oldTarget = data->before.row.target;
        if ((result = C::Capture(context, context.oldTarget, data->before)) != ClockResult::Ok ||
            (result = C::Capture(context, context.oldTarget, data->observed)) != ClockResult::Ok)
            return finish(result);
        if (data->before.row.target != context.oldTarget || !C::Same(data->before, data->observed))
            return finish(ClockResult::BindingChanged);
        receipt.cell = data->before.row.cell;
        receipt.oldTarget = context.oldTarget;
        receipt.newTarget = data->before.newPed.nativePed;
        receipt.oldHandle = data->before.oldPed.handle;
        receipt.newHandle = data->before.newPed.handle;
        receipt.beforeOldHead = data->before.oldHead;
        receipt.beforeNewHead = data->before.newHead;
        receipt.beforeFreeHead = data->before.freeHead;
        receipt.noEffect = receipt.oldTarget == receipt.newTarget;
        const auto verify = [&](const C::Snapshot& expected, std::uintptr_t registeredTarget)
        {
            auto checked = C::Capture(context, registeredTarget, data->observed);
            if (checked != ClockResult::Ok)
                return checked;
            if ((checked = C::Capture(context, registeredTarget, data->bookend)) != ClockResult::Ok)
                return checked;
            if (!C::Same(expected, data->observed) || !C::Same(data->observed, data->bookend))
                return ClockResult::BindingChanged;
            return C::Guard(context);
        };
        if (!receipt.noEffect)
        {
            // Require capacity even when the old node could replenish it:
            // the qualified route never relies on exhaustion/pruning.
            if (!data->before.freeHead)
                return finish(ClockResult::NotInitialized);
            data->unregistered = data->before;
            if (!C::UnregisterPlan(image, data->unregistered) || !C::Graph(image, data->unregistered, 0))
                return finish(ClockResult::BindingChanged);
            data->exchanged = data->unregistered;
            data->exchanged.row.target = receipt.newTarget;
            std::memcpy(data->exchanged.row.bytes.data() + 0x1a0, &receipt.newTarget, sizeof(receipt.newTarget));
            data->final = data->exchanged;
            if (!C::RegisterPlan(image, data->final) || !C::Graph(image, data->final, receipt.newTarget))
                return finish(ClockResult::BindingChanged);
        }
        else
            data->final = data->before;
        if ((result = verify(data->before, receipt.oldTarget)) != ClockResult::Ok)
            return finish(result);
        receipt.acknowledged = Calls::Prefix::Preflight;
        if (!receipt.noEffect)
        {
#if defined(MTA_DE_CAMERA_TARGET_UNREGISTER_INVOKE)
            const auto unregisterCall = unregisterInvoke ? unregisterInvoke : &MTA_DE_CAMERA_TARGET_UNREGISTER_INVOKE;
#else
            const auto unregisterCall = unregisterInvoke ? unregisterInvoke : &C::UnregisterCall;
#endif
#if defined(MTA_DE_CAMERA_TARGET_POINTER_EXCHANGE)
            const auto pointerCall = exchange ? exchange : &MTA_DE_CAMERA_TARGET_POINTER_EXCHANGE;
#else
            const auto pointerCall = exchange ? exchange : &C::Exchange;
#endif
#if defined(MTA_DE_CAMERA_TARGET_REGISTER_INVOKE)
            const auto registerCall = registerInvoke ? registerInvoke : &MTA_DE_CAMERA_TARGET_REGISTER_INVOKE;
#else
            const auto registerCall = registerInvoke ? registerInvoke : &C::RegisterCall;
#endif
            if (receipt.oldTarget)
            {
                if ((result = C::Guard(context)) != ClockResult::Ok)
                    return finish(result);
                receipt.unregisterCall.entered = true;
                result = unregisterCall(reinterpret_cast<Calls::UnregisterFunction>(image + R::UnregisterRva), reinterpret_cast<void*>(receipt.oldTarget),
                                        reinterpret_cast<void**>(receipt.cell), receipt.unregisterCall.exceptionCode);
                if (result != ClockResult::Ok)
                    return finish(result);
                receipt.unregisterCall.returned = true;
                if ((result = verify(data->unregistered, 0)) != ClockResult::Ok)
                    return finish(result);
                receipt.unregisterCall.verified = true;
            }
            receipt.acknowledged = Calls::Prefix::OldUnregistered;
            if ((result = C::Guard(context)) != ClockResult::Ok)
                return finish(result);
            receipt.pointerWrite.entered = true;
            result = pointerCall(receipt.cell, receipt.oldTarget, receipt.newTarget, receipt.pointerWrite.observed, receipt.pointerWrite.accepted,
                                 receipt.pointerWrite.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.pointerWrite.returned = true;
            if (!receipt.pointerWrite.accepted || receipt.pointerWrite.observed != receipt.oldTarget)
                return finish(ClockResult::BindingChanged);
            if ((result = verify(data->exchanged, 0)) != ClockResult::Ok)
                return finish(result);
            receipt.pointerWrite.verified = true;
            receipt.acknowledged = Calls::Prefix::PointerReplaced;
            if (receipt.newTarget)
            {
                if ((result = C::Guard(context)) != ClockResult::Ok)
                    return finish(result);
                receipt.registerCall.entered = true;
                result = registerCall(reinterpret_cast<Calls::RegisterFunction>(image + R::RegisterRva), reinterpret_cast<void*>(receipt.newTarget),
                                      reinterpret_cast<void**>(receipt.cell), false, receipt.registerCall.exceptionCode);
                if (result != ClockResult::Ok)
                    return finish(result);
                receipt.registerCall.returned = true;
                if ((result = verify(data->final, receipt.newTarget)) != ClockResult::Ok)
                    return finish(result);
                receipt.registerCall.verified = true;
            }
            receipt.acknowledged = Calls::Prefix::NewRegistered;
        }
        if ((result = verify(data->final, receipt.newTarget)) != ClockResult::Ok)
            return finish(result);
        receipt.afterOldHead = data->final.oldHead;
        receipt.afterNewHead = data->final.newHead;
        receipt.afterFreeHead = data->final.freeHead;
        for (std::size_t index = 0; index != data->before.bank.size(); ++index)
            if (std::memcmp(&data->before.bank[index], &data->final.bank[index], sizeof(C::Node)))
                ++receipt.changedReferenceNodes;
        receipt.acknowledged = Calls::Prefix::Complete;
        receipt.effectsVerified = true;
        return finish(ClockResult::Ok);
    }
}
