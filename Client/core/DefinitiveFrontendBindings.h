/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit ownership of MTA's configured frontend input producer
 *****************************************************************************/
#pragma once
#include "DefinitiveFrontend.h"
#include <game/CGameAdapter.h>

namespace DefinitiveFrontendBindings
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint64_t MovementActions = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(BACKWARDS) | MtaInput::ActionBit(LEFT) |
                                                     MtaInput::ActionBit(RIGHT) | MtaInput::ActionBit(SPRINT) | MtaInput::ActionBit(WALK);
    using Status = DefinitiveFrontend::Status;
    enum Field : std::uint32_t
    {
        Version,
        Phase,
        OwnerThread,
        BindingsCreated,
        ConfigurationLoaded,
        BindCount,
        SupportedBindings,
        IgnoredBindings,
        EligibleKeys,
        UnsupportedKeys,
        InputSuppressed,
        NeutralFrames,
        CurrentZero,
        PreviousZero,
        CleanupComplete,
        BindingsRetained,
        ConfigurationReadOnly,
        CVarRevision,
        NetworkStarted,
        GameplayReady,
        FullMtaReady,
        ReleaseFault,
        Reserved22,
        Reserved23,
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
    enum class Lifecycle : std::uint32_t
    {
        Disabled,
        Starting,
        Running,
        Stopping,
        Stopped,
        Failed
    };
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    static_assert(FieldCount == 32);
    using Enable = Status (*)(void*, std::uint32_t);
    using Read = Status (*)(void*, std::uint32_t*, std::uint32_t);
    // This producer is explicitly neutral even without visible frontend input.
    // Its coherent copied states grant no permission to write native controls.
    using ProduceNeutral = Status (*)(void*, mta::game_de::MtaInputState*, std::uint32_t);
}
