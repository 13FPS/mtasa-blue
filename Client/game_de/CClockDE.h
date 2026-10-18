/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the existing MTA clock interface over checked DE native access
 *****************************************************************************/
#pragma once

#include <windows.h>
#include <game/CClock.h>
#include <game/CGameClockService.h>
#include <atomic>
#include <cstdint>

namespace mta::game_de
{
    struct ClockValues
    {
        std::uint8_t  hour = 0, minute = 0;
        std::uint16_t seconds = 0;
        std::uint32_t nativeTimer = 0, lastMinuteChange = 0, millisecondsPerGameMinute = 0;
    };

    // Native ownership belongs to the qualified engine callback, not the
    // producer that constructs the adapter. Every access must check that owner
    // and its live binding before touching memory; failed reads preserve output.
    class NativeClockAccess
    {
    public:
        virtual ClockResult Read(ClockValues& output) noexcept = 0;
        virtual ClockResult Set(std::uint8_t hour, std::uint8_t minute) noexcept = 0;
        virtual ClockResult Freeze(bool value) noexcept = 0;
        virtual ClockResult Frozen(bool& output) const noexcept = 0;

    protected:
        ~NativeClockAccess() = default;
    };

    // The module owns this concrete object; the old CClock SDK deliberately has
    // no virtual destructor. Neither native failures nor unavailable services
    // may be hidden behind a synthetic or cached game clock.
    class CClockDE final : public CClock
    {
    public:
        explicit CClockDE(NativeClockAccess& native) noexcept : access(native) {}
        void Set(BYTE hour, BYTE minute) noexcept override;
        void Get(BYTE* hour, BYTE* minute) noexcept override;
        bool SetTimeFrozen(bool value) noexcept override;
        bool IsTimeFrozen() const noexcept override;
        bool ResetTimeFrozen() noexcept override;

        ClockResult TrySet(BYTE hour, BYTE minute) noexcept;
        ClockResult TryGet(BYTE* hour, BYTE* minute) noexcept;
        ClockResult TrySetTimeFrozen(bool value) noexcept;
        ClockResult TryIsTimeFrozen(bool& output) const noexcept;
        ClockResult TryResetTimeFrozen() noexcept;
        ClockResult LastResult() const noexcept { return lastResult.load(); }

    private:
        ClockResult                      Record(ClockResult result) const noexcept;
        NativeClockAccess&               access;
        mutable std::atomic<ClockResult> lastResult{ClockResult::NotInitialized};
    };
}
