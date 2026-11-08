/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain MTA wheel defaults and entered uncertainty with one owner
 *****************************************************************************/
#pragma once
#include "NativeModelWheelMemory.h"
#include <atomic>
#include <unordered_map>

namespace mta::game_de
{
    class NativeModelWheelDomain final
    {
    public:
        using CheckOwner = NativeModelWheelMemory::CheckOwner;
        ClockResult Read(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication,
                         NativeModelWheelSizes&) noexcept;
        ClockResult Set(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, NativeModelWheelGroup group, float, CheckOwner,
                        CheckOwner afterApplication) noexcept;
        ClockResult ResetOne(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, const NativeModelWheelSizes* defaults, CheckOwner,
                             CheckOwner afterApplication) noexcept;
        ClockResult ResetAll(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplication) noexcept;
        // Producer/End paths observe only atomics. Engine-thread map iteration
        // and native writes remain confined to admitted normal callbacks.
        bool Pending() const noexcept { return active.load() || pending.load(); }
        bool Uncertain() const noexcept { return uncertain.load(); }
        bool CanDiscard() const noexcept { return !Pending(); }

    private:
        struct Saved
        {
            NativeModelWheelMemory::Origin origin;
            std::array<std::uint32_t, 2>   bits{};
            bool                           restored = false;
        };
        std::unordered_map<std::uint32_t, Saved> defaults, retired;
        std::atomic_flag                         operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>                        pending{false}, uncertain{false}, active{false};
        NativeModelWheelMemory::Receipt          firstUncertain;
        void                                     PublishPending() noexcept;
        ClockResult Write(std::uintptr_t image, const NativeModelWheelMemory::Value&, const std::array<std::uint32_t, 2>&, CheckOwner,
                          CheckOwner     afterApplication) noexcept;
    };
}
