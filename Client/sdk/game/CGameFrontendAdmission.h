/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read-only qualification before borrowing the interactive DE frontend
 *****************************************************************************/
#pragma once

#include "CGameAdapter.h"
#include <type_traits>

namespace mta::game_de
{
    inline constexpr std::uint32_t FrontendAdmissionVersionV1 = 1;

    enum class FrontendAdmission : std::uint32_t
    {
        EligibleLocal = 1,
        Deferred
    };

    // Deferred describes an observed startup/transition state or a completely
    // qualified local identity that currently fails the existing movement
    // predicate. Missing native evidence is an error, never a deferred success.
    // Neither result grants a native input lease or a playerless GUI capability.
    struct FrontendAdmissionStateV1
    {
        std::uint32_t size = sizeof(FrontendAdmissionStateV1), version = FrontendAdmissionVersionV1;
        std::uint32_t producerThread = 0, admission = 0, systemState = 0;
        std::uint32_t contextKnownMask = 0, contextFlags = 0, reserved = 0;
    };
    static_assert(std::is_standard_layout_v<FrontendAdmissionStateV1> && std::is_trivially_copyable_v<FrontendAdmissionStateV1>);
    static_assert(sizeof(FrontendAdmissionStateV1) == 32);

    // Optional API outside ABI6. It reuses the backend's checked state and
    // local-eligibility readers, without allocating a GUI, installing hooks,
    // synthesizing input or changing a native startup state.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_FRONTEND_ADMISSION_EXPORT __declspec(dllexport)
#else
    #define MTA_FRONTEND_ADMISSION_EXPORT
#endif
    extern "C" MTA_FRONTEND_ADMISSION_EXPORT Status ReadGameFrontendAdmissionV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                FrontendAdmissionStateV1* output) noexcept;
#undef MTA_FRONTEND_ADMISSION_EXPORT
}
