/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bind MTA's frontend to its Core-owned scoped native input producer
 *****************************************************************************/
#pragma once
#include "DefinitiveGuiSurfaceReference.h"

namespace DefinitiveGuiFrontendInputReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 256;
    inline constexpr std::uint32_t MaximumDurationMs = 35000;
    inline constexpr std::uint32_t DisplayDurationMs = 20000;
    inline constexpr std::uint32_t NativeAcquireMs = 5000;
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using Outcome = DefinitiveGuiStartupReference::Outcome;
    using StopReason = DefinitiveGuiStartupReference::StopReason;

    // Existing surface/frontend/console schemas keep their own contracts. This
    // new layout proves a Core-owned neutral scope, never script control writes
    // or camera/gameplay readiness. No native pointer crosses this boundary.
    enum Field : std::uint32_t
    {
        Frontend = 96,
        Console = 128,
        InteractionVersion = 160,
        InteractionEnabled,
        InteractionFocused,
        FocusAcquired,
        FocusLost,
        InputEventsLow,
        InputEventsHigh,
        SyntheticReleasesLow,
        SyntheticReleasesHigh,
        PressedKeys,
        PressedButtons,
        InputDetached,
        InputFailed,
        GuiCloseRequested,
        InteractionReserved174,
        InteractionReserved175,
        Bindings = 176,
        NativeMetrics = 208,
        NativeModulePinned = 232,
        NativeReserved233,
        NativeReserved234,
        NativeReserved235,
        NativeStarted,
        NativeVerified,
        NativeEnded,
        NativeCleanupPending,
        NativeAcquireElapsed,
        NativeAcquireTimeout,
        NativeActiveMs,
        NativeOwnerRetained,
        CameraCaptureVerified,
        FullEngineInputReady,
        Reserved246,
    };

    inline void SetPair(Snapshot& values, Field field, std::uint64_t value) noexcept
    {
        values[field] = static_cast<std::uint32_t>(value);
        values[field + 1] = static_cast<std::uint32_t>(value >> 32);
    }

    template <typename Owner, typename Clock, typename Request>
    Snapshot Run(Owner& owner, Clock& clock, const Request& request) noexcept
    {
        return DefinitiveGuiStartupReference::Run<Snapshot>(owner, clock, request);
    }
}
