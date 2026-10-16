/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Renewable frontend lifetime without changing bounded trial APIs
 *****************************************************************************/
#pragma once

#include "DefinitiveFrontend.h"
#include "DefinitiveInputReference.h"
#include <array>
#include <cstdint>

namespace DefinitivePersistentFrontend
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 288;
    inline constexpr std::uint32_t HostHeartbeatMs = 1000;
    using Status = DefinitiveFrontend::Status;
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using HeartbeatCallback = bool (*)();

    // The first 256 fields retain the common GUI/input counter positions, but
    // belong to this separate schema. DisplayDuration is zero (until closed),
    // so this report can never masquerade as the historical 20-second trial.
    // NativeActiveMs freezes when input shutdown starts, excluding cleanup.
    enum Field : std::uint32_t
    {
        PersistentVersion = 256,
        SessionGenerationLow,
        SessionGenerationHigh,
        HostLeaseFresh,
        NativeLeaseGenerationLow,
        NativeLeaseGenerationHigh,
        NativeLeaseActive,
        NativeLeaseRevoked,
        NativeLeaseReason,
        HostHeartbeatBudgetMs,
        NativeHeartbeatBudgetMs,
        NativeLeaseRenewalsLow,
        NativeLeaseRenewalsHigh,
        StopReason,
        Reserved270,
    };
    enum class Stop : std::uint32_t
    {
        Running,
        GuiClosed,
        Cancelled,
        HostLeaseExpired,
        FocusLost,
        Fault
    };

    struct RequestV1
    {
        std::uint32_t                             size = sizeof(RequestV1);
        std::uint32_t                             version = ApiVersion;
        std::uint64_t                             generation = 0;
        std::uint32_t                             reserved[2]{};
        DefinitiveInputReference::CancelCallback  cancelled = nullptr;
        DefinitiveInputReference::PublishCallback publish = nullptr;
        // This value-only callback reads an independently renewed host lease.
        // The worker cannot renew it on the script's behalf while drawing.
        HeartbeatCallback heartbeat = nullptr;
    };
    using Run = Status (*)(void*, const RequestV1*);

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    Status RunOwner(void* handle, const RequestV1* request) noexcept;
#endif
}
