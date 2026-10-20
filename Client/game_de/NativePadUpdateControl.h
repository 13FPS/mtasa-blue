/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own the real global DE pad updater behind MTA's existing pad API
 *****************************************************************************/
#pragma once

#include "NativePadUpdateAccess.h"
#include <atomic>
#include <cstdint>

namespace mta::game_de
{
    struct NativePadUpdateState
    {
        std::uint64_t generation = 0;
        std::uint64_t calls = 0, forwarded = 0, suppressed = 0;
        std::uint32_t callbacks = 0, retainedPeers = 0;
        bool          bound = false, hooked = false, enabled = true, stopping = false;
        bool          ownedEntry = false, abandoned = false;
    };

    // One process-wide producer owns both native pads and their global source
    // update. This is deliberately independent of a per-player controls word.
    // The callback retains only pinned process-lifetime storage, never CGame,
    // Core or this producer's address. A false End keeps restoration debt alive.
    class NativePadUpdateControl final : public NativePadUpdateAccess
    {
    public:
        NativePadUpdateControl() = default;
        ~NativePadUpdateControl() override;
        NativePadUpdateControl(const NativePadUpdateControl&) = delete;
        NativePadUpdateControl& operator=(const NativePadUpdateControl&) = delete;

        bool Begin() noexcept;
        bool IsEnabled(bool& output) const noexcept override;
        bool SetDisabled(bool disabled) noexcept override;
        bool End() noexcept;
        void Read(NativePadUpdateState& output) const noexcept;

        // Explicit cleanup for an owner destroyed after a failed End. A new
        // producer cannot steal a live owner's generation. Shutdown may help
        // from another thread, but state mutations stay producer-thread bound.
        static bool RecoverAbandoned() noexcept;

    private:
        std::atomic<std::uint64_t> generation{0};
        NativePadUpdateState       finalState;
        bool                       hasFinalState = false;
    };
}
