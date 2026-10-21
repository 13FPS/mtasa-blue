/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked wanted reads and real native reactions in one callback scope
 *****************************************************************************/
#pragma once
#include "NativePlayerInfoMemory.h"
#include <cstdint>

namespace mta::game_de
{
    class NativeWantedMemory final
    {
    public:
        using CheckOwner = NativePlayerInfoMemory::CheckOwner;
        using NativeFunction = void (*)(void*, std::uint32_t);
        using Invoke = ClockResult (*)(NativeFunction, void*, std::uint32_t, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uint32_t requestedLevel = 0, exceptionCode = 0;
            bool          entered = false, returned = false, timestampRestored = false;
        };
        explicit NativeWantedMemory(const NativePlayerInfoMemory& player) noexcept : playerInfo(player) {}
        ClockResult Qualify(CheckOwner owner) const noexcept;
        ClockResult ReadLevel(std::uint32_t& output, CheckOwner owner) const noexcept;
        // The caller must stop its existing lease after any entered failure.
        // Native stats, crime queues and reactions can already have changed;
        // neither retries nor whole-object rollback implement this SDK call.
        ClockResult SetLevel(std::uint32_t level, bool noFlash, CheckOwner owner, Receipt* receipt = nullptr, Invoke invoke = nullptr) const noexcept;

    private:
        const NativePlayerInfoMemory& playerInfo;
    };
}
