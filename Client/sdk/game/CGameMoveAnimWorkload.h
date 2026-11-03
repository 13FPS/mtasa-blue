/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional explicit physical validation of MTA's native movement path
 ****************************************************************************/
#pragma once
#include "CGameClockService.h"

namespace mta::game_de
{
    inline constexpr std::uint32_t MoveAnimWorkloadVersionV1 = 1;
    inline constexpr std::uint32_t MoveAnimWorkloadMaximumDurationMsV1 = 8000;
    enum class MoveAnimWorkloadPhaseV1 : std::uint32_t
    {
        Idle,
        Preflight,
        Installing,
        Published,
        Written,
        Reapplied,
        Observing,
        Withdrawing,
        Released,
        Completed,
        Refused,
        Unknown,
        Retired
    };
    struct MoveAnimWorkloadRequestV1
    {
        std::uint32_t size = sizeof(MoveAnimWorkloadRequestV1), version = MoveAnimWorkloadVersionV1;
        std::uint32_t durationMs = MoveAnimWorkloadMaximumDurationMsV1;
        // Only already-loaded 54/55/56 are candidates; no request/load is implied.
        std::int32_t  preferredGroup = 54;
        std::uint32_t reserved[4]{};
    };
    struct MoveAnimWorkloadStateV1
    {
        std::uint32_t           size = sizeof(MoveAnimWorkloadStateV1), version = MoveAnimWorkloadVersionV1;
        ClockServiceStateV1     owner;
        std::uint64_t           sequence = 0;
        MoveAnimWorkloadPhaseV1 phase = MoveAnimWorkloadPhaseV1::Idle;
        ClockResult             result = ClockResult::NotInitialized;
        std::uint32_t           selector = 0, handle = 0;
        std::int32_t            beforeGroup = 0, selectedGroup = 0, afterGroup = 0;
        std::uint32_t           tokenRegistered = 0, hooksEnabled = 0, overridePublished = 0;
        std::uint32_t           writeAcknowledged = 0, reapplyAcknowledged = 0;
        std::uint32_t           withdrawAttempted = 0, withdrawAcknowledged = 0, releaseAttempted = 0, releaseAcknowledged = 0;
        std::uint32_t           endAcknowledged = 0, cleanupPending = 0, cleanupViaOwnerEnd = 0, notEntered = 0;
        // An acknowledgement is native evidence, never the player's visual report.
        std::uint32_t physicalConfirmationRequired = 1, fullMtaReady = 0;
        std::uint32_t reserved[4]{};
    };
    static_assert(std::is_standard_layout_v<MoveAnimWorkloadRequestV1> && std::is_trivially_copyable_v<MoveAnimWorkloadRequestV1>);
    static_assert(std::is_standard_layout_v<MoveAnimWorkloadStateV1> && std::is_trivially_copyable_v<MoveAnimWorkloadStateV1>);

    // Core calls Begin only for an explicit local trial command, after the
    // menu/input borrowers retire. No package load, readiness probe or timer
    // activates this service. The old clock and observer reports stay unchanged.
    // Renew/Read belong to that same producer; End retains the owner and all
    // context until native hooks/references/debt actually retire. End with
    // generation0 is allowed only for this attempt's proven no-token refusal;
    // it is never a wildcard for another native owner. A verified
    // group write is intentional state and is never rolled back by this trial.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_MOVEANIM_WORKLOAD_EXPORT __declspec(dllexport)
#else
    #define MTA_MOVEANIM_WORKLOAD_EXPORT
#endif
    extern "C" MTA_MOVEANIM_WORKLOAD_EXPORT Status BeginGameMoveAnimWorkloadV1(std::uint32_t version, IGameAdapter*, const MoveAnimWorkloadRequestV1*,
                                                                               MoveAnimWorkloadStateV1*) noexcept;
    extern "C" MTA_MOVEANIM_WORKLOAD_EXPORT Status RenewGameMoveAnimWorkloadV1(std::uint32_t version, IGameAdapter*, std::uint64_t generation) noexcept;
    extern "C" MTA_MOVEANIM_WORKLOAD_EXPORT Status ReadGameMoveAnimWorkloadV1(std::uint32_t version, IGameAdapter*, MoveAnimWorkloadStateV1*) noexcept;
    extern "C" MTA_MOVEANIM_WORKLOAD_EXPORT Status EndGameMoveAnimWorkloadV1(std::uint32_t version, IGameAdapter*, std::uint64_t generation,
                                                                             MoveAnimWorkloadStateV1*) noexcept;
#undef MTA_MOVEANIM_WORKLOAD_EXPORT
}
