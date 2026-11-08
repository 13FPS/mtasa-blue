/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Acknowledge one native unlink without replaying an unknown effect
 ****************************************************************************/
#include "NativeWorldRemoveDomain.h"
#include <memory>

namespace mta::game_de
{
    namespace WorldRemoveDomainDetail
    {
        struct ActiveRelease
        {
            std::atomic<bool>& active;
            ~ActiveRelease() { active.store(false); }
        };

        ClockResult Owner(NativeWorldRemoveDomain::CheckOwner owner, NativeWorldRemoveDomain::CheckOwner application) noexcept
        {
            if (!owner || !application)
                return ClockResult::InvalidArgument;
            const auto result = owner();
            return result == ClockResult::Ok ? application() : result;
        }
    }

    void NativeWorldRemoveDomain::Clear() noexcept
    {
        image = 0;
        generation = 0;
        kind = {};
        index = 0;
        identity.clear();
        receipt = {};
        failure = ClockResult::NotInitialized;
        retained.store(false);
    }

    ClockResult NativeWorldRemoveDomain::Unknown(ClockResult result) noexcept
    {
        failure = result == ClockResult::Ok ? ClockResult::BindingChanged : result;
        uncertain.store(true);
        retained.store(true);
        return failure;
    }

    ClockResult NativeWorldRemoveDomain::Ready(std::uintptr_t selectedImage, std::uint64_t selectedGeneration, Kind selectedKind, std::uint32_t selectedIndex,
                                               CheckOwner owner, CheckOwner application) noexcept
    {
        if (active.exchange(true))
            return ClockResult::OwnerConflict;
        WorldRemoveDomainDetail::ActiveRelease release{active};
        if (retained.load())
            return ClockResult::OwnerConflict;
        try
        {
            auto value = std::make_unique<NativeWorldRemoveMemory::Value>();
            // Capture qualifies the shared genuine rectangle inputs. It
            // grants no future entry permission or complete world membership.
            return NativeWorldRemoveMemory::Capture(selectedImage, selectedGeneration, selectedKind, selectedIndex, owner, application, *value);
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }

    ClockResult NativeWorldRemoveDomain::Remove(std::uintptr_t selectedImage, std::uint64_t selectedGeneration, Kind selectedKind, std::uint32_t selectedIndex,
                                                CheckOwner owner, CheckOwner application, NativeWorldRemoveCalls::Invoker invoker) noexcept
    {
        if (active.exchange(true))
            return ClockResult::OwnerConflict;
        WorldRemoveDomainDetail::ActiveRelease release{active};
        if (retained.load())
            return ClockResult::OwnerConflict;
        bool dispatching = false;
        try
        {
            auto value = std::make_unique<NativeWorldRemoveMemory::Value>();
            auto result = NativeWorldRemoveMemory::Capture(selectedImage, selectedGeneration, selectedKind, selectedIndex, owner, application, *value);
            if (result != ClockResult::Ok)
                return result;

            // Prepare durable CPU intent before native entry. The current raw
            // admission snapshot remains local and is never reused as a lease.
            identity = value->identity;
            image = selectedImage;
            generation = selectedGeneration;
            kind = selectedKind;
            index = selectedIndex;
            receipt = {};
            retained.store(true);
            result = WorldRemoveDomainDetail::Owner(owner, application);
            if (result != ClockResult::Ok)
            {
                Clear();
                return result;
            }
            dispatching = true;
            result = NativeWorldRemoveCalls::Invoke(selectedImage, *value, receipt, invoker);
            dispatching = false;
            if (!receipt.entered)
            {
                Clear();
                return result == ClockResult::Ok ? ClockResult::ReadFailure : result;
            }
            if (result != ClockResult::Ok || !receipt.returned)
                return Unknown(result);

            result = WorldRemoveDomainDetail::Owner(owner, application);
            if (result != ClockResult::Ok)
                return Unknown(result);
            if (!NativeWorldRemoveMemory::Removed(selectedImage, *value))
                return Unknown(ClockResult::BindingChanged);

            // The SDK's enclosing world policy owns any eventual restoration.
            // Acknowledging this effect does not qualify freeing its pool row.
            Clear();
            return ClockResult::Ok;
        }
        catch (...)
        {
            if (dispatching || receipt.entered)
                return Unknown(ClockResult::ReadFailure);
            Clear();
            return ClockResult::ReadFailure;
        }
    }
}
