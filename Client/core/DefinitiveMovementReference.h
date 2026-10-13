/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Core-owned configured movement trial value contract
 *****************************************************************************/

#pragma once

#include "DefinitiveInputReference.h"

namespace DefinitiveMovementReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 96;
    inline constexpr std::uint32_t MaximumDurationMs = 30000;
    inline constexpr std::uint32_t MaximumSamples = 600;
    inline constexpr std::uint32_t ReadinessBudgetMs = 10000;
    inline constexpr std::uint32_t CleanupDeadlineMs = 2000;
    inline constexpr std::uint32_t CleanupBackoffMs = 250;

    // Fields0..63 retain the configured-input contract, including signed
    // controller bits. Phase3 adds pending cleanup; Phase2 remains terminal.
    // The twelve existing native metrics occupy low/high uint32 pairs in their
    // established order, without truncating counters at this DLL boundary.
    enum Field : std::uint32_t
    {
        NativeMetrics = 64,
        ContextKnownMask = 88,
        ContextFlags,
        Eligible,
        WriterStarted,
        CleanupAttempts,
        CleanupDeadlineExceeded,
        BackendPinnedExpected,
        CleanupPending,
    };

    enum class StopReason : std::uint32_t
    {
        EligibilityError = 10,
        FocusLost,
        NativeFailure,
        ReadinessTimeout,
    };

    using Snapshot = std::array<std::uint32_t, FieldCount>;
}
