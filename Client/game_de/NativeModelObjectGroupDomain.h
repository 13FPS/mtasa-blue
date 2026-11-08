/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep object-group defaults and acknowledged restore prefixes together
 ****************************************************************************/
#pragma once
#include "NativeModelObjectGroupMemory.h"
#include <atomic>
#include <unordered_map>

namespace mta::game_de
{
    class NativeModelObjectGroupDomain final
    {
    public:
        using CheckOwner = NativeModelObjectGroupMemory::CheckOwner;
        ClockResult Read(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication, std::uint16_t&) noexcept;
        ClockResult Set(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, std::uint16_t, CheckOwner, CheckOwner afterApplication) noexcept;
        ClockResult RestoreOne(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication) noexcept;
        ClockResult RestoreAll(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplication) noexcept;
        // End/producers inspect atomics only; native reads and cache iteration
        // remain under the same current normal engine-owner callback.
        bool Pending() const noexcept { return active.load() || pending.load(); }
        bool Uncertain() const noexcept { return uncertain.load(); }
        bool CanDiscard() const noexcept { return !Pending(); }

    private:
        struct Saved
        {
            NativeModelObjectGroupMemory::Origin origin;
            std::uint16_t                        group = 0;
            bool                                 restored = false;
        };
        std::unordered_map<std::uint32_t, Saved> originals;
        std::atomic_flag                         operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>                        pending{false}, uncertain{false}, active{false};
        NativeModelObjectGroupMemory::Receipt    firstUncertain;
        void                                     PublishPending() noexcept;
        ClockResult Write(std::uintptr_t image, const NativeModelObjectGroupMemory::Value&, std::uint16_t, CheckOwner, CheckOwner afterApplication) noexcept;
    };
}
