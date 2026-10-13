/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Value-only boundary for a Core-owned configured-input diagnostic
 *****************************************************************************/

#pragma once

#include <array>
#include <cstdint>

namespace DefinitiveInputReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 64;
    inline constexpr std::uint32_t MaximumDurationMs = 60000;
    inline constexpr std::uint32_t SampleIntervalMs = 50;
    inline constexpr std::uint32_t MaximumSamples = 1200;
    inline constexpr std::uint32_t MaximumPulseGapMs = 1000;

    enum Field : std::uint32_t
    {
        Version,
        Phase,
        Sequence,
        ElapsedMs,
        Flags,  // Bit0 foreground process, Bit1 rearmed, Bit2 assumed on-foot context.
        SupportedBindings,
        IgnoredBindings,
        EligibleKeys,
        UnsupportedKeys,
        FocusLosses,
        Rearms,
        SampleCount,
        HeldActionsLow,
        HeldActionsHigh,
        Reason,
        InputCleanupComplete,
        Current = 16,
        Previous = 40,
    };

    enum class StopReason : std::uint32_t
    {
        Running,
        TimeBudget,
        Cancelled,
        ConfigError,
        NoSupportedKeys,
        AdapterError,
        PulseGap,
        SampleBudget,
        Exception,
        CleanupError,
    };

    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using CancelCallback = bool (*)();
    using PublishCallback = void (*)(const std::uint32_t*, std::uint32_t);

    // Both callbacks execute on InitializeCore's worker, outside loader lock.
    // The bridge copies values before return; it never holds Core pointers while
    // the owning worker tears down XML, binds, adapter or the loaded Core itself.
    struct Request
    {
        std::uint32_t   durationMs = 0;
        CancelCallback  cancelled = nullptr;
        PublishCallback publish = nullptr;
        bool            nativeMovement = false;
    };

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    bool Configure(const Request& request) noexcept;
    bool Activate() noexcept;
    bool GetActiveRequest(Request& request) noexcept;
    void Complete() noexcept;
#endif
}
