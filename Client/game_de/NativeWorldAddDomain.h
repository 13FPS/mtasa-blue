/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain unknown WorldAdd effects without retaining a native lease
 ****************************************************************************/
#pragma once
#include "NativeWorldAddEntryCalls.h"
#include <atomic>

namespace mta::game_de
{
    class NativeWorldAddDomain final
    {
    public:
        using Kind = NativeWorldAddEntryCalls::Kind;
        using CheckOwner = NativeWorldAddEntryCalls::CheckOwner;
        ClockResult Ready(std::uintptr_t, std::uint64_t, Kind, std::uint32_t, CheckOwner, CheckOwner) noexcept;
        ClockResult Add(std::uintptr_t, std::uint64_t, Kind, std::uint32_t, CheckOwner, CheckOwner, NativeWorldAddEntryCalls::Invoker = nullptr,
                        NativeWorldBoundRectCalls::Invoker = nullptr) noexcept;
        bool        Pending() const noexcept { return active.load() || retained.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }
        bool        CanDiscard() const noexcept { return !Pending(); }

    private:
        // Publish in-flight use before borrowing. Retained intent is plain CPU
        // data; a captured world/model/matrix Value never outlives its callback.
        std::atomic<bool>                 active{false}, retained{false}, uncertain{false};
        std::uintptr_t                    image = 0;
        std::uint64_t                     generation = 0;
        Kind                              kind{};
        std::uint32_t                     index = 0;
        NativeWorldAddEntryCalls::Receipt receipt;
        ClockResult                       failure = ClockResult::NotInitialized;
        void                              Clear() noexcept;
    };
}
