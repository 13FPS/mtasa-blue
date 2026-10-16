/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Completed Core ownership for explicit DE frontend startup
 *****************************************************************************/
#pragma once

#include "DefinitiveFrontend.h"
#include "DefinitiveInputReference.h"
#include <array>
#include <cstdint>

namespace DefinitiveFrontendStartup
{
    inline constexpr std::uint32_t ApiVersion = 1;
    using Status = DefinitiveFrontend::Status;

    enum Field : std::uint32_t
    {
        Version,
        Phase,  // 1 starting, 2 running, 3 stopping, 4 stopped, 5 failed.
        OwnerThread,
        CoreRetained,
        BackendStartAttempted,
        BackendStarted,
        ExactImageBound,
        Pulses,
        FrontendRetained,
        FrontendPhase,
        GuiBorrowed,
        ServicesRetained,
        InputRequested,
        CleanupPending,
        CleanupComplete,
        ReleaseFault,
        CapabilitiesLow,
        CapabilitiesHigh,
        NetworkStarted,
        GameplayReady,
        FullMtaReady,
        LastFailure,
        ViewRuns,
        LastViewCleanupComplete,
        Reserved24,
        Reserved25,
        Reserved26,
        Reserved27,
        Reserved28,
        Reserved29,
        Reserved30,
        Reserved31,
        FieldCount
    };
    static_assert(FieldCount == 32);
    using Snapshot = std::array<std::uint32_t, FieldCount>;

    enum class Failure : std::uint32_t
    {
        None,
        CoreConstruction,
        BackendStart,
        BackendPulse,
        InputBind,
        BackendStop,
        View
    };

    // Begin owns a completed Core and starts its actual backend without engine
    // reads. Native image binding is explicit. This success is frontend-only;
    // it never changes InitializeCore or the existing full-startup diagnostics.
    using Begin = void* (*)(std::uint32_t);
    using Pulse = Status (*)(void*);
    using BindInput = Status (*)(void*);
    using Read = Status (*)(void*, std::uint32_t*, std::uint32_t);
    using Shutdown = Status (*)(void*);
    using Release = Status (*)(void*);
    using RunView = Status (*)(void*, std::uint32_t, DefinitiveInputReference::CancelCallback, DefinitiveInputReference::PublishCallback);

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    struct Controller;
    void*  BeginOwner() noexcept;
    Status PulseOwner(void* handle) noexcept;
    Status BindOwner(void* handle) noexcept;
    Status ReadOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept;
    Status ShutdownOwner(void* handle) noexcept;
    Status ReleaseOwner(void* handle) noexcept;
    Status RunViewOwner(void* handle, std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                        DefinitiveInputReference::PublishCallback publish) noexcept;
#endif
}
