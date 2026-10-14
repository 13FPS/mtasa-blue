/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Value-only Core-owned native render observation contract
 *****************************************************************************/

#pragma once

#include <array>
#include <algorithm>
#include <cstdint>

namespace DefinitiveRenderReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 40;
    inline constexpr std::uint32_t ApiVersionV2 = 2;
    inline constexpr std::uint32_t FieldCountV2 = 44;
    inline constexpr std::uint32_t MaximumDurationMs = 10000;
    inline constexpr std::uint32_t SampleIntervalMs = 50;
    inline constexpr std::uint32_t MaximumSamples = 200;
    inline constexpr std::uint32_t MaximumPulseGapMs = 1000;
    inline constexpr std::uint32_t CleanupDeadlineMs = 2000;
    inline constexpr std::uint32_t CleanupBackoffMs = 250;

    enum Field : std::uint32_t
    {
        Version,
        Phase,
        Sequence,
        ElapsedMs,
        Reason,
        SampleCount,
        CleanupComplete,
        CleanupAttempts,
        CleanupDeadlineExceeded,
        BackendPinnedExpected,
        CleanupPending,
        ObserverStarted,
        Bound,
        Active,
        ModulePinned,
        GenerationLow,
        GenerationHigh,
        PresentCallbacksLow,
        PresentCallbacksHigh,
        AcceptedSnapshotsLow,
        AcceptedSnapshotsHigh,
        RejectedSnapshotsLow,
        RejectedSnapshotsHigh,
        NestedCallbacksLow,
        NestedCallbacksHigh,
        CallbackThreadId,
        Width,
        Height,
        SyncInterval,
        NativeOriginalResult,
        FailureReason,
        NativeOriginalReturned,
        NativeCleanupPending,
        ForeignCallbacksLow,
        ForeignCallbacksHigh,
        PublicationMissesLow,
        PublicationMissesHigh,
        NativePresence,
        Reserved,
        ReservedHigh,
        RequestedRoute,
        ObservedRoute,
        PresenceKnownMask,
        QueuePresent,
    };

    enum class StopReason : std::uint32_t
    {
        Running,
        TimeBudget,
        Cancelled,
        BeginError,
        ReadError,
        PulseGap,
        SampleBudget,
        Exception,
        CleanupError,
    };

    // Phase3 is an actual retained Core/worker owner, not terminal failure. A
    // cleanup deadline reports failure but cannot authorize unloading live code.
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using SnapshotV2 = std::array<std::uint32_t, FieldCountV2>;

    // Both protocols copy the actual observer's scalar state. Keeping this
    // shared with focused tests prevents route selection from inventing a
    // device/context or treating an unobserved RHI as an accepted callback.
    template <std::size_t N, typename State>
    void CopyNativeState(std::array<std::uint32_t, N>& snapshot, const State& state) noexcept
    {
        static_assert(N == FieldCount || N == FieldCountV2);
        snapshot[Bound] = state.bound;
        snapshot[Active] = state.active;
        snapshot[ModulePinned] = snapshot[BackendPinnedExpected] = state.modulePinned;
        const std::array<std::uint64_t, 5> counters{state.generation, state.nativePresentCallbacks, state.acceptedSnapshots, state.rejectedSnapshots,
                                                    state.nestedCallbacks};
        for (std::size_t index = 0; index < counters.size(); ++index)
        {
            snapshot[GenerationLow + index * 2] = static_cast<std::uint32_t>(counters[index]);
            snapshot[GenerationLow + index * 2 + 1] = static_cast<std::uint32_t>(counters[index] >> 32);
        }
        snapshot[CallbackThreadId] = state.callbackThreadId;
        snapshot[Width] = state.width;
        snapshot[Height] = state.height;
        snapshot[SyncInterval] = state.syncInterval;
        snapshot[NativeOriginalResult] = state.lastOriginalResult;
        snapshot[FailureReason] = state.failureReason;
        snapshot[NativeOriginalReturned] = state.lastOriginalReturned;
        snapshot[NativeCleanupPending] = state.pendingCleanup;
        snapshot[ForeignCallbacksLow] = static_cast<std::uint32_t>(state.foreignThreadCallbacks);
        snapshot[ForeignCallbacksHigh] = static_cast<std::uint32_t>(state.foreignThreadCallbacks >> 32);
        snapshot[PublicationMissesLow] = static_cast<std::uint32_t>(state.publicationMisses);
        snapshot[PublicationMissesHigh] = static_cast<std::uint32_t>(state.publicationMisses >> 32);
        snapshot[NativePresence] =
            (state.windowPresent ? 1u : 0u) | (state.swapchainPresent ? 2u : 0u) | (state.devicePresent ? 4u : 0u) | (state.contextPresent ? 8u : 0u);
    }

    template <typename State>
    void CopyNativeStateV2(SnapshotV2& snapshot, const State& state) noexcept
    {
        CopyNativeState(snapshot, state.base);
        snapshot[RequestedRoute] = state.requestedRoute;
        snapshot[ObservedRoute] = state.observedRoute;
        snapshot[PresenceKnownMask] = state.presenceKnownMask;
        snapshot[QueuePresent] = state.queuePresent;
    }

    // The real Core uses this same value-only lifecycle. Injecting only the
    // observer and monotonic clock makes cleanup/cancel/deadline behavior
    // testable without substituting Core or invoking a game/render function.
    template <typename SnapshotType = Snapshot, typename Observer, typename Clock, typename Request>
    SnapshotType Run(Observer& observer, Clock& clock, const Request& request) noexcept
    {
        static_assert(noexcept(clock.Now()) && noexcept(clock.Wait(std::uint32_t{})), "Lifecycle clock must not throw during retained cleanup");
        SnapshotType   snapshot{};
        constexpr auto count = std::tuple_size<SnapshotType>::value;
        static_assert(count == FieldCount || count == FieldCountV2);
        snapshot[Version] = count == FieldCountV2 ? ApiVersionV2 : ApiVersion;
        bool validRoute = true;
        if constexpr (count == FieldCountV2)
        {
            snapshot[RequestedRoute] = request.renderRoute;
            validRoute = request.renderRoute == 1 || request.renderRoute == 2;
        }
        auto reason = StopReason::BeginError;
        auto started = clock.Now();
        auto lastPulse = started;
        try
        {
            if (request.cancelled())
                reason = StopReason::Cancelled;
            else if (validRoute && observer.Begin())
            {
                snapshot[ObserverStarted] = 1;
                started = lastPulse = clock.Now();
                reason = StopReason::Running;
                for (;;)
                {
                    const auto now = clock.Now();
                    if (now >= started)
                        snapshot[ElapsedMs] = static_cast<std::uint32_t>((std::min)(now - started, std::uint64_t{0xffffffffu}));
                    if (request.cancelled())
                    {
                        reason = StopReason::Cancelled;
                        break;
                    }
                    if (now < started || now < lastPulse || now - lastPulse > MaximumPulseGapMs)
                    {
                        reason = StopReason::PulseGap;
                        break;
                    }
                    if (now - started >= request.durationMs)
                    {
                        reason = StopReason::TimeBudget;
                        break;
                    }
                    if (snapshot[SampleCount] >= MaximumSamples)
                    {
                        reason = StopReason::SampleBudget;
                        break;
                    }
                    lastPulse = now;
                    if (!observer.Read(snapshot) || snapshot[FailureReason])
                    {
                        reason = StopReason::ReadError;
                        break;
                    }
                    snapshot[Phase] = 1;
                    ++snapshot[Sequence];
                    ++snapshot[SampleCount];
                    request.publish(snapshot.data(), static_cast<std::uint32_t>(count));
                    clock.Wait(SampleIntervalMs);
                }
            }
        }
        catch (...)
        {
            reason = StopReason::Exception;
        }

        // A failed Begin may already hold a native lease. Reporting a cleanup
        // timeout never permits the owning stack or loaded Core to disappear.
        const auto cleanupStarted = clock.Now();
        snapshot[CleanupPending] = 1;
        for (;;)
        {
            if (snapshot[CleanupAttempts] != 0xffffffffu)
                ++snapshot[CleanupAttempts];
            bool ended = false;
            try
            {
                ended = observer.End();
            }
            catch (...)
            {
                reason = StopReason::Exception;
            }
            const auto now = clock.Now();
            if (now < cleanupStarted || now - cleanupStarted >= CleanupDeadlineMs)
            {
                snapshot[CleanupDeadlineExceeded] = 1;
                reason = StopReason::CleanupError;
            }
            if (ended)
                break;
            try
            {
                observer.Read(snapshot);
            }
            catch (...)
            {
            }
            snapshot[Phase] = 3;
            snapshot[Reason] = static_cast<std::uint32_t>(reason);
            ++snapshot[Sequence];
            try
            {
                request.publish(snapshot.data(), static_cast<std::uint32_t>(count));
            }
            catch (...)
            {
                if (!snapshot[CleanupDeadlineExceeded])
                    reason = StopReason::Exception;
            }
            clock.Wait(snapshot[CleanupDeadlineExceeded] ? CleanupBackoffMs : SampleIntervalMs);
        }
        bool endedState = false;
        try
        {
            endedState = observer.Read(snapshot);
        }
        catch (...)
        {
        }
        snapshot[Phase] = 2;
        ++snapshot[Sequence];
        snapshot[CleanupPending] = 0;
        // End has succeeded even when Begin never obtained an observer. Such a
        // clean cancellation/factory rejection has no native state to invent.
        snapshot[CleanupComplete] = endedState ? (!snapshot[Active] && !snapshot[NativeCleanupPending] ? 1u : 0u) : (!snapshot[ObserverStarted] ? 1u : 0u);
        snapshot[Reason] = static_cast<std::uint32_t>(snapshot[CleanupComplete] ? reason : StopReason::CleanupError);
        try
        {
            request.publish(snapshot.data(), static_cast<std::uint32_t>(count));
        }
        catch (...)
        {
        }
        return snapshot;
    }
}
