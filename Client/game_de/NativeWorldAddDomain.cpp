/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep a native add acknowledgement with the existing runtime owner
 ****************************************************************************/
#include "NativeWorldAddDomain.h"
#include <memory>

namespace mta::game_de
{
    namespace WorldAddDomainDetail
    {
        struct ActiveRelease
        {
            std::atomic<bool>& active;
            ~ActiveRelease() { active.store(false); }
        };
    }
    void NativeWorldAddDomain::Clear() noexcept
    {
        image = 0;
        generation = 0;
        kind = {};
        index = 0;
        receipt = {};
        failure = ClockResult::NotInitialized;
        retained.store(false);
    }
    ClockResult NativeWorldAddDomain::Ready(std::uintptr_t selectedImage, std::uint64_t selectedGeneration, Kind selectedKind, std::uint32_t selectedIndex,
                                            CheckOwner owner, CheckOwner after) noexcept
    {
        if (active.exchange(true))
            return ClockResult::OwnerConflict;
        WorldAddDomainDetail::ActiveRelease release{active};
        if (retained.load())
            return ClockResult::OwnerConflict;
        try
        {
            auto current = std::make_unique<NativeWorldAddEntryMemory::Value>();
            // This executes the qualified read-only producer and checks the
            // current budget. It grants no future entry or membership lease.
            const auto result = NativeWorldAddEntryMemory::Capture(selectedImage, selectedGeneration, selectedKind, selectedIndex, owner, after, *current);
            return result != ClockResult::Ok                                  ? result
                   : NativeWorldAddEntryMemory::Seal(selectedImage, *current) ? ClockResult::Ok
                                                                              : ClockResult::BindingChanged;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeWorldAddDomain::Add(std::uintptr_t selectedImage, std::uint64_t selectedGeneration, Kind selectedKind, std::uint32_t selectedIndex,
                                          CheckOwner owner, CheckOwner after, NativeWorldAddEntryCalls::Invoker invoke,
                                          NativeWorldBoundRectCalls::Invoker read) noexcept
    {
        if (active.exchange(true))
            return ClockResult::OwnerConflict;
        WorldAddDomainDetail::ActiveRelease release{active};
        if (retained.load())
            return ClockResult::OwnerConflict;
        image = selectedImage;
        generation = selectedGeneration;
        kind = selectedKind;
        index = selectedIndex;
        receipt = {};
        retained.store(true);
        const auto result = NativeWorldAddEntryCalls::Add(selectedImage, selectedGeneration, selectedKind, selectedIndex, owner, after, &receipt, invoke, read);
        if (!receipt.entered)
        {
            // A read-only rectangle can finish before capacity/source refusal.
            // With no mutating entry there is no world effect to retain.
            Clear();
            return result == ClockResult::Ok ? ClockResult::ReadFailure : result;
        }
        if (result == ClockResult::Ok && receipt.returned && receipt.acknowledged && !receipt.uncertain)
        {
            Clear();
            return ClockResult::Ok;
        }
        failure = result == ClockResult::Ok ? ClockResult::BindingChanged : result;
        uncertain.store(true);
        // No destructor/End retry can resolve an entered unknown add. The
        // enclosing MTA pool owner must still prove absence/restoration itself.
        return failure;
    }
}
