/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional value-only observations of native startup prerequisites
 *****************************************************************************/
#pragma once
#include "CGameApplicationFrameObservation.h"

namespace mta::game_de
{
    inline constexpr std::uint32_t StartupObservationVersionV1 = 1;
    enum StartupObservationFacts : std::uint32_t
    {
        StartupLoadingTypeObserved = 1,
        StartupLoadingTypeMatches = 2,
        StartupSimpleFadeTypeObserved = 4,
        StartupSimpleFadeTypeMatches = 8,
        StartupGameModeOwnerObserved = 16,
        StartupWorldLinkObserved = 32
    };
    enum StartupObservationRefusals : std::uint32_t
    {
        StartupLoadingTypeRefused = 1,
        StartupSimpleFadeTypeRefused = 2,
        StartupGameModeOwnerRefused = 4,
        StartupWorldLinkRefused = 8
    };

    // Keep the existing application-frame record unchanged. These additional
    // observations distinguish native metadata and ownership links from opaque
    // dictionary membership; none certifies GC, provider or StartGame readiness.
    struct StartupObservationStateV1
    {
        std::uint32_t                      size = sizeof(StartupObservationStateV1), version = StartupObservationVersionV1;
        std::uint64_t                      generation = 0;
        ApplicationFrameObservationStateV1 frame;
        std::uint64_t                      observations = 0, refusedObservations = 0;
        std::uint64_t                      loadingTypeObserved = 0, loadingCompatible = 0;
        std::uint64_t                      simpleFadeTypeObserved = 0, simpleFadeCompatible = 0;
        std::uint64_t                      ownerObserved = 0, worldObserved = 0;
        std::uint32_t                      lastFacts = 0, lastRefusals = 0;
        std::uint32_t                      reserved[10]{};
    };
    static_assert(std::is_standard_layout_v<StartupObservationStateV1> && std::is_trivially_copyable_v<StartupObservationStateV1> &&
                  sizeof(StartupObservationStateV1) == 256);

    // This mode borrows the existing clock lease's Renew/Read/End ownership
    // functions while admitting no clock commands or native startup writes.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_STARTUP_OBSERVATION_EXPORT __declspec(dllexport)
#else
    #define MTA_STARTUP_OBSERVATION_EXPORT
#endif
    extern "C" MTA_STARTUP_OBSERVATION_EXPORT Status BeginGameStartupObservationV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                   const ClockServiceRequestV1* request, ClockServiceStateV1* output) noexcept;
    extern "C" MTA_STARTUP_OBSERVATION_EXPORT Status ReadGameStartupObservationV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation,
                                                                                  StartupObservationStateV1* output) noexcept;
}
