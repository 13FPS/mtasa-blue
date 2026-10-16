/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit existing-console lifecycle without upgrading frontend API1
 *****************************************************************************/
#pragma once
#include "DefinitiveFrontend.h"

namespace DefinitiveFrontendConsole
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t MaximumTextCapacity = 8192;
    using Status = DefinitiveFrontend::Status;
    enum Field : std::uint32_t
    {
        Version,
        Phase,
        OwnerThread,
        ConsoleCreated,
        ConsoleVisible,
        InputActive,
        MenuInputRequested,
        ConsoleInputRequested,
        AggregateInputRequested,
        CommandPolicyActive,
        CommandsRegistered,
        CommandsExecuted,
        CommandsRejected,
        LastCommand,
        TextAccepted,
        HistoryEntries,
        HistorySelections,
        AutoCompleteUses,
        OutputBytes,
        InputBytes,
        HistoryPersistent,
        CleanupComplete,
        ConsoleRetained,
        CommandsRetained,
        ConfigurationReadOnly,
        NetworkStarted,
        GameplayReady,
        FullMtaReady,
        ReleaseFault,
        MaxInputCharacters,
        CVarRevision,
        Reserved31,
        FieldCount
    };
    enum class Lifecycle : std::uint32_t
    {
        Disabled,
        Starting,
        Running,
        Stopping,
        Stopped,
        Failed
    };
    enum class Command : std::uint32_t
    {
        None,
        Help,
        Clear,
        Rejected
    };
    enum class Text : std::uint32_t
    {
        Input,
        Output
    };
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using Enable = Status (*)(void*, std::uint32_t);
    using Read = Status (*)(void*, std::uint32_t*, std::uint32_t);
    using SetVisible = Status (*)(void*, std::uint32_t);
    // The admitted host routes only its focused owned HWND. Ordinary text and
    // Enter remain with its real GUI translator; this intercepts hotkeys only.
    using ProcessMessage = Status (*)(void*, std::uint32_t, std::uintptr_t, std::intptr_t, std::uint32_t*);
    // required includes the trailing NUL. A null buffer/capacity0 queries size;
    // insufficient capacity preserves the buffer while returning required.
    using ReadText = Status (*)(void*, std::uint32_t, char*, std::uint32_t, std::uint32_t*);
}
