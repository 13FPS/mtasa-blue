/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain first native time-info defaults without retaining a borrow
 ****************************************************************************/
#pragma once
#include "NativeModelTimeMemory.h"
#include <atomic>
#include <map>
namespace mta::game_de
{
    class NativeModelTimeDomain final
    {
    public:
        using CheckOwner = NativeModelTimeMemory::CheckOwner;
        using Exchange = NativeModelTimeMemory::Exchange;
        ClockResult Set(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, std::uint16_t hours, CheckOwner, CheckOwner afterApplication,
                        bool& applied, Exchange = nullptr) noexcept;
        ClockResult ResetAll(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplication, Exchange = nullptr) noexcept;
        bool        Pending() const noexcept { return active.load() || pending.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }
        bool        CanDiscard() const noexcept { return !Pending(); }

    private:
        struct Saved
        {
            NativeModelTimeMemory::Origin origin;
            std::array<unsigned char, 4>  first{};
            std::uint32_t                 route = 0;
            std::uint16_t                 lastOwned = 0;
            bool                          restored = false;
        };
        struct Unknown
        {
            // These are plain intent/identity fields, never a retained Value,
            // UObject reader token or permission to reuse a prior callback.
            NativeModelTimeMemory::Origin origin;
            std::uint16_t                 desired = 0, observed = 0;
            std::uint32_t                 exceptionCode = 0;
            ClockResult                   result = ClockResult::NotInitialized;
            bool                          entered = false, accepted = false;
        };
        // Classic keys this cache by native time-info pointer. Bank model IDs
        // can alias one metadata row and therefore must share one first value.
        std::map<std::uintptr_t, Saved> originals;
        std::atomic_flag                operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>               active{false}, pending{false}, uncertain{false};
        Unknown                         firstUnknown;
        void                            Publish() noexcept;
        ClockResult Write(std::uintptr_t, const NativeModelTimeMemory::Value&, std::uint16_t, CheckOwner, CheckOwner, Exchange, bool& accepted) noexcept;
    };
}
