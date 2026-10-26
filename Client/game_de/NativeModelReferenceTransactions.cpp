/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete native model-key acquisition through genuine map helpers
 ****************************************************************************/
#include "NativeModelReferenceTransactions.h"
#include "NativeModelReferenceHeap.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelBankProfile.h"
#include <windows.h>
namespace mta::game_de::NativeModelReferenceTransactions
{
    namespace
    {
        namespace P = NativeModelReferenceTransactionProfile;
        namespace M = NativeModelReferenceMap;
        namespace H = NativeModelReferenceHeap;
        namespace C = NativeStreamingModelContext;
        ClockResult NativeAcquire(const Calls& calls, std::uintptr_t map, std::uint32_t key, Receipt& receipt) noexcept
        {
            ReservedRow reserved;
            __try
            {
                // This reproduces the exact caller protocol with the genuine
                // allocator/publisher functions. No allocator growth, bit-set
                // creation or rehash implementation is copied into MTA.
                calls.reserve(map, &reserved);
                receipt.reserved = true;
                receipt.reservedIndex = reserved.index;
                if (reserved.row)
                {
                    *reinterpret_cast<std::uint32_t*>(reserved.row) = key;
                    *reinterpret_cast<std::uint32_t*>(reserved.row + 4) = 0;
                    *reinterpret_cast<std::int32_t*>(reserved.row + 8) = -1;
                }
                calls.publish(map, &receipt.publishedIndex, static_cast<std::int32_t>(key), reserved.row, reserved.index);
                receipt.published = true;
                // Publication has completed before any guarded return point.
                // Unexpected native outputs stop this operation; a reservation
                // or published key is never retried or silently rolled back.
                if (reserved.index < 0 || receipt.publishedIndex != reserved.index || receipt.publishedIndex >= NativeModelReferenceProfile::MaximumRows)
                    return ClockResult::BindingChanged;
                std::uintptr_t rows = 0;
                if (!M::Copy(map, &rows, sizeof(rows)))
                    return ClockResult::ReadFailure;
                if (!rows || static_cast<std::uintptr_t>(receipt.publishedIndex) >
                                 ((std::numeric_limits<std::uintptr_t>::max)() - rows) / NativeModelReferenceProfile::RowBytes)
                    return ClockResult::BindingChanged;
                receipt.row = rows + static_cast<std::uintptr_t>(receipt.publishedIndex) * NativeModelReferenceProfile::RowBytes;
                M::Row current;
                if (!M::Copy(receipt.row, &current, sizeof(current)))
                    return ClockResult::ReadFailure;
                if (current.key != key || current.count != 0)
                    return ClockResult::BindingChanged;
                receipt.before = 0;
                receipt.target = 1;
                static_assert(sizeof(LONG) == sizeof(std::uint32_t));
                const LONG old = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(receipt.row + 4), LONG{1}, LONG{0});
                std::memcpy(&receipt.observed, &old, sizeof(old));
                receipt.accepted = receipt.observed == 0;
                return receipt.accepted ? ClockResult::Ok : ClockResult::Busy;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                receipt.exceptionCode = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        bool HeapDisjoint(const M::Snapshot& map, const H::Value& heap) noexcept
        {
            if (map.rows.empty())
                return true;
            const auto bytes = map.rows.size() * NativeModelReferenceProfile::RowBytes;
            return M::Disjoint(map.rowsAddress, bytes, heap.authority, P::HeapBytes) &&
                   (heap.current == heap.authority || M::Disjoint(map.rowsAddress, bytes, heap.current, P::ProxyBytes));
        }
        bool PublishedOnlyTarget(const M::Snapshot& before, const M::Snapshot& after, const Receipt& receipt) noexcept
        {
            if (before.map != after.map || after.selected != receipt.publishedIndex || after.selected < 0 ||
                after.rows[static_cast<std::size_t>(after.selected)].count != 1 ||
                after.rows[static_cast<std::size_t>(after.selected)].key != receipt.modelIndex)
                return false;
            const auto freeBefore = M::Field<std::int32_t>(before, 0x34), freeAfter = M::Field<std::int32_t>(after, 0x34);
            if (after.rows.size() != before.rows.size() + (freeBefore ? 0 : 1) || freeAfter != freeBefore - (freeBefore ? 1 : 0))
                return false;
            const auto expected = freeBefore ? M::Field<std::int32_t>(before, 0x30) : static_cast<std::int32_t>(before.rows.size());
            if (receipt.reservedIndex != expected)
                return false;
            if (M::Field<std::int32_t>(after, 0xc) < M::Field<std::int32_t>(before, 0xc) ||
                M::Field<std::int32_t>(after, 0x2c) < M::Field<std::int32_t>(before, 0x2c) ||
                M::Field<std::int32_t>(after, 0x48) < M::Field<std::int32_t>(before, 0x48))
                return false;
            std::int32_t expectedHead = -1;
            if (freeBefore)
            {
                std::memcpy(&expectedHead, &before.rows[static_cast<std::size_t>(expected)].count, 4);
                if (after.rowsAddress != before.rowsAddress || after.bitsAddress != before.bitsAddress ||
                    M::Field<std::int32_t>(after, 0x2c) != M::Field<std::int32_t>(before, 0x2c))
                    return false;
            }
            else if (before.rows.size() < static_cast<std::size_t>(M::Field<std::int32_t>(before, 0xc)) && after.rowsAddress != before.rowsAddress)
                return false;
            if (M::Field<std::int32_t>(after, 0x30) != expectedHead)
                return false;
            for (std::size_t index = 0; index != before.rows.size(); ++index)
            {
                const bool was = M::Occupied(before, index), now = M::Occupied(after, index);
                if (index == static_cast<std::size_t>(after.selected))
                {
                    if (was || !now)
                        return false;
                }
                else if (was != now || (was && (before.rows[index].key != after.rows[index].key || before.rows[index].count != after.rows[index].count)))
                    return false;
                else if (!was)
                {
                    const auto previous = index == static_cast<std::size_t>(expectedHead) ? 0xffffffffu : before.rows[index].key;
                    if (after.rows[index].key != previous || after.rows[index].count != before.rows[index].count ||
                        after.rows[index].next != before.rows[index].next || after.rows[index].bucket != before.rows[index].bucket)
                        return false;
                }
            }
            return true;
        }
    }
    ClockResult Acquire(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner owner, Receipt* output, const Calls* overrideCalls,
                        bool requirePersistentOrigin) noexcept
    {
        Receipt receipt;
        receipt.modelIndex = modelIndex;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount || (overrideCalls && (!overrideCalls->reserve || !overrideCalls->publish)))
            return finish(ClockResult::InvalidArgument);
        // Upgrades acquire only their selected interface reference, as MTA
        // does; paired loading does not grant a second reference transaction.
        if (!owner)
            return finish(ClockResult::NotInitialized);
        try
        {
            NativeModelBankReader          bank({image}, owner);
            NativeModelLoadedReader        loaded({image}, owner);
            NativeModelBankReader::Value   model;
            NativeModelLoadedReader::Value state;
            auto                           result = bank.Read(modelIndex, model);
            if (result != ClockResult::Ok || (result = loaded.Read(modelIndex, state)) != ClockResult::Ok)
                return finish(result);
            if (!state.streamedIn || !model.nativeModelInfo)
                return finish(ClockResult::NotInitialized);
            if (!M::QualifyProfile(image))
                return finish(ClockResult::UnsupportedBuild);
            C::Value    context;
            H::Value    heap, confirmedHeap;
            M::Snapshot first, confirmed;
            if ((result = C::Capture(image, model, context)) != ClockResult::Ok || (result = M::Capture(context, model, first, false)) != ClockResult::Ok)
                return finish(result);
            receipt.root = context.root;
            receipt.map = first.map;
            receipt.origin = {context.root, first.map, context.rootMembership.index, context.rootMembership.serial};
            if (first.selected >= 0)
            {
                // Existing native row semantics, width and zero/wrap guards
                // stay in their real shared operation rather than a second
                // counter implementation added for the acquisition route.
                NativeModelReferenceMemory::Receipt adjusted;
                result = NativeModelReferenceMemory::Adjust(image, modelIndex, 1, owner, &adjusted, nullptr, nullptr, requirePersistentOrigin);
                // Nested Adjust owns the actual native entry. Do not retain
                // the earlier outer Context as though it acquired this ref.
                receipt.root = adjusted.root;
                receipt.map = adjusted.map;
                receipt.origin = adjusted.origin;
                receipt.row = adjusted.row;
                receipt.before = adjusted.before;
                receipt.target = adjusted.target;
                receipt.observed = adjusted.observed;
                receipt.exceptionCode = adjusted.exceptionCode;
                receipt.entered = adjusted.entered;
                receipt.accepted = adjusted.accepted;
                return finish(result);
            }
            // Durable capabilities require an already-issued nonzero weak
            // identity before native reserve. Never allocate one here; old
            // immediate callers deliberately keep their serial-zero domain.
            if (requirePersistentOrigin &&
                !NativeUObjectMembershipAccess::MatchesWeak(context.rootMembership, receipt.origin.rootIndex, receipt.origin.rootSerial))
                return finish(ClockResult::BindingChanged);
            // Existing rows require no allocator admission. Only a genuine
            // absent-key insertion depends on the current initialized heap.
            if (!H::QualifyProfile(image))
                return finish(ClockResult::UnsupportedBuild);
            if ((result = H::Capture(image, heap)) != ClockResult::Ok)
                return finish(result);
            if (!HeapDisjoint(first, heap))
                return finish(ClockResult::BindingChanged);
            receipt.heap = heap.current;
            if (first.rows.size() == static_cast<std::size_t>(NativeModelReferenceProfile::MaximumRows) && !M::Field<std::int32_t>(first, 0x34))
                return finish(ClockResult::UnsupportedBuild);
            if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = loaded.Recheck(state)) != ClockResult::Ok ||
                (result = C::Recheck(image, model, context)) != ClockResult::Ok || (result = H::Capture(image, confirmedHeap)) != ClockResult::Ok ||
                (result = M::Capture(context, model, confirmed, false)) != ClockResult::Ok)
                return finish(result);
            if (!M::Same(first, confirmed, false, 0) || !H::Same(heap, confirmedHeap) || !H::QualifyProfile(image))
                return finish(ClockResult::BindingChanged);
            if ((result = owner()) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_MODEL_REFERENCE_NATIVE_RESERVE)
            const auto reserve = &MTA_DE_MODEL_REFERENCE_NATIVE_RESERVE;
#else
            const auto reserve = reinterpret_cast<ReserveFunction>(image + P::ReserveRva);
#endif
#if defined(MTA_DE_MODEL_REFERENCE_NATIVE_PUBLISH)
            const auto publish = &MTA_DE_MODEL_REFERENCE_NATIVE_PUBLISH;
#else
            const auto publish = reinterpret_cast<PublishFunction>(image + P::PublishRva);
#endif
            const Calls calls = overrideCalls ? *overrideCalls : Calls{reserve, publish};
            receipt.entered = true;
            result = NativeAcquire(calls, first.map, modelIndex, receipt);
            if (result != ClockResult::Ok)
                return finish(result);
            if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = loaded.Recheck(state)) != ClockResult::Ok ||
                (result = C::Recheck(image, model, context)) != ClockResult::Ok || (result = H::Recheck(image, heap)) != ClockResult::Ok ||
                (result = M::Capture(context, model, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!HeapDisjoint(confirmed, heap) || !PublishedOnlyTarget(first, confirmed, receipt) || !H::QualifyProfile(image))
                return finish(ClockResult::BindingChanged);
            return finish(owner());
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
