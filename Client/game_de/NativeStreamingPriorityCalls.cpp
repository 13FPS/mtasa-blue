/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Wait selected real native requests without flushing unrelated work
 ****************************************************************************/
#include "NativeStreamingPriorityCalls.h"
#include "NativeStreamingPriorityProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include <windows.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
namespace mta::game_de::NativeStreamingPriorityCalls
{
    namespace PriorityDetail
    {
        namespace P = NativeStreamingPriorityProfile;
        namespace C = NativeStreamingCompletionContext;
        struct Array
        {
            std::uintptr_t data = 0;
            std::int32_t   count = 0, capacity = 0;
        };
        struct Pair
        {
            std::uintptr_t object = 0, control = 0;
        };
        struct Pending
        {
            Array          loaded;
            std::uintptr_t boundObject = 0, boundInterface = 0;
            Array          paths;
            Pair           handle;
            std::uint32_t  model = 0, originalPriority = 0;
        };
        static_assert(sizeof(Pending) == P::PendingBytes && sizeof(Array) == 16 && sizeof(Pair) == 16, "native x64 pending producer ABI");
        struct Row
        {
            unsigned char flags = 0, state = 0;
        };
        struct Candidate
        {
            Pair          handle;
            std::uint32_t model = 0;
        };
        struct Snapshot
        {
            std::array<Row, P::FileEnd>                            rows{};
            std::array<Candidate, P::MaximumCalls>                 selected{};
            std::uint32_t                                          selectedCount = 0;
            Array                                                  pending;
            std::uintptr_t                                         engine = 0, asset = 0, assetClass = 0;
            std::array<NativeUObjectMembershipAccess::Snapshot, 8> members{};
            std::size_t                                            membersCount = 0;
            unsigned char                                          ready = 0;
        };
        bool Pointer(std::uintptr_t image, std::uintptr_t rva, std::uintptr_t expected) noexcept
        {
            std::uintptr_t actual = 0;
            return NativeImageAccess::ReadImage(image, image + rva, &actual, sizeof(actual)) && actual == image + expected;
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            if (!C::Profile(image))
                return false;
            for (const auto& pin : P::Code)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            // These are relocated image pointers. Compare current image+RVA,
            // never the preferred-base pointer bytes from the disk image.
            return Pointer(image, P::CallbackVtableRva + 0x10, P::CallbackRva) && Pointer(image, P::ControlVtableRva, P::ControlDestroyRva) &&
                   Pointer(image, P::ControlVtableRva + 8, P::ControlDeleteRva);
        }
        bool ArrayValid(const Array& value, std::size_t stride, std::size_t budget) noexcept
        {
            return value.count >= 0 && value.capacity >= value.count && static_cast<std::size_t>(value.capacity) <= budget &&
                   (value.capacity ? (value.data && value.data % alignof(std::uintptr_t) == 0 &&
                                      C::Writable(value.data, static_cast<std::size_t>(value.capacity) * stride))
                                   : value.data == 0);
        }
        bool Member(std::uintptr_t image, std::uintptr_t object, Snapshot& snapshot) noexcept
        {
            if (!object)
                return false;
            for (std::size_t i = 0; i != snapshot.membersCount; ++i)
                if (snapshot.members[i].objectAddress == object)
                    return true;
            if (snapshot.membersCount == snapshot.members.size() ||
                !NativeUObjectMembershipAccess(image, &C::Copy).Capture(object, snapshot.members[snapshot.membersCount]))
                return false;
            ++snapshot.membersCount;
            return true;
        }
        bool Fence(std::uintptr_t image, const Snapshot& snapshot) noexcept
        {
            NativeUObjectMembershipAccess access(image, &C::Copy);
            for (std::size_t i = 0; i != snapshot.membersCount; ++i)
                if (!access.Recheck(snapshot.members[i]))
                    return false;
            return true;
        }
        bool SameOwner(const Snapshot& a, const Snapshot& b) noexcept
        {
            if (a.engine != b.engine || a.asset != b.asset || a.assetClass != b.assetClass || a.membersCount != b.membersCount)
                return false;
            for (std::size_t i = 0; i != a.membersCount; ++i)
            {
                const auto& x = a.members[i];
                const auto& y = b.members[i];
                if (x.objectAddress != y.objectAddress || x.classAddress != y.classAddress || x.vtableAddress != y.vtableAddress ||
                    x.outerAddress != y.outerAddress || x.itemAddress != y.itemAddress || x.index != y.index || x.serial != y.serial)
                    return false;
            }
            return true;
        }
        bool Same(const Snapshot& a, const Snapshot& b) noexcept
        {
            if (!SameOwner(a, b) || a.selectedCount != b.selectedCount || a.ready != b.ready || std::memcmp(&a.pending, &b.pending, sizeof(Array)) ||
                std::memcmp(a.rows.data(), b.rows.data(), sizeof(a.rows)))
                return false;
            for (std::size_t i = 0; i != a.selectedCount; ++i)
                if (a.selected[i].model != b.selected[i].model || a.selected[i].handle.object != b.selected[i].handle.object ||
                    a.selected[i].handle.control != b.selected[i].handle.control)
                    return false;
            return true;
        }
        bool Handle(std::uintptr_t image, const Snapshot& snapshot, const Pair& pair) noexcept
        {
            if (!C::Writable(pair.object, 160) || !C::Writable(pair.control, 24))
                return false;
            std::array<unsigned char, 160> handle{};
            std::array<unsigned char, 24>  control{};
            if (!C::Copy(pair.object, handle.data(), handle.size()) || !C::Copy(pair.control, control.data(), control.size()))
                return false;
            const auto     at = [](const auto& bytes, std::size_t offset, auto& output) { std::memcpy(&output, bytes.data() + offset, sizeof(output)); };
            std::uintptr_t self = 0, selfControl = 0, vtable = 0, ownedObject = 0, manager = 0;
            std::int32_t   strong = 0, weak = 0;
            Array          children;
            at(handle, 0, self);
            at(handle, 8, selfControl);
            at(handle, 0x78, children);
            at(handle, 0x98, manager);
            at(control, 0, vtable);
            at(control, 8, strong);
            at(control, 12, weak);
            at(control, 16, ownedObject);
            // Native Wait pins this real weak-self pair before it can call
            // loading callbacks. The pending record already owns a strong
            // reference; the adapter never manufactures or adjusts one.
            return self == pair.object && selfControl == pair.control && vtable == image + P::ControlVtableRva && ownedObject == pair.object && strong > 0 &&
                   strong < (std::numeric_limits<std::int32_t>::max)() && weak > 0 && weak < (std::numeric_limits<std::int32_t>::max)() &&
                   manager == snapshot.asset + P::AssetStreamableOffset && ArrayValid(children, sizeof(Pair), P::MaximumArray);
        }
        ClockResult Capture(std::uintptr_t image, std::uintptr_t root, Snapshot& output, bool drain = false) noexcept
        {
            output = Snapshot{};
            std::array<unsigned char, 64 * P::RowBytes> rows{};
            for (std::size_t begin = 0; begin != P::FileEnd;)
            {
                const auto count = (std::min)(std::size_t{64}, P::FileEnd - begin);
                if (!NativeImageAccess::ReadImage(image, image + P::RowsRva + begin * P::RowBytes, rows.data(), count * P::RowBytes, true))
                    return ClockResult::ReadFailure;
                for (std::size_t i = 0; i != count; ++i)
                {
                    const Row row{rows[i * P::RowBytes + 12], rows[i * P::RowBytes + 13]};
                    output.rows[begin + i] = row;
                    if ((row.flags & 0x10) && row.state > 3)
                        return ClockResult::UnsupportedBuild;
                    if (!(row.flags & 0x10) || (row.state != 2 && row.state != 3))
                        continue;
                    if (row.flags & 0x80)
                        return ClockResult::BindingChanged;
                    if (output.selectedCount == output.selected.size())
                        return ClockResult::UnsupportedBuild;
                    output.selected[output.selectedCount++].model = static_cast<std::uint32_t>(begin + i);
                }
                begin += count;
            }
            if (!output.selectedCount && !drain)
                return ClockResult::Ok;
            if (!C::At(root, 0x78, output.ready) || output.ready > 1 || !C::At(root, 0x80, output.pending) ||
                !ArrayValid(output.pending, sizeof(Pending), P::MaximumPending))
                return ClockResult::BindingChanged;
            if (!C::At(image, P::EngineRva, output.engine) || !C::Writable(output.engine, P::EngineAssetOffset + 8) ||
                !C::At(output.engine, P::EngineAssetOffset, output.asset) || !C::Writable(output.asset, 0x458) ||
                !C::At(image, P::AssetClassRva, output.assetClass) || !Member(image, output.engine, output) || !Member(image, output.asset, output) ||
                output.members[1].vtableAddress != image + P::AssetVtableRva || output.members[1].classAddress != output.assetClass ||
                !Member(image, output.assetClass, output))
                return ClockResult::BindingChanged;
            for (std::size_t i = 0; i != output.membersCount; ++i)
                if (!Member(image, output.members[i].classAddress, output))
                    return ClockResult::BindingChanged;
            std::uintptr_t callbackVtable = 0;
            if (!C::At(root, P::CallbackOffset, callbackVtable) || callbackVtable != image + P::CallbackVtableRva)
                return ClockResult::BindingChanged;
            for (std::int32_t index = 0; index != output.pending.count; ++index)
            {
                Pending record;
                if (!C::Copy(output.pending.data + static_cast<std::size_t>(index) * sizeof(Pending), &record, sizeof(record)) || record.model >= P::FileEnd ||
                    !ArrayValid(record.loaded, 8, P::MaximumArray) || !ArrayValid(record.paths, 24, P::MaximumArray) || record.boundObject != root ||
                    record.boundInterface != root + P::CallbackOffset || ((record.handle.object == 0) != (record.handle.control == 0)))
                    return ClockResult::BindingChanged;
                // Genuine retirement may drain already-ready nonpriority
                // callbacks too. Qualify their native ownership, but never
                // force their handles through Wait.
                if (record.handle.object && !Handle(image, output, record.handle))
                    return ClockResult::BindingChanged;
                for (std::size_t selected = 0; selected != output.selectedCount; ++selected)
                {
                    auto& candidate = output.selected[selected];
                    if (candidate.model != record.model)
                        continue;
                    if (candidate.handle.object)
                        return ClockResult::BindingChanged;
                    if (!record.handle.object)
                        return ClockResult::UnsupportedBuild;
                    candidate.handle = record.handle;
                }
            }
            for (std::size_t index = 0; index != output.selectedCount; ++index)
                if (!output.selected[index].handle.object)
                    return ClockResult::UnsupportedBuild;
            Array         confirmed;
            unsigned char ready = 0;
            if (!C::At(root, 0x80, confirmed) || std::memcmp(&confirmed, &output.pending, sizeof(Array)) || !C::At(root, 0x78, ready) ||
                ready != output.ready || !Fence(image, output))
                return ClockResult::BindingChanged;
            // Wait's real native loader is mandatory, unlike the optional
            // final all-request flush. Do not call a missing loader object.
            std::uintptr_t loader = 0, vtable = 0, entry = 0;
            if (!C::At(image, P::LoaderRva, loader) || !C::Writable(loader, 8) || !C::At(loader, 0, vtable) ||
                !NativeImageAccess::ReadImage(image, vtable + 0x30, &entry, sizeof(entry)) || entry < image ||
                !NativeImageAccess::ReadImage(image, entry, rows.data(), 1, false, true))
                return ClockResult::NotInitialized;
            return ClockResult::Ok;
        }
        ClockResult Reacquire(std::uintptr_t image, CheckOwner owner, const C::Value& original, C::Value& world) noexcept
        {
            auto result = C::Owner(image, owner);
            if (result != ClockResult::Ok || (result = C::Capture(image, world)) != ClockResult::Ok)
                return result;
            if (!C::Same(original, world, false) || !C::Fence(image, world) || !Profile(image))
                return ClockResult::BindingChanged;
            return NativeStartGameCalls::CheckThread(image);
        }
#if !defined(MTA_DE_STREAMING_PRIORITY_WAIT_INVOKE)
        ClockResult NativeWait(WaitFunction function, std::uintptr_t handle, std::int32_t& nativeResult, std::uint32_t& exception) noexcept
        {
            __try
            {
                nativeResult = function(handle);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
#if !defined(MTA_DE_STREAMING_PRIORITY_DRAIN_INVOKE)
        ClockResult NativeDrain(DrainFunction function, std::uintptr_t root, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(root);
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
    ClockResult Load(std::uintptr_t image, CheckOwner owner, Receipt* output, WaitInvoke waitInvoke, DrainInvoke drainInvoke) noexcept
    {
        namespace D = PriorityDetail;
        namespace C = NativeStreamingCompletionContext;
        namespace P = NativeStreamingPriorityProfile;
        Receipt receipt;
        receipt.waitRva = P::WaitRva;
        receipt.drainRva = P::DrainRva;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!C::TryEnter())
            return finish(ClockResult::Busy);
        C::Scope scope;
        auto     result = C::Owner(image, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!D::Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        C::Value first, world;
        if ((result = C::Capture(image, first)) != ClockResult::Ok)
            return finish(result);
        receipt.root = first.root;
        receipt.gameMode = first.gameMode;
        receipt.level = first.level;
        receipt.world = first.world;
        // Large native row snapshots live outside the callback stack. All
        // allocation happens before entry, with an explicit failure result.
        std::unique_ptr<D::Snapshot> initial(new (std::nothrow) D::Snapshot), current(new (std::nothrow) D::Snapshot);
        if (!initial || !current)
            return finish(ClockResult::ReadFailure);
        if ((result = D::Capture(image, first.root, *initial)) != ClockResult::Ok || (result = D::Reacquire(image, owner, first, world)) != ClockResult::Ok ||
            (result = D::Capture(image, world.root, *current)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(first, world, true) || !D::Same(*initial, *current))
            return finish(ClockResult::BindingChanged);
        receipt.selectedBefore = current->selectedCount;
#if defined(MTA_DE_STREAMING_PRIORITY_WAIT_INVOKE)
        const auto wait = waitInvoke ? waitInvoke : &MTA_DE_STREAMING_PRIORITY_WAIT_INVOKE;
#else
        const auto wait = waitInvoke ? waitInvoke : &D::NativeWait;
#endif
#if defined(MTA_DE_STREAMING_PRIORITY_DRAIN_INVOKE)
        const auto drain = drainInvoke ? drainInvoke : &MTA_DE_STREAMING_PRIORITY_DRAIN_INVOKE;
#else
        const auto drain = drainInvoke ? drainInvoke : &D::NativeDrain;
#endif
        std::array<D::Pair, P::MaximumCalls> visited{};
        std::size_t                          visitedCount = 0;
        for (;;)
        {
            receipt.selectedAfter = current->selectedCount;
            const D::Candidate* next = nullptr;
            for (std::size_t index = 0; index != current->selectedCount && !next; ++index)
            {
                const auto& candidate = current->selected[index];
                bool        seen = false;
                for (std::size_t i = 0; i != visitedCount; ++i)
                    if (visited[i].object == candidate.handle.object && visited[i].control == candidate.handle.control)
                    {
                        seen = true;
                        break;
                    }
                if (!seen)
                    next = &candidate;
            }
            // Native completion can report a failed/requeued model. Like the
            // genuine all-request operation, successful calls do not claim
            // every model loaded. Existing MTA Request policy checks that.
            if (!next)
            {
                if ((result = D::Reacquire(image, owner, first, world)) != ClockResult::Ok)
                    return finish(result);
                receipt.returned = true;
                return finish(ClockResult::Ok);
            }
            if (visitedCount == visited.size())
                return finish(ClockResult::UnsupportedBuild);
            if ((result = D::Reacquire(image, owner, first, world)) != ClockResult::Ok || (result = D::Capture(image, world.root, *initial)) != ClockResult::Ok)
                return finish(result);
            if (!D::Same(*current, *initial) || !D::Fence(image, *initial))
                return finish(ClockResult::BindingChanged);
            if ((result = C::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            const auto pair = next->handle;
            receipt.lastModel = next->model;
            visited[visitedCount++] = pair;
            receipt.entered = true;
            ++receipt.waits;
            result = wait(reinterpret_cast<WaitFunction>(image + P::WaitRva), pair.object, receipt.nativeWaitResult, receipt.exceptionCode);
            // Nothing below reads the old handle, control or pending storage.
            // Wait owns its internal strong pins and callbacks may have
            // retired/reallocated all of those objects before returning.
            if (result != ClockResult::Ok)
                return finish(result);
            if ((result = D::Reacquire(image, owner, first, world)) != ClockResult::Ok ||
                (result = D::Capture(image, world.root, *initial, true)) != ClockResult::Ok)
                return finish(result);
            if (!D::SameOwner(*current, *initial))
                return finish(ClockResult::BindingChanged);
            ++receipt.drains;
            result = drain(reinterpret_cast<DrainFunction>(image + P::DrainRva), world.root, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            if ((result = D::Reacquire(image, owner, first, world)) != ClockResult::Ok ||
                (result = D::Capture(image, world.root, *current, true)) != ClockResult::Ok)
                return finish(result);
            // After an entered call even an emptied priority set must retain
            // the admitted manager identity; cleanup never restores it.
            if (!D::SameOwner(*initial, *current))
                return finish(ClockResult::BindingChanged);
        }
    }
}
