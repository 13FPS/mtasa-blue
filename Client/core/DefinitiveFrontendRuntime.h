/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain genuine frontend configuration across independently retired views
 *****************************************************************************/
#pragma once

#include "DefinitiveFrontendStartup.h"
#include "DefinitivePersistentFrontend.h"

namespace DefinitiveFrontendRuntime
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t StartupFieldCount = DefinitiveFrontendStartup::FieldCount;
    using Status = DefinitiveFrontend::Status;

    // The existing Startup32 remains an exact prefix. Only this optional
    // schema reports session-owned XML/CVars/bindings while a view is hidden;
    // old view-retirement fields continue to describe the retired view aliases.
    enum Field : std::uint32_t
    {
        RuntimeVersion = StartupFieldCount,
        RuntimePhase,
        OwnerThread,
        SessionGenerationLow,
        SessionGenerationHigh,
        ServicesInitialized,
        ServicesRetained,
        XmlRetained,
        ConfigRetained,
        CVarsLoaded,
        BindingsRetained,
        BindingManagerCreations,
        ConfigurationLoads,
        BindingLoads,
        ViewsAttached,
        ViewsRetired,
        ViewGenerationLow,
        ViewGenerationHigh,
        ViewBorrowed,
        HiddenNeutralVerified,
        CVarRevisionLow,
        CVarRevisionHigh,
        BindCount,
        ConfigurationReadOnly,
        CleanupPending,
        CleanupComplete,
        LastFailure,
        Reserved59,
        NetworkStarted,
        GameplayReady,
        FullMtaReady,
        Reserved63,
        FieldCount
    };
    static_assert(StartupFieldCount == 32 && FieldCount == 64);
    using Snapshot = std::array<std::uint32_t, FieldCount>;

    enum class Phase : std::uint32_t
    {
        Starting = 1,
        Hidden,
        Visible,
        Stopping,
        Stopped,
        Failed
    };
    enum class Failure : std::uint32_t
    {
        None,
        ServicesStart,
        View,
        ServicesStop,
        BackendStop
    };

    // Runtime and view generations have separate lifetimes. The former is a
    // nonzero Core-owner identity, not the Bridge's independent host/mailbox
    // token. Each native view requests the next Core view generation; direct
    // CPU/GUI borrowing consumes that same counter without native admission.
    using Begin = void* (*)(std::uint32_t);
    using Read = Status (*)(void*, std::uint32_t*, std::uint32_t);
    using Pulse = Status (*)(void*);
    using RunView = Status (*)(void*, const DefinitivePersistentFrontend::RequestV1*);
    using Shutdown = Status (*)(void*);
    using Release = Status (*)(void*);
    // The worker may explicitly load durable movement binds before its first
    // view/input borrower. Legacy Runtime Begin remains configuration-read-only.
    using EnableBindingPersistence = Status (*)(void*, std::uint32_t);
    using EnableBindingPersistenceAtPath = Status (*)(void*, std::uint32_t, const wchar_t*, std::uint32_t);

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    void*  BeginOwner() noexcept;
    Status ReadOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept;
    Status PulseOwner(void* handle) noexcept;
    Status RunViewOwner(void* handle, const DefinitivePersistentFrontend::RequestV1* request) noexcept;
    Status ShutdownOwner(void* handle) noexcept;
    Status ReleaseOwner(void* handle) noexcept;
    Status EnableBindingPersistenceOwner(void* handle, std::uint32_t version) noexcept;
    Status EnableBindingPersistenceAtPathOwner(void* handle, std::uint32_t version, const wchar_t* path, std::uint32_t chars) noexcept;
#endif
}
