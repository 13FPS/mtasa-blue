/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Separate real-menu trial and script-owned engine-control handshake
 *****************************************************************************/
#pragma once
#include "DefinitiveGuiSurfaceReference.h"

namespace DefinitiveGuiFrontendReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 160;
    inline constexpr std::uint32_t MaximumDurationMs = 40000;
    inline constexpr std::uint32_t DisplayDurationMs = 30000;
    inline constexpr std::uint32_t ControlAcquireMs = 5000;
    inline constexpr std::uint32_t HeartbeatExpiryMs = 1000;
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using Outcome = DefinitiveGuiStartupReference::Outcome;
    using StopReason = DefinitiveGuiStartupReference::StopReason;

    // The first 96 positions share ownership facts with the passive surface,
    // but this distinct protocol validates real menu output, not fixture colors.
    // 96..127 contain the unchanged DefinitiveFrontend API1 snapshot.
    enum Field : std::uint32_t
    {
        Frontend = 96,
        InteractionVersion = 128,
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
        ControlGeneration,
        ControlRequested,
        ControlAcknowledged,
        ControlFresh,
        ControlRevoked,
        ControlInputDetached,
        ControlRestored,
        ControlFault,
        ControlWaitingMs,
        ControlExpiryMs,
        ControlTimeout,
        ActiveDuration,
        FrontendStarted,
        FrontendStopped,
        CameraCaptureVerified,
        FullEngineInputReady,
        Reserved158,
        Reserved159,
    };

    enum class ControlAction : std::uint32_t
    {
        Request = 1,
        Poll = 2,
        InputDetached = 3
    };
    enum ControlField : std::uint32_t
    {
        Generation,
        Requested,
        Acknowledged,
        Fresh,
        Revoked,
        Detached,
        Restored,
        Fault,
        ControlFieldCount
    };
    using ControlSnapshot = std::array<std::uint32_t, ControlFieldCount>;
    // This callback exchanges only copied scalars. Only the script thread may
    // invoke the existing engine commands, after checking the player's identity.
    using ControlCallback = bool (*)(std::uint32_t, std::uint32_t*, std::uint32_t);

    // Keep the new counters typed to this protocol; the common ownership
    // helper accepts its own enum and cannot represent these added positions.
    template <std::size_t Count>
    inline void SetPair(std::array<std::uint32_t, Count>& values, Field field, std::uint64_t value) noexcept
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
