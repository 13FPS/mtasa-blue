/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Apply MTA's IPL itinerary with entered-prefix lifetime accounting
 ****************************************************************************/
#include "NativeWorldIplCalls.h"
#include "NativeWorldIplMemory.h"
#include "NativeWorldIplProfile.h"
#include <windows.h>
#include <algorithm>
#include <utility>

// The fixture may replace only this private memory dependency with explicit
// value doubles. Shipping always uses the full native snapshot implementation.
#if !defined(MTA_DE_WORLD_IPL_MEMORY_NAMESPACE)
    #define MTA_DE_WORLD_IPL_MEMORY_NAMESPACE NativeWorldIplMemory
#endif
#if !defined(MTA_DE_WORLD_IPL_REMOVE_NATIVE_INVOKE)
    #define MTA_DE_WORLD_IPL_REMOVE_NATIVE_INVOKE &RemoveNative
#endif
#if !defined(MTA_DE_WORLD_IPL_ADD_NATIVE_INVOKE)
    #define MTA_DE_WORLD_IPL_ADD_NATIVE_INVOKE &AddNative
#endif
#if !defined(MTA_DE_WORLD_IPL_CLEAR_NATIVE_INVOKE)
    #define MTA_DE_WORLD_IPL_CLEAR_NATIVE_INVOKE &ClearNative
#endif

namespace mta::game_de::NativeWorldIplCalls
{
    namespace M = MTA_DE_WORLD_IPL_MEMORY_NAMESPACE;
    namespace P = NativeWorldIplProfile;
    namespace C = NativeStreamingCompletionContext;
    namespace
    {
        class Active final
        {
        public:
            Active(std::atomic_flag& flag, std::atomic<bool>& busy) noexcept : flag(flag), busy(busy), held(!flag.test_and_set(std::memory_order_acquire))
            {
                if (held)
                    busy.store(true, std::memory_order_release);
            }
            ~Active()
            {
                if (held)
                {
                    busy.store(false, std::memory_order_release);
                    flag.clear(std::memory_order_release);
                }
            }
            explicit operator bool() const noexcept { return held; }

        private:
            std::atomic_flag&  flag;
            std::atomic<bool>& busy;
            bool               held;
        };
        ClockResult RemoveNative(RemoveFunction entry, std::int32_t index, std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(index);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult AddNative(AddFunction entry, void* tree, void* row, const void* bounds, std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(tree, row, bounds);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult ClearNative(ClearFunction entry, void* head, std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(head);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        std::vector<NativeWorldIplPlan::Node> Plans(const M::Snapshot& snapshot)
        {
            std::vector<NativeWorldIplPlan::Node> nodes;
            nodes.reserve(snapshot.nodes.size());
            for (const auto& node : snapshot.nodes)
                nodes.push_back(node.plan);
            return nodes;
        }
        ClockResult Budget(const M::Snapshot& snapshot, const std::vector<std::uint32_t>& selected, std::size_t first, NativeWorldIplPlan::Budget& output)
        {
            if (!snapshot.tree)
            {
                output = {};
                return ClockResult::Ok;
            }
            std::vector<NativeWorldIplPlan::Rect> bounds;
            for (auto at = first; at != selected.size(); ++at)
            {
                const M::Row* row = nullptr;
                if (!M::ReadRow(snapshot, selected[at], row))
                    return ClockResult::BindingChanged;
                bounds.push_back(row->bounds);
            }
            return NativeWorldIplPlan::Require(Plans(snapshot), bounds, snapshot.quad.free, snapshot.links.free, output);
        }
        bool Shape(const M::Snapshot& a, const M::Snapshot& b) noexcept
        {
            if (a.nodes.size() != b.nodes.size() || a.quad.states != b.quad.states)
                return false;
            for (std::size_t at = 0; at != a.nodes.size(); ++at)
            {
                const auto& x = a.nodes[at];
                const auto& y = b.nodes[at];
                if (x.address != y.address || x.plan.level != y.plan.level || x.plan.bounds != y.plan.bounds || x.plan.child != y.plan.child)
                    return false;
            }
            return true;
        }
        std::size_t Memberships(const M::Snapshot& snapshot, std::uintptr_t row) noexcept
        {
            return static_cast<std::size_t>(std::count_if(snapshot.list.begin(), snapshot.list.end(), [row](const auto& link) { return link.item == row; }));
        }
        ClockResult FinalOwner(std::uintptr_t image, CheckOwner owner, CheckOwner afterApplicationOwner, const M::Snapshot& snapshot)
        {
            auto result = afterApplicationOwner();
            if (result != ClockResult::Ok || (result = C::Owner(image, owner)) != ClockResult::Ok)
                return result;
            return M::Seal(image, snapshot) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
    }
    ClockResult Controller::Bind(std::uint64_t requested) noexcept
    {
        if (!requested)
            return ClockResult::InvalidArgument;
        if (generation == requested)
            return phase == Phase::Unknown ? ClockResult::OwnerConflict : ClockResult::Ok;
        if (retained.load(std::memory_order_acquire))
            return ClockResult::OwnerConflict;
        generation = requested;
        phase = Phase::Enabled;
        return ClockResult::Ok;
    }
    ClockResult Controller::Ready(std::uintptr_t image, std::uint64_t requested, CheckOwner owner, CheckOwner afterApplicationOwner) noexcept
    {
        Active operation(lock, active);
        if (!operation)
            return ClockResult::Busy;
        auto result = Bind(requested);
        if (result != ClockResult::Ok)
            return result;
        if (!C::TryEnter())
            return ClockResult::Busy;
        C::Scope scope;
        try
        {
            M::Snapshot snapshot;
            // This is profile/current-bank readiness only. Disable separately
            // qualifies every selected loaded entity/UE graph before its first
            // effect; pending states still refuse the whole bank. Enable does
            // not enter the native entity-destruction branch.
            return M::Capture(image, owner, afterApplicationOwner, snapshot, false);
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    ClockResult Controller::Run(std::uintptr_t image, std::uint64_t requested, CheckOwner owner, CheckOwner afterApplicationOwner, bool enabled,
                                IplFilter filter, Receipt* output, const Invocations* supplied) noexcept
    {
        Receipt receipt;
        receipt.generation = requested;
        Active operation(lock, active);
        if (!operation)
        {
            receipt.result = ClockResult::Busy;
            if (output)
                *output = receipt;
            return receipt.result;
        }
        receipt.before = phase;
        bool       accepted = false;
        const auto finish = [&](ClockResult result)
        {
            if (result != ClockResult::Ok && accepted)
            {
                phase = Phase::Unknown;
                retained.store(true, std::memory_order_release);
            }
            receipt.after = phase;
            receipt.result = result;
            receipt.retained = retained.load(std::memory_order_acquire);
            receipt.unknown = phase == Phase::Unknown;
            if (output)
                *output = receipt;
            return result;
        };
        auto result = Bind(requested);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.before = phase;
        if (!owner || !afterApplicationOwner)
            return finish(ClockResult::NotInitialized);
        if (enabled && !filter)
            return finish(ClockResult::InvalidArgument);
        if ((enabled && phase == Phase::Enabled) || (!enabled && phase == Phase::Disabled))
        {
            result = afterApplicationOwner();
            return finish(result != ClockResult::Ok ? result : C::Owner(image, owner));
        }
        if (!C::TryEnter())
            return finish(ClockResult::Busy);
        C::Scope scope;
        try
        {
            M::Snapshot before;
            if ((result = M::Capture(image, owner, afterApplicationOwner, before, !enabled)) != ClockResult::Ok)
                return finish(result);
            std::vector<std::uint32_t> selected;
            for (const auto& row : before.rows)
            {
                if (!row.index)
                    continue;
                if (enabled && row.bytes[P::DisabledOffset] != 1)
                    return finish(ClockResult::BindingChanged);
                const auto name = row.name;
                if (!enabled || filter(name))
                    selected.push_back(row.index);
            }
            receipt.selected = static_cast<std::uint32_t>(selected.size());
            // Collect every callback decision before the first native prefix.
            // Reentry is Busy, and raw edits made by a callback must fail this
            // exact seal rather than becoming a renewed operation snapshot.
            if ((result = FinalOwner(image, owner, afterApplicationOwner, before)) != ClockResult::Ok)
                return finish(result);
            if (enabled && !before.list.empty())
                return finish(ClockResult::BindingChanged);
            NativeWorldIplPlan::Budget all;
            if (enabled && (result = Budget(before, selected, 0, all)) != ClockResult::Ok)
                return finish(result);
            // Owner fixtures may replace only these final entry wrappers so
            // the actual Runtime/Memory/Controller lifetime path still runs.
            // Shipping defaults always execute the qualified native bodies.
            const auto invokeRemove = supplied && supplied->remove ? supplied->remove : MTA_DE_WORLD_IPL_REMOVE_NATIVE_INVOKE;
            const auto invokeAdd = supplied && supplied->add ? supplied->add : MTA_DE_WORLD_IPL_ADD_NATIVE_INVOKE;
            const auto invokeClear = supplied && supplied->clear ? supplied->clear : MTA_DE_WORLD_IPL_CLEAR_NATIVE_INVOKE;
            auto       enter = [&]
            {
                accepted = true;
                phase = Phase::Unknown;
                retained.store(true, std::memory_order_release);
                ++receipt.entered;
            };
            for (std::size_t at = 0; at != selected.size(); ++at)
            {
                M::Snapshot current;
                if ((result = M::Capture(image, owner, afterApplicationOwner, current, !enabled)) != ClockResult::Ok)
                    return finish(result);
                if (!M::SameIdentity(before, current) || (!enabled && !Shape(before, current)))
                    return finish(ClockResult::BindingChanged);
                const M::Row* row = nullptr;
                if (!M::ReadRow(current, selected[at], row))
                    return finish(ClockResult::BindingChanged);
                if ((result = FinalOwner(image, owner, afterApplicationOwner, current)) != ClockResult::Ok)
                    return finish(result);
                if (!enabled)
                {
                    enter();
                    result =
                        invokeRemove(reinterpret_cast<RemoveFunction>(image + P::RemoveAndIgnoreRva), static_cast<std::int32_t>(row->index), receipt.exception);
                    if (result != ClockResult::Ok)
                        return finish(result);
                    ++receipt.returned;
                    M::Snapshot after;
                    if ((result = M::Capture(image, owner, afterApplicationOwner, after, true)) != ClockResult::Ok)
                        return finish(result);
                    const M::Row* changed = nullptr;
                    if (!M::SameIdentity(before, after) || !Shape(before, after) || !M::ReadRow(after, row->index, changed) ||
                        changed->bytes[P::DisabledOffset] != 1 || changed->bytes[P::DisabledOffset + 1] != 0 ||
                        (row->streamState == 1 &&
                         (changed->streamState != 0 || changed->bytes[0x2d] != 0 || !NativeWorldIplLoadedMemory::Removed(image, current.loaded, row->index))))
                        return finish(ClockResult::BindingChanged);
                    ++receipt.removeAcks;
                    continue;
                }
                NativeWorldIplPlan::Budget remaining;
                if ((result = Budget(current, selected, at, remaining)) != ClockResult::Ok)
                    return finish(result);
                enter();
                if (!M::StoreEnabled(*row))
                    return finish(ClockResult::WriteFailure);
                ++receipt.flagStores;
                M::Snapshot cleared;
                if ((result = M::Capture(image, owner, afterApplicationOwner, cleared, false)) != ClockResult::Ok)
                    return finish(result);
                const M::Row* enabledRow = nullptr;
                if (!M::SameIdentity(before, cleared) || !M::ReadRow(cleared, row->index, enabledRow) || enabledRow->bytes[P::DisabledOffset] != 0)
                    return finish(ClockResult::BindingChanged);
                if (!cleared.tree)
                    continue;
                NativeWorldIplPlan::Budget       one;
                const std::vector<std::uint32_t> only{row->index};
                if ((result = Budget(cleared, only, 0, one)) != ClockResult::Ok ||
                    (result = FinalOwner(image, owner, afterApplicationOwner, cleared)) != ClockResult::Ok)
                    return finish(result);
                const auto oldMembers = Memberships(cleared, row->address);
                enter();
                result = invokeAdd(reinterpret_cast<AddFunction>(image + P::QuadAddRva), reinterpret_cast<void*>(cleared.tree),
                                   reinterpret_cast<void*>(enabledRow->address), reinterpret_cast<const void*>(enabledRow->address), receipt.exception);
                if (result != ClockResult::Ok)
                    return finish(result);
                ++receipt.returned;
                M::Snapshot after;
                if ((result = M::Capture(image, owner, afterApplicationOwner, after, false)) != ClockResult::Ok)
                    return finish(result);
                if (!M::SameIdentity(before, after) || after.quad.free > cleared.quad.free || after.links.free > cleared.links.free ||
                    cleared.quad.free - after.quad.free != one.quadNodes || cleared.links.free - after.links.free != one.links ||
                    Memberships(after, enabledRow->address) != oldMembers + one.links)
                    return finish(ClockResult::BindingChanged);
                ++receipt.addAcks;
            }
            if (!enabled)
            {
                // Match MTA's recursive clear order over the actual DE tree.
                // Missing DE lazy children are empty; native leaf clear owns
                // every list-node release and allocator cursor update.
                for (const auto& node : before.nodes)
                {
                    if (node.plan.level || !node.head)
                        continue;
                    M::Snapshot current;
                    if ((result = M::Capture(image, owner, afterApplicationOwner, current, true)) != ClockResult::Ok)
                        return finish(result);
                    if (!M::SameIdentity(before, current) || !Shape(before, current))
                        return finish(ClockResult::BindingChanged);
                    const auto found = std::find_if(current.nodes.begin(), current.nodes.end(), [&](const auto& item) { return item.address == node.address; });
                    if (found == current.nodes.end())
                        return finish(ClockResult::BindingChanged);
                    if (!found->head)
                        continue;
                    if ((result = FinalOwner(image, owner, afterApplicationOwner, current)) != ClockResult::Ok)
                        return finish(result);
                    enter();
                    result = invokeClear(reinterpret_cast<ClearFunction>(image + P::SingleClearRva), reinterpret_cast<void*>(node.address + P::QuadListOffset),
                                         receipt.exception);
                    if (result != ClockResult::Ok)
                        return finish(result);
                    ++receipt.returned;
                    M::Snapshot after;
                    if ((result = M::Capture(image, owner, afterApplicationOwner, after, true)) != ClockResult::Ok)
                        return finish(result);
                    const auto clean = std::find_if(after.nodes.begin(), after.nodes.end(), [&](const auto& item) { return item.address == node.address; });
                    if (!M::SameIdentity(before, after) || !Shape(before, after) || clean == after.nodes.end() || clean->head)
                        return finish(ClockResult::BindingChanged);
                    ++receipt.clearAcks;
                }
            }
            M::Snapshot final;
            if ((result = M::Capture(image, owner, afterApplicationOwner, final, !enabled)) != ClockResult::Ok)
                return finish(result);
            if (!M::SameIdentity(before, final) || (!enabled && !final.list.empty()))
                return finish(ClockResult::BindingChanged);
            if ((result = FinalOwner(image, owner, afterApplicationOwner, final)) != ClockResult::Ok)
                return finish(result);
            phase = enabled ? Phase::Enabled : Phase::Disabled;
            retained.store(!enabled, std::memory_order_release);
            return finish(ClockResult::Ok);
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
    ClockResult Controller::Disable(std::uintptr_t image, std::uint64_t requested, CheckOwner owner, CheckOwner afterApplicationOwner, Receipt* receipt,
                                    const Invocations* invokes) noexcept
    {
        return Run(image, requested, owner, afterApplicationOwner, false, nullptr, receipt, invokes);
    }
    ClockResult Controller::Enable(std::uintptr_t image, std::uint64_t requested, CheckOwner owner, CheckOwner afterApplicationOwner, IplFilter filter,
                                   Receipt* receipt, const Invocations* invokes) noexcept
    {
        return Run(image, requested, owner, afterApplicationOwner, true, filter, receipt, invokes);
    }
    ClockResult Controller::End(std::uint64_t requested) noexcept
    {
        Active operation(lock, active);
        if (!operation)
            return ClockResult::Busy;
        if (!generation)
            return ClockResult::Ok;
        if (!requested || generation != requested)
            return ClockResult::BindingChanged;
        if (retained.load(std::memory_order_acquire))
            return ClockResult::Busy;
        generation = 0;
        phase = Phase::Unbound;
        return ClockResult::Ok;
    }
}
