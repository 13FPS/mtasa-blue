/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Independent runtime worker protocol without changing old trials
 *****************************************************************************/
#pragma once

#include <array>
#include <cstdint>

namespace deonline::FrontendRuntimeReport
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t HeaderCount = 64;
    inline constexpr std::uint32_t CoreOffset = 64;
    inline constexpr std::uint32_t ViewOffset = 128;
    inline constexpr std::uint32_t FieldCount = 416;
    using Snapshot = std::array<std::uint32_t, FieldCount>;

    enum class Phase : std::uint32_t
    {
        Off,
        Starting,
        Hidden,
        Showing,
        Visible,
        Hiding,
        Stopping,
        Stopped,
        Failed,
        RetainedCleanup
    };
    enum class Command : std::uint32_t
    {
        None,
        Show,
        Hide,
        Stop
    };
    enum class Acknowledgement : std::uint32_t
    {
        None,
        Completed,
        Failed,
        Superseded
    };
    enum class CoreStage : std::uint32_t
    {
        None,
        Initial,
        BeforeShow,
        AfterHide,
        Stopped
    };
    enum class Failure : std::uint32_t
    {
        None,
        Exports,
        Begin,
        CoreRead,
        CoreState,
        View,
        ViewPublication,
        ViewRetirement,
        Shutdown,
        Release,
        Unload,
        Exception,
        HostExpired,
        CommandExhausted,
        ViewGenerationExhausted
    };
    enum class StopReason : std::uint32_t
    {
        None,
        Requested,
        HostExpired,
        ViewFault,
        LoaderFault,
        Cancelled
    };

    // All words are uint32. Core64 and view288 retain their own API1 meanings.
    // CoreStage identifies a real owner read, never a guessed live Core state
    // while that owner is blocked inside its view. Sequence is a uint64 pair.
    enum Field : std::uint32_t
    {
        Version,
        WorkerPhase,
        SequenceLow,
        SequenceHigh,
        ElapsedMs,
        SessionGenerationLow,
        SessionGenerationHigh,
        ScriptThread,
        WorkerThread,
        AcceptedSequence,
        AcceptedCommand,
        AcknowledgedSequence,
        AcknowledgedCommand,
        AcknowledgementCode,
        OutstandingSequence,
        OutstandingCommand,
        StopRequested,
        StopSequence,
        HostLeaseFresh,
        HostHeartbeatBudgetMs,
        ViewGenerationLow,
        ViewGenerationHigh,
        ViewAttempts,
        ViewsRetired,
        ShowAcknowledgements,
        HideAcknowledgements,
        CoreSampleStage,
        CoreSampleCount,
        LastCoreStatus,
        LastViewStatus,
        CoreLoaded,
        CoreOwnerRetained,
        BackendPinnedExpected,
        CleanupPending,
        CleanupComplete,
        CleanupDeadlineExceeded,
        LastFailure,
        SessionStopReason,
        ViewSamples,
        CoreStoppedVerified,
        CoreReleasedVerified,
        CoreUnloadedVerified,
        LegacyModulesBefore,
        LegacyModulesAfter,
        LastViewCleanupComplete,
        HostExpiryObserved,
        FocusHideCount,
        GuiHideCount,
        SupersededRequests,
        LastRetiredViewGeneration,
        NativeHeartbeatBudgetMs,
        CursorHeartbeatBudgetMs,
        ClockStarted,
        NetworkStarted,
        GameplayReady,
        FullMtaReady,
        Reserved56,
        Reserved57,
        Reserved58,
        Reserved59,
        Reserved60,
        Reserved61,
        Reserved62,
        Reserved63
    };
    static_assert(Reserved63 + 1 == HeaderCount);
}
