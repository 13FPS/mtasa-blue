/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Separate protocol for an MTA-owned GUI surface inside the game HWND
 *****************************************************************************/
#pragma once
#include "DefinitiveGuiStartupReference.h"

namespace DefinitiveGuiSurfaceReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 96;
    inline constexpr std::uint32_t MaximumDurationMs = 10000;
    inline constexpr std::uint32_t DisplayDurationMs = 8000;
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using Outcome = DefinitiveGuiStartupReference::Outcome;
    using StopReason = DefinitiveGuiStartupReference::StopReason;

    // 0..59 reuse the common ownership lifecycle's positions, not its offscreen
    //  workload policy.38 counts checked pixels;53/54/55 certify transparency,
    //  red widget texture and premultiplied alpha. Queue/target are always MTA's.
    enum Field : std::uint32_t
    {
        ParentValidated = 60,
        ParentWindowed,
        ParentAlive,
        ChildCreated,
        ChildShown,
        CompositionCreated,
        SwapchainCreated,
        PresentCalls,
        PresentSucceeded,
        PresentHresult,
        GuiFrames,
        ResizeCount,
        ResizePending,
        ParentDestroyed,
        StopRequested,
        ChildRetained,
        QueueRetained,
        SurfaceRetained,
        WindowCleanupComplete,
        CompositionCleanupComplete,
        ClassUnregistered,
        OwnerThreadId,
        ParentThreadId,
        AlphaMode,
        SignalAttempts,
        DisplayDuration,
        PrivateSurface,
        PresentBusy,
        // 88..95 stay zero; no HWND or native graphics pointer enters this wire.
    };

    template <typename Owner, typename Clock, typename Request>
    Snapshot Run(Owner& owner, Clock& clock, const Request& request) noexcept
    {
        return DefinitiveGuiStartupReference::Run<Snapshot>(owner, clock, request);
    }
}
