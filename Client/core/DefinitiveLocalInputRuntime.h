/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse one retained Core binding manager for local on-foot input
 *****************************************************************************/
#pragma once

#include "DefinitiveFrontend.h"
#include <array>
#include <cstdint>
#include <type_traits>

namespace DefinitiveLocalInputRuntime
{
    inline constexpr std::uint32_t ApiVersion = 1;
    using Status = DefinitiveFrontend::Status;

    struct RequestV1
    {
        std::uint32_t size = sizeof(RequestV1), version = ApiVersion;
        void*         window = nullptr;
        std::uint32_t reserved[2]{};
    };
    static_assert(std::is_standard_layout_v<RequestV1> && std::is_trivially_copyable_v<RequestV1>);
#if defined(_WIN64)
    static_assert(sizeof(RequestV1) == 24);
#endif

    enum class State : std::uint32_t
    {
        Disabled = 1,
        Arming,
        Active,
        Suspended,
        RetainedCleanup,
        Failed
    };
    enum class Failure : std::uint32_t
    {
        None,
        InputBind,
        Bindings,
        CanonicalInput,
        FocusLost,
        EligibilityLost,
        LeaseRevoked,
        NativeBegin,
        NativeRead,
        NativeRenew,
        NativeEnd,
        Exception
    };
    enum Field : std::uint32_t
    {
        Version,
        Phase,
        OwnerThread,
        Configured,
        Enabled,
        NativeAttempted,
        NativeGenerationLow,
        NativeGenerationHigh,
        NativeActive,
        NativeBound,
        NativeRevoked,
        NativeReason,
        NativeLifetimeMs,
        RenewalsLow,
        RenewalsHigh,
        HeldActionsLow,
        HeldActionsHigh,
        PhysicalRearmReady,
        CanonicalNeutral,
        BindingsBorrowed,
        CleanupPending,
        CleanupComplete,
        LastFailure,
        Focused,
        ContextKnownMask,
        ContextFlags,
        ContextEligible,
        NativeEndAttempts,
        SupportedBindings,
        IgnoredBindings,
        EligibleKeys,
        UnsupportedKeys,
        FieldCount
    };
    static_assert(FieldCount == 32);
    using Snapshot = std::array<std::uint32_t, FieldCount>;

    // Configure/resume is deliberate. Pulse waits without native ownership for
    // focus, released keys and exact eligibility; focus/context/lease loss
    // suspends and flushes rather than silently restarting a revoked lease.
    // Suspend retains configuration, Disable also clears its enablement. Busy
    // means native or canonical cleanup remains owned by this same Core.
    // This optional record does not reinterpret Startup32/Runtime64/Runtime416
    // or advertise CGame, remote input, networking or full gameplay readiness.
    using Configure = Status (*)(void*, const RequestV1*);
    using Pulse = Status (*)(void*);
    using Read = Status (*)(void*, std::uint32_t*, std::uint32_t);
    using Suspend = Status (*)(void*);
    using Disable = Status (*)(void*);

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    Status ConfigureOwner(void* handle, const RequestV1* request) noexcept;
    Status PulseOwner(void* handle) noexcept;
    Status ReadOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept;
    Status SuspendOwner(void* handle) noexcept;
    Status DisableOwner(void* handle) noexcept;
#endif
}
