/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve acknowledged cover-removal prefixes without native replay
 ****************************************************************************/
#pragma once
#include "NativeWorldCoverMemory.h"
#include "NativeWorldCoverCalls.h"
#include <atomic>
#include <memory>
namespace mta::game_de
{
    class NativeWorldCoverDomain final
    {
    public:
        using CheckOwner = NativeStreamingCompletionContext::CheckOwner;
        ClockResult Ready(std::uintptr_t, std::uint64_t, CheckOwner, CheckOwner) noexcept;
        ClockResult RemoveAll(std::uintptr_t, std::uint64_t, CheckOwner, CheckOwner, NativeWorldCoverCalls::Invoker = nullptr) noexcept;
        bool        Pending() const noexcept { return active.load() || pending.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }
        bool        CanDiscard() const noexcept { return !Pending(); }

    private:
        std::atomic_flag                               operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>                              active{false}, pending{false}, uncertain{false};
        std::uintptr_t                                 image = 0;
        std::uint64_t                                  generation = 0;
        std::size_t                                    acknowledged = 0;
        bool                                           entering = false;
        std::unique_ptr<NativeWorldCoverMemory::Value> expected;
    };
}
