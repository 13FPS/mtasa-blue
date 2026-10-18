/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional engine-thread access to MTA's existing clock interface
 *****************************************************************************/
#pragma once

#include "CGameAdapter.h"
#include <type_traits>

namespace mta::game_de
{
    inline constexpr std::uint32_t ClockServiceVersionV1 = 1;
    inline constexpr std::uint32_t ClockServiceLeaseMsV1 = 500;
    inline constexpr std::uint32_t ClockServiceMaximumDurationMsV1 = 30000;

    enum class ClockResult : std::uint32_t
    {
        Ok,
        InvalidArgument,
        NotInitialized,
        WrongThread,
        BindingChanged,
        ReadFailure,
        WriteFailure,
        RestoreFailure,
        PatchFailure,
        Busy,
        Expired,
        UnsupportedBuild,
        OwnerConflict,
        Stopped,
        OriginalException
    };

    enum class ClockOperation : std::uint32_t
    {
        Read,
        Set,
        Freeze,
        Unfreeze,
        ResetTimeFrozen
    };

    struct ClockServiceRequestV1
    {
        std::uint32_t size = sizeof(ClockServiceRequestV1), version = ClockServiceVersionV1;
        std::uint32_t maximumDurationMs = ClockServiceMaximumDurationMsV1;
        std::uint32_t reserved[3]{};
    };

    struct ClockCommandV1
    {
        std::uint32_t  size = sizeof(ClockCommandV1), version = ClockServiceVersionV1;
        std::uint64_t  generation = 0, sequence = 0;
        ClockOperation operation = ClockOperation::Read;
        std::uint32_t  hour = 0, minute = 0;
        std::uint32_t  reserved = 0;
    };

    struct ClockServiceStateV1
    {
        std::uint32_t size = sizeof(ClockServiceStateV1), version = ClockServiceVersionV1;
        std::uint64_t generation = 0;
        std::uint32_t producerThread = 0, engineThread = 0;
        std::uint32_t bound = 0, hooked = 0, active = 0, ready = 0;
        std::uint32_t stopping = 0, restored = 0, frozen = 0, modulePinned = 0;
        ClockResult   failure = ClockResult::Ok, lastCommandResult = ClockResult::NotInitialized;
        std::uint32_t hour = 0, minute = 0, seconds = 0, nativeTimer = 0, lastMinuteChange = 0;
        std::uint32_t millisecondsPerGameMinute = 0;
        std::uint64_t lastCompletedSequence = 0, pendingSequence = 0;
        std::uint64_t callbacks = 0, qualifiedCallbacks = 0, foreignCallbacks = 0, nestedCallbacks = 0;
        std::uint64_t reads = 0, sets = 0, freezes = 0, unfreezes = 0, resets = 0;
        std::uint64_t renewals = 0, restoreAttempts = 0, restoreFailures = 0;
        std::uint32_t inFlight = 0, patchInstalled = 0;
        std::uint32_t retainedPeerThreads = 0, patchRecoveryPending = 0, dataRecoveryPending = 0, protectionRestorePending = 0;
        std::uint32_t reserved[2]{};
    };
    static_assert(std::is_standard_layout_v<ClockServiceRequestV1> && std::is_trivially_copyable_v<ClockServiceRequestV1>);
    static_assert(std::is_standard_layout_v<ClockCommandV1> && std::is_trivially_copyable_v<ClockCommandV1>);
    static_assert(std::is_standard_layout_v<ClockServiceStateV1> && std::is_trivially_copyable_v<ClockServiceStateV1>);
    static_assert(sizeof(ClockServiceRequestV1) == 24);
    static_assert(sizeof(ClockCommandV1) == 40);
    static_assert(sizeof(ClockServiceStateV1) == 232);

    // The producer queues values; only the qualified native callback invokes
    // CClock. End can remain pending until that callback restores captured time
    // and sub-minute phase. Callers retain the adapter/module and retry End.
    // This opt-in service neither changes ABI6 nor advertises a usable CGame.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_CLOCK_SERVICE_EXPORT __declspec(dllexport)
#else
    #define MTA_CLOCK_SERVICE_EXPORT
#endif
    extern "C" MTA_CLOCK_SERVICE_EXPORT Status BeginGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, const ClockServiceRequestV1* request,
                                                                       ClockServiceStateV1* output) noexcept;
    extern "C" MTA_CLOCK_SERVICE_EXPORT Status SubmitGameClockCommandV1(std::uint32_t version, IGameAdapter* adapter, const ClockCommandV1* command) noexcept;
    extern "C" MTA_CLOCK_SERVICE_EXPORT Status RenewGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept;
    extern "C" MTA_CLOCK_SERVICE_EXPORT Status ReadGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, ClockServiceStateV1* output) noexcept;
    extern "C" MTA_CLOCK_SERVICE_EXPORT Status EndGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept;
#undef MTA_CLOCK_SERVICE_EXPORT
}
