/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional neutral frontend input lease without changing Adapter ABI6
 *****************************************************************************/
#pragma once
#include "CGameAdapter.h"
#include <type_traits>

namespace mta::game_de
{
    inline constexpr std::uint32_t FrontendInputLeaseVersionV1 = 1;
    inline constexpr std::uint32_t FrontendInputLeaseLifetimeMsV1 = 500;

    enum class FrontendInputLeaseReason : std::uint32_t
    {
        None,
        Expired,
        ClockRollback,
        FocusLost,
        NonNeutral,
        Stopped,
        NativeFailure
    };

    struct FrontendInputLeaseRequestV1
    {
        std::uint32_t size = sizeof(FrontendInputLeaseRequestV1), version = FrontendInputLeaseVersionV1;
        // This must be the current process's foreground root game HWND. MTA
        // children may receive focus inside that family; another app may not.
        void*         window = nullptr;
        MtaInputState neutral;
        std::uint32_t reserved[2]{};
    };

    struct FrontendInputLeaseStateV1
    {
        std::uint32_t size = sizeof(FrontendInputLeaseStateV1), version = FrontendInputLeaseVersionV1;
        std::uint64_t generation = 0;
        std::uint32_t producerThread = 0, active = 0, revoked = 0, reason = 0;
        std::uint32_t lifetimeMs = FrontendInputLeaseLifetimeMsV1, bound = 0;
        std::uint64_t refreshedAtMs = 0, renewals = 0;
        std::uint32_t reserved[2]{};
    };
    static_assert(std::is_standard_layout_v<FrontendInputLeaseRequestV1> && std::is_trivially_copyable_v<FrontendInputLeaseRequestV1>);
    static_assert(std::is_standard_layout_v<FrontendInputLeaseStateV1> && std::is_trivially_copyable_v<FrontendInputLeaseStateV1>);
    static_assert(sizeof(FrontendInputLeaseStateV1) == 64);
#if defined(_WIN64)
    static_assert(sizeof(FrontendInputLeaseRequestV1) == 120);
#endif

    // All calls belong to the initialized adapter's owning producer thread.
    // Begin's output is untouched for invalid requests; a native setup refusal
    // publishes its new generation so Read/End can retire partial ownership.
    // Renew cannot extend a revoked/expired generation or accept movement. End
    // can return Unsupported while the same producer must retry actual detach.
    // This optional API never changes IGameAdapter's vtable or capabilities.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_FRONTEND_INPUT_LEASE_EXPORT __declspec(dllexport)
#else
    #define MTA_FRONTEND_INPUT_LEASE_EXPORT
#endif
    extern "C" MTA_FRONTEND_INPUT_LEASE_EXPORT Status BeginGameFrontendInputLeaseV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                    const FrontendInputLeaseRequestV1* request,
                                                                                    FrontendInputLeaseStateV1*         output) noexcept;
    extern "C" MTA_FRONTEND_INPUT_LEASE_EXPORT Status RenewGameFrontendInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation,
                                                                                    const MtaInputState* neutral) noexcept;
    extern "C" MTA_FRONTEND_INPUT_LEASE_EXPORT Status ReadGameFrontendInputLeaseV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                   FrontendInputLeaseStateV1* output) noexcept;
    extern "C" MTA_FRONTEND_INPUT_LEASE_EXPORT Status EndGameFrontendInputLeaseV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                  std::uint64_t generation) noexcept;
#undef MTA_FRONTEND_INPUT_LEASE_EXPORT
}
