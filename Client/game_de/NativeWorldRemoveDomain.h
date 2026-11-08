/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep entered-unknown world unlink intent with its native owner
 ****************************************************************************/
#pragma once
#include "NativeWorldRemoveMemory.h"
#include "NativeWorldRemoveCalls.h"
#include <atomic>
#include <vector>

namespace mta::game_de
{
    class NativeWorldRemoveDomain final
    {
    public:
        using Kind = NativeWorldRemoveMemory::Kind;
        using CheckOwner = NativeStreamingCompletionContext::CheckOwner;
        ClockResult Ready(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, CheckOwner, CheckOwner afterApplication) noexcept;
        ClockResult Remove(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, CheckOwner, CheckOwner afterApplication,
                           NativeWorldRemoveCalls::Invoker = nullptr) noexcept;
        bool        Pending() const noexcept { return active.load() || retained.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }
        bool        CanDiscard() const noexcept { return !Pending(); }

    private:
        // The gate itself publishes in-flight use before any borrow or callback.
        // An unknown entry retains only CPU intent, never a stale native lease.
        std::atomic<bool>               active{false}, retained{false}, uncertain{false};
        std::uintptr_t                  image = 0;
        std::uint64_t                   generation = 0;
        Kind                            kind{};
        std::uint32_t                   index = 0;
        std::vector<std::uintptr_t>     identity;
        NativeWorldRemoveCalls::Receipt receipt;
        ClockResult                     failure = ClockResult::NotInitialized;
        void                            Clear() noexcept;
        ClockResult                     Unknown(ClockResult) noexcept;
    };
}
