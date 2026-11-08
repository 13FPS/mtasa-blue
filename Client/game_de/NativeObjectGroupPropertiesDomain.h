/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain typed group defaults and acknowledged vector/reset prefixes
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
        ClockResult ReadRaw(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty, CheckOwner,
                            CheckOwner afterApplication, std::uint32_t&) noexcept;
        ClockResult ReadVector(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty, CheckOwner,
                               CheckOwner afterApplication, NativeObjectGroupVectorBits&) noexcept;
        ClockResult HasOriginal(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, CheckOwner, CheckOwner afterApplication, bool&) noexcept;
        ClockResult Set(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty, float, bool& modified, CheckOwner,
                        CheckOwner afterApplication) noexcept;
        ClockResult SetRaw(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty, std::uint32_t, bool& modified,
                           CheckOwner, CheckOwner afterApplication) noexcept;
        ClockResult SetVector(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty, const NativeObjectGroupVectorBits&,
                              bool& modified, CheckOwner, CheckOwner afterApplication) noexcept;
        ClockResult RestoreOne(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, bool& modified, CheckOwner,
                               CheckOwner afterApplication) noexcept;
        ClockResult RestoreAll(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplication) noexcept;
        bool        Pending() const noexcept { return active.load() || pending.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }
        bool        CanDiscard() const noexcept { return !Pending(); }

    private:
        enum class Kind : unsigned char
        {
            Float,
            Raw,
            Vector
        };
        struct Saved
        {
            NativeObjectGroupPropertiesMemory::Origin origin;
            NativeObjectGroupPropertiesMemory::Bits   bits{}, lastOwned{};
            std::uint32_t                             acknowledged = 0;
        };
        struct VectorChange
        {
            bool                        active = false;
            std::uint8_t                group = 0, acknowledged = 0;
            NativeObjectGroupProperty   first = NativeObjectGroupProperty::Count;
            NativeObjectGroupVectorBits desired{};
        };
        std::unordered_map<std::uint8_t, Saved>    originals;
        std::atomic_flag                           operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>                          pending{false}, active{false}, uncertain{false};
        bool                                       bulkInProgress = false;
        VectorChange                               vectorChange;
        NativeObjectGroupPropertiesMemory::Receipt firstUncertain;
        void                                       PublishPending() noexcept;
        ClockResult Write(std::uintptr_t image, const NativeObjectGroupPropertiesMemory::Value&, NativeObjectGroupProperty, std::uint32_t, CheckOwner,
                          CheckOwner afterApplication, bool& accepted) noexcept;
        ClockResult Change(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty, const NativeObjectGroupVectorBits&,
                           Kind, bool& modified, CheckOwner, CheckOwner afterApplication) noexcept;
        ClockResult Restore(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, Saved&, CheckOwner, CheckOwner afterApplication) noexcept;
    };
}
