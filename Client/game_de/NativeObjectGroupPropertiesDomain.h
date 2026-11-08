/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA modified state and retained first originals independently
 ****************************************************************************/
#pragma once
#include "NativeObjectGroupPropertiesMemory.h"
#include <atomic>
#include <unordered_map>

namespace mta::game_de
{
    class NativeObjectGroupPropertiesDomain final
    {
    public:
        using CheckOwner = NativeObjectGroupPropertiesMemory::CheckOwner;
        ClockResult Read(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty, CheckOwner, CheckOwner afterApplication,
                         float&) noexcept;
        ClockResult HasOriginal(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, CheckOwner, CheckOwner afterApplication, bool&) noexcept;
        ClockResult Set(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty, float, bool& modified, CheckOwner,
                        CheckOwner afterApplication) noexcept;
        ClockResult RestoreOne(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, bool& modified, CheckOwner,
                               CheckOwner afterApplication) noexcept;
        ClockResult RestoreAll(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplication) noexcept;
        bool        Pending() const noexcept { return active.load() || pending.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }
        bool        CanDiscard() const noexcept { return !Pending(); }

    private:
        struct Saved
        {
            NativeObjectGroupPropertiesMemory::Origin origin;
            NativeObjectGroupPropertiesMemory::Bits   bits{};
            std::uint8_t                              acknowledged = 0;
        };
        std::unordered_map<std::uint8_t, Saved>    originals;
        std::atomic_flag                           operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>                          pending{false}, active{false}, uncertain{false};
        bool                                       bulkInProgress = false;
        NativeObjectGroupPropertiesMemory::Receipt firstUncertain;
        void                                       PublishPending() noexcept;
        ClockResult Write(std::uintptr_t image, const NativeObjectGroupPropertiesMemory::Value&, NativeObjectGroupProperty, std::uint32_t, CheckOwner,
                          CheckOwner     afterApplication) noexcept;
        ClockResult Restore(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, Saved&, CheckOwner, CheckOwner afterApplication) noexcept;
    };
}
