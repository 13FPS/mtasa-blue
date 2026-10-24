/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain native sequence storage across failed publication and End
 ****************************************************************************/
#include "NativeCustomAnimSequenceController.h"
#include <algorithm>
#include <iterator>
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
    void NativeCustomAnimSequenceController::Publish() noexcept
    {
        retained.store(!entries.empty(), std::memory_order_release);
        bool needsRecovery = stopping && !entries.empty();
        for (const auto& entry : entries)
            needsRecovery = needsRecovery || (!entry.issued && entry.storage.HasOutstandingResource()) || entry.storage.GetState() == Storage::State::FaultDebt;
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
    ClockResult NativeCustomAnimSequenceController::Allocate(std::uintptr_t image, std::uint64_t generation, std::size_t capacity, CheckOwner owner,
                                                             std::uintptr_t& output, Receipt* receipt, InvokeMalloc invoke) noexcept
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
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        if (!capacity)
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
        result = NativeCustomAnimSequenceStorageCalls::Allocate(image, entry->storage, generation, capacity, owner, &local, invoke);
        if (result == ClockResult::Ok && local.readyTag)
            entry->issued = true;
        if (entry->storage.CanDiscard())
            entries.erase(entry);
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
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        Receipt    local;
        const auto result = NativeCustomAnimSequenceStorageCalls::FreeStorage(image, entry->storage, generation, owner, &local, invoke);
        // A returned native free is already consumed even if its post-call
        // acknowledgement failed. Erasing that released record cannot replay
        // free or infer anything by probing a possibly reused native address.
        if (entry->storage.CanDiscard())
            entries.erase(entry);
        Publish();
        if (receipt)
            *receipt = local;
        return result;
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
            if (entry->storage.CanDiscard())
            {
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
            result = NativeCustomAnimSequenceStorageCalls::FreeStorage(image, entry->storage, generation, owner, &receipt, invoke);
            if (entry->storage.CanDiscard())
            {
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
