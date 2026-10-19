/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional value-only observation of genuine DE application callbacks
 *****************************************************************************/
#pragma once

#include "CGameClockService.h"
#include <cstdint>
#include <type_traits>

namespace mta::game_de
{
    inline constexpr std::uint32_t ApplicationFrameObservationVersionV1 = 1;
    enum ApplicationFrameUiMask : std::uint32_t
    {
        LoadingObserved = 1,
        LoadingPresent = 2,
        LoadingNonNull = 4,
        SimpleFadeObserved = 8,
        SimpleFadePresent = 16,
        SimpleFadeNonNull = 32
    };

    // Lookup availability, key presence and nonnull values are separate facts.
    // No native addresses escape this record and none of these observations
    // certifies UI class type, provider readiness or permission to start a game.
    struct ApplicationFrameObservationStateV1
    {
        std::uint32_t size = sizeof(ApplicationFrameObservationStateV1), version = ApplicationFrameObservationVersionV1;
        std::uint64_t generation = 0, frames = 0;
        std::uint32_t engineThread = 0, nativeState = 0;
        std::uint64_t frontendFrames = 0, playingFrames = 0;
        std::uint64_t loadingLookups = 0, loadingKeyPresent = 0, loadingNonNull = 0;
        std::uint64_t simpleFadeLookups = 0, simpleFadeKeyPresent = 0, simpleFadeNonNull = 0;
        std::uint64_t lookupFailures = 0;
        std::uint32_t lastLookupMask = 0, deltaBits = 0;
        std::uint32_t reserved[4]{};
    };
    static_assert(std::is_standard_layout_v<ApplicationFrameObservationStateV1> && std::is_trivially_copyable_v<ApplicationFrameObservationStateV1> &&
                  sizeof(ApplicationFrameObservationStateV1) == 128);

    // The existing clock ownership record/renew/end functions retain the same
    // native transaction owner. This separately named mode never submits clock
    // commands or changes the existing V1 clock payload or adapter ABI.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_APPLICATION_OBSERVATION_EXPORT __declspec(dllexport)
#else
    #define MTA_APPLICATION_OBSERVATION_EXPORT
#endif
    extern "C" MTA_APPLICATION_OBSERVATION_EXPORT Status BeginGameApplicationFrameObservationV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                                const ClockServiceRequestV1* request,
                                                                                                ClockServiceStateV1*         output) noexcept;
    extern "C" MTA_APPLICATION_OBSERVATION_EXPORT Status ReadGameApplicationFrameObservationV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                               std::uint64_t                       generation,
                                                                                               ApplicationFrameObservationStateV1* output) noexcept;
}
