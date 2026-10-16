/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional owner-thread lifecycle for the existing MTA frontend
 *****************************************************************************/
#pragma once
#include <array>
#include <cstdint>

class CGUI;

namespace DefinitiveFrontend
{
    inline constexpr std::uint32_t ApiVersion = 1;
    enum class Status : std::uint32_t
    {
        Ok,
        Busy,
        InvalidArgument,
        WrongThread,
        Failed,
        Stopped
    };
    enum Field : std::uint32_t
    {
        Version,
        Phase,
        OwnerThread,
        Frames,
        MenuVisible,
        MenuFading,
        InputRequested,
        CloseRequested,
        CreditsVisible,
        AboutClicks,
        XmlLoaded,
        ConfigLoaded,
        LocalizationLoaded,
        UpdaterCreated,
        UpdaterWorkerStarted,
        BackgroundRequestsRejected,
        CleanupPending,
        CleanupComplete,
        OwnsCoreShell,
        ReleaseFault,
        ServicesRetained,
        GuiBorrowed,
        ConfigurationReadOnly,
        ApplicationLocalePersisted,
        NetworkStarted,
        AdapterStarted,
        GameplayReady,
        FullMtaReady,
        UpdaterConfigLoaded,
        Reserved29,
        Reserved30,
        Reserved31,
        FieldCount
    };
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using Create = void* (*)(std::uint32_t, CGUI*);
    using Pulse = Status (*)(void*, std::int32_t, std::int32_t, std::uint32_t);
    using Read = Status (*)(void*, std::uint32_t*, std::uint32_t);
    using Shutdown = Status (*)(void*);
    using Release = Status (*)(void*);

    struct Controller;
}
