/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional renewable local movement input without changing Adapter ABI6
 *****************************************************************************/
#pragma once
#include "CGameAdapter.h"
#include <type_traits>

namespace mta::game_de
{
    inline constexpr std::uint32_t LocalInputLeaseVersionV1 = 1;
    inline constexpr std::uint32_t LocalInputLeaseLifetimeMsV1 = 500;

    enum class LocalInputLeaseReason : std::uint32_t
    {
        None,
        Expired,
        ClockRollback,
        FocusLost,
        InvalidState,
        Stopped,
        NativeFailure,
        EligibilityLost
    };

    struct LocalInputLeaseRequestV1
    {
        std::uint32_t size = sizeof(LocalInputLeaseRequestV1), version = LocalInputLeaseVersionV1;
        // The foreground root HWND belongs to this game process. A menu must
        // retire this movement lease before borrowing native input itself.
        void*         window = nullptr;
        MtaInputState neutral;
        std::uint32_t reserved[2]{};
    };

    struct LocalInputLeaseStateV1
    {
        std::uint32_t size = sizeof(LocalInputLeaseStateV1), version = LocalInputLeaseVersionV1;
        std::uint64_t generation = 0;
        std::uint32_t producerThread = 0, active = 0, revoked = 0, reason = 0;
        std::uint32_t lifetimeMs = LocalInputLeaseLifetimeMsV1, bound = 0;
        std::uint64_t refreshedAtMs = 0, renewals = 0;
        std::uint32_t reserved[2]{};
    };
    static_assert(std::is_standard_layout_v<LocalInputLeaseRequestV1> && std::is_trivially_copyable_v<LocalInputLeaseRequestV1>);
    static_assert(std::is_standard_layout_v<LocalInputLeaseStateV1> && std::is_trivially_copyable_v<LocalInputLeaseStateV1>);
    static_assert(sizeof(LocalInputLeaseStateV1) == 64);
#if defined(_WIN64)
    static_assert(sizeof(LocalInputLeaseRequestV1) == 120);
#endif

    // Begin requires both canonical frames neutral. Renew alone publishes six-
    // action on-foot movement and extends the deadline; SubmitControlActions
    // can stage MTA history but cannot renew this native lease. Expiry, lost
    // focus and invalid owner input revoke the generation irreversibly. Native
    // eligibility and scoped restoration remain required on every callback.
    // Malformed/wrong-owner calls preserve output. A valid native Begin refusal
    // can publish a partial generation that the same producer must Read/End.
    // End may retain hooks/restoration and return Unsupported until retried.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_LOCAL_INPUT_LEASE_EXPORT __declspec(dllexport)
#else
    #define MTA_LOCAL_INPUT_LEASE_EXPORT
#endif
    extern "C" MTA_LOCAL_INPUT_LEASE_EXPORT Status BeginGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter,
                                                                              const LocalInputLeaseRequestV1* request, LocalInputLeaseStateV1* output) noexcept;
    extern "C" MTA_LOCAL_INPUT_LEASE_EXPORT Status RenewGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation,
                                                                              const MtaInputState* movement) noexcept;
    extern "C" MTA_LOCAL_INPUT_LEASE_EXPORT Status ReadGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter,
                                                                             LocalInputLeaseStateV1* output) noexcept;
    extern "C" MTA_LOCAL_INPUT_LEASE_EXPORT Status EndGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept;
#undef MTA_LOCAL_INPUT_LEASE_EXPORT
}
