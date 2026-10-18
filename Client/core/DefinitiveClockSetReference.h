/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded Core-owned Set/readback/restoration via MTA's existing CClock
 ****************************************************************************/
#pragma once

#include "DefinitiveClockReference.h"

namespace DefinitiveClockSetReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 128;
    inline constexpr std::uint32_t MaximumDurationMs = 5000;
    inline constexpr std::uint32_t MinimumDurationMs = 200;
    inline constexpr std::uint32_t ReadyTimeoutMs = DefinitiveClockReference::ReadyTimeoutMs;
    inline constexpr std::uint32_t HeartbeatMs = DefinitiveClockReference::HeartbeatMs;
    inline constexpr std::uint32_t MaximumPulseGapMs = DefinitiveClockReference::MaximumPulseGapMs;
    inline constexpr std::uint32_t AcknowledgementTimeoutMs = DefinitiveClockReference::AcknowledgementTimeoutMs;
    inline constexpr std::uint32_t CleanupDeadlineMs = DefinitiveClockReference::CleanupDeadlineMs;
    inline constexpr std::uint32_t CleanupBackoffMs = DefinitiveClockReference::CleanupBackoffMs;

    // Fields0..87 retain the read-only reference's meaning. In particular
    // ReadOnlyVerified remains false rather than being relabelled as Set proof.
    enum Field : std::uint32_t
    {
        Version,
        Phase,
        Sequence,
        ElapsedMs,
        Reason,
        CleanupPending,
        CleanupComplete,
        CleanupDeadlineExceeded,
        BeginAttempted,
        BeginSucceeded,
        ReadyObserved,
        WorkloadComplete,
        ReadOnlyVerified,
        BackendPinnedExpected,
        GenerationLow,
        GenerationHigh,
        EngineThread,
        ProducerThread,
        NativeBound,
        NativeHooked,
        NativeActive,
        NativeReady,
        NativeStopping,
        NativeRestored,
        NativeFrozen,
        NativeFailure,
        LastCommandResult,
        Hour,
        Minute,
        Seconds,
        NativeTimer,
        LastMinuteChange,
        MillisecondsPerGameMinute,
        ReadSubmitted,
        ReadAcknowledged,
        AcknowledgementTimeouts,
        RenewalAttempts,
        RenewalFailures,
        FocusLosses,
        PulseGaps,
        ReadFailures,
        SubmitFailures,
        CleanupAttempts,
        EndSucceeded,
        ResetSucceeded,
        NativeStateReads,
        NativeStateValid,
        RefusedNoGeneration,
        LastCompletedSequenceLow,
        LastCompletedSequenceHigh,
        PendingSequenceLow,
        PendingSequenceHigh,
        CallbacksLow,
        CallbacksHigh,
        QualifiedCallbacksLow,
        QualifiedCallbacksHigh,
        ForeignCallbacksLow,
        ForeignCallbacksHigh,
        NestedCallbacksLow,
        NestedCallbacksHigh,
        ReadsLow,
        ReadsHigh,
        SetsLow,
        SetsHigh,
        FreezesLow,
        FreezesHigh,
        UnfreezesLow,
        UnfreezesHigh,
        ResetsLow,
        ResetsHigh,
        RenewalsLow,
        RenewalsHigh,
        RestoreAttemptsLow,
        RestoreAttemptsHigh,
        RestoreFailuresLow,
        RestoreFailuresHigh,
        NativeInFlight,
        PatchInstalled,
        RetainedPeerThreads,
        PatchRecoveryPending,
        DataRecoveryPending,
        ProtectionRestorePending,
        MaximumObservedPulseGap,
        NativeTimerProgressSamples,
        WorkloadElapsedMs,
        FullMtaReady,
        NotEntered,
        AdapterStopped,
        InitialReadAcknowledged = 88,
        InitialHour,
        InitialMinute,
        InitialSeconds,
        InitialNativeTimer,
        InitialLastMinuteChange,
        InitialMillisecondsPerGameMinute,
        TargetHour,
        TargetMinute,
        SetSubmitted,
        SetAcknowledged,
        SetSequenceLow,
        SetSequenceHigh,
        SetAcknowledgementNativeTimer,
        SetAcknowledgementHour,
        SetAcknowledgementMinute,
        SetAcknowledgementSeconds,
        CoherentSetReadback,
        SubsequentReadbackVerified,
        SubsequentReadAcknowledged,
        ProviderRestorationVerified,
        ExactlyOneSetVerified,
        ForbiddenOperationObserved,
        TargetProgressSamples,
        SetReadbackMismatch,
        RateRejectedBeforeWrite,
        SetAcknowledgementElapsedMs,
        LastReadbackNativeTimer,
        LastReadbackHour,
        LastReadbackMinute,
        LastReadbackSeconds,
        InitialReadSequenceLow,
        InitialReadSequenceHigh,
        ProviderCapturedSnapshotNotCompared,
        // Fields122..127 stay zero. Initial producer Read is not the provider's capture.
    };

    enum class StopReason : std::uint32_t
    {
        Running,
        Completed,
        Cancelled,
        InvalidRequest,
        BeginError,
        ReadyTimeout,
        NativeError,
        ReadError,
        SubmitError,
        LeaseError,
        PulseGap,
        FocusLost,
        AcknowledgementTimeout,
        UnexpectedWrites,
        Exception,
        CleanupError,
        PreflightRejected,
        UnsupportedRate,
        IncorrectReadback
    };

    using Snapshot = std::array<std::uint32_t, FieldCount>;
    struct Request
    {
        std::uint32_t                             durationMs = MaximumDurationMs;
        DefinitiveInputReference::CancelCallback  cancelled = nullptr;
        DefinitiveInputReference::PublishCallback publish = nullptr;
    };

    inline void SetPair(Snapshot& values, Field first, std::uint64_t value) noexcept
    {
        values[first] = static_cast<std::uint32_t>(value);
        values[first + 1] = static_cast<std::uint32_t>(value >> 32);
    }

    inline bool Retired(const Snapshot& values) noexcept
    {
        DefinitiveClockReference::Snapshot common{};
        for (std::uint32_t index = 0; index < 88; ++index)
            common[index] = values[index];
        return DefinitiveClockReference::Retired(common);
    }

    inline bool TargetValues(const Snapshot& values, std::uint32_t hour, std::uint32_t minute, std::uint32_t seconds, std::uint32_t timer) noexcept
    {
        const std::uint32_t nativeElapsed = timer - values[InitialNativeTimer];
        return hour == values[TargetHour] && minute >= values[TargetMinute] && seconds <= 59 &&
               nativeElapsed <= MaximumDurationMs + AcknowledgementTimeoutMs + MaximumPulseGapMs && minute <= values[TargetMinute] + nativeElapsed / 1000 + 1;
    }

    inline bool Completed(const Snapshot& values) noexcept
    {
        if (!Retired(values) || values[Reason] != static_cast<std::uint32_t>(StopReason::Completed) || !values[BeginSucceeded] || !values[ReadyObserved] ||
            !values[WorkloadComplete] || values[ReadOnlyVerified] || !values[InitialReadAcknowledged] || values[InitialMillisecondsPerGameMinute] != 1000 ||
            values[SetSubmitted] != 1 || values[SetAcknowledged] != 1 || !values[CoherentSetReadback] || !values[SubsequentReadbackVerified] ||
            !values[SubsequentReadAcknowledged] || !values[ProviderRestorationVerified] || !values[ProviderCapturedSnapshotNotCompared] ||
            !values[ExactlyOneSetVerified] || values[ForbiddenOperationObserved] || values[SetReadbackMismatch] || values[RateRejectedBeforeWrite] ||
            !values[TargetProgressSamples] || !values[NativeTimerProgressSamples] || (!values[QualifiedCallbacksLow] && !values[QualifiedCallbacksHigh]) ||
            values[CleanupDeadlineExceeded] || values[FullMtaReady] || values[NotEntered])
            return false;
        if (values[TargetHour] != (values[InitialHour] == 12 ? 6u : 12u) || values[TargetMinute] != 15 || values[InitialHour] > 23 ||
            values[InitialMinute] > 59 || values[InitialSeconds] > 59 || values[InitialReadSequenceLow] != 1 || values[InitialReadSequenceHigh] ||
            values[SetSequenceLow] != 2 || values[SetSequenceHigh] || values[SetsLow] != 1 || values[SetsHigh] || values[FreezesLow] || values[FreezesHigh] ||
            values[UnfreezesLow] || values[UnfreezesHigh] || values[ResetsLow] || values[ResetsHigh] || values[NativeFailure] || values[LastCommandResult])
            return false;
        if (!TargetValues(values, values[SetAcknowledgementHour], values[SetAcknowledgementMinute], values[SetAcknowledgementSeconds],
                          values[SetAcknowledgementNativeTimer]) ||
            !TargetValues(values, values[LastReadbackHour], values[LastReadbackMinute], values[LastReadbackSeconds], values[LastReadbackNativeTimer]) ||
            values[LastReadbackNativeTimer] == values[SetAcknowledgementNativeTimer] || values[SetAcknowledgementElapsedMs] >= AcknowledgementTimeoutMs ||
            values[WorkloadElapsedMs] < MinimumDurationMs || values[WorkloadElapsedMs] > MaximumDurationMs + MaximumPulseGapMs ||
            values[ReadAcknowledged] != values[SubsequentReadAcknowledged] + 1 || values[ReadSubmitted] != values[ReadAcknowledged])
            return false;
        for (std::uint32_t index = 122; index < FieldCount; ++index)
            if (values[index])
                return false;
        return true;
    }

    inline bool CopyNative(Snapshot& values, const mta::game_de::ClockServiceStateV1& state, std::uint64_t generation) noexcept
    {
        DefinitiveClockReference::Snapshot common{};
        for (std::uint32_t index = 0; index < 88; ++index)
            common[index] = values[index];
        if (!DefinitiveClockReference::CopyNative(common, state, generation))
            return false;
        // Share the existing SDK wire validation and numeric common-field map.
        // Extended Set evidence is sticky and must survive provider retirement.
        for (std::uint32_t index = 13; index <= 81; ++index)
            values[index] = common[index];
        return true;
    }

    // Only value locals and atomic callbacks are permitted after Begin. Failed
    // retirement retains this stack/owner indefinitely, including after a
    // deadline; releasing Core while a peer is held would violate ownership.
    template <typename Owner, typename Clock>
    Snapshot RunLifecycle(Owner& owner, Clock& clock, const Request& request) noexcept
    {
        using namespace mta::game_de;
        static_assert(noexcept(clock.Now()) && noexcept(clock.Wait(std::uint32_t{})));
        Snapshot values{};
        values[Version] = ApiVersion;
        StopReason          reason = StopReason::Running;
        const auto          started = clock.Now();
        auto                lastPulse = started;
        std::uint64_t       generation = 0, nextCommand = 0, pending = 0, sentAt = 0, workloadStarted = 0;
        std::uint64_t       maxSets = 0;
        ClockOperation      pendingOperation = ClockOperation::Read;
        bool                ready = false, haveTimer = false;
        std::uint32_t       previousTimer = 0;
        ClockServiceStateV1 native;
        const auto          saturate = [](std::uint64_t value) noexcept { return static_cast<std::uint32_t>(value > 0xffffffffu ? 0xffffffffu : value); };
        const auto          increment = [](std::uint32_t& value) noexcept
        {
            if (value != 0xffffffffu)
                ++value;
        };
        const auto elapsed = [&]() noexcept
        {
            const auto now = clock.Now();
            values[ElapsedMs] = now >= started ? saturate(now - started) : 0;
            return now;
        };
        const auto publish = [&]() noexcept
        {
            elapsed();
            values[Reason] = static_cast<std::uint32_t>(reason);
            increment(values[Sequence]);
            if (request.publish)
                try
                {
                    request.publish(values.data(), FieldCount);
                }
                catch (...)
                {
                    reason = StopReason::Exception;
                    values[Reason] = static_cast<std::uint32_t>(reason);
                }
        };
        const auto copy = [&]() noexcept
        {
            if (!CopyNative(values, native, generation))
            {
                values[NativeStateValid] = 0;
                return false;
            }
            if (native.sets > maxSets)
                maxSets = native.sets;
            if (native.freezes || native.unfreezes || native.resets || native.frozen || native.patchInstalled || native.patchRecoveryPending ||
                native.protectionRestorePending || (native.dataRecoveryPending && !values[SetSubmitted]) || native.sets > 1 ||
                native.sets > values[SetSubmitted])
                values[ForbiddenOperationObserved] = 1;
            if (haveTimer && native.nativeTimer != previousTimer)
                increment(values[NativeTimerProgressSamples]);
            previousTimer = native.nativeTimer;
            haveTimer = true;
            return true;
        };
        const auto targetReadback = [&]() noexcept
        {
            // Set changes only hour/minute/phase, matching CClock. Distinct
            // non-midnight target hours cannot arise naturally in this bounded
            // 1000ms/game-minute recipe. Allow at most one callback boundary
            // of rounding while native time progresses between observations.
            return native.millisecondsPerGameMinute == 1000 && TargetValues(values, native.hour, native.minute, native.seconds, native.nativeTimer);
        };
        if (request.durationMs < MinimumDurationMs || request.durationMs > MaximumDurationMs || !request.cancelled || !request.publish)
            reason = StopReason::InvalidRequest;
        try
        {
            if (reason == StopReason::Running && request.cancelled())
                reason = StopReason::Cancelled;
            if (reason == StopReason::Running && !owner.Focused())
            {
                increment(values[FocusLosses]);
                reason = StopReason::FocusLost;
            }
            if (reason == StopReason::Running)
            {
                ClockServiceRequestV1 begin;
                values[BeginAttempted] = 1;
                const bool began = owner.Begin(begin, native);
                generation = native.generation;
                if (generation)
                    copy();
                else
                    values[RefusedNoGeneration] = 1;
                if (!began)
                    reason = StopReason::BeginError;
                else if (!generation || !values[NativeStateValid])
                    reason = StopReason::NativeError;
                else
                    values[BeginSucceeded] = 1;
                lastPulse = clock.Now();
            }
            const auto readinessStarted = clock.Now();
            while (reason == StopReason::Running)
            {
                const auto now = elapsed();
                if (request.cancelled())
                {
                    reason = StopReason::Cancelled;
                    break;
                }
                if (!owner.Focused())
                {
                    increment(values[FocusLosses]);
                    reason = StopReason::FocusLost;
                    break;
                }
                const auto gap = now >= lastPulse ? now - lastPulse : 0xffffffffffffffffull;
                if (gap > values[MaximumObservedPulseGap])
                    values[MaximumObservedPulseGap] = saturate(gap);
                if (now < started || now < lastPulse || gap > MaximumPulseGapMs)
                {
                    increment(values[PulseGaps]);
                    reason = StopReason::PulseGap;
                    break;
                }
                lastPulse = now;
                increment(values[RenewalAttempts]);
                if (!owner.Renew(generation))
                {
                    increment(values[RenewalFailures]);
                    reason = StopReason::LeaseError;
                    break;
                }
                increment(values[NativeStateReads]);
                if (!owner.Read(native))
                {
                    values[NativeStateValid] = 0;
                    increment(values[ReadFailures]);
                    reason = StopReason::ReadError;
                    break;
                }
                if (!copy())
                {
                    reason = StopReason::NativeError;
                    break;
                }
                if (values[ForbiddenOperationObserved])
                {
                    reason = StopReason::UnexpectedWrites;
                    break;
                }
                if (native.failure != ClockResult::Ok || !native.active || !native.bound || !native.hooked || native.stopping ||
                    (native.ready && (!native.engineThread || !native.qualifiedCallbacks || !native.modulePinned)))
                {
                    reason = StopReason::NativeError;
                    break;
                }
                if (!ready && native.ready)
                {
                    ready = true;
                    workloadStarted = now;
                    values[ReadyObserved] = 1;
                }
                if (!ready)
                {
                    if (now < readinessStarted || now - readinessStarted >= ReadyTimeoutMs)
                    {
                        reason = StopReason::ReadyTimeout;
                        break;
                    }
                }
                else
                {
                    values[WorkloadElapsedMs] = now >= workloadStarted ? saturate(now - workloadStarted) : 0;
                    if (pending)
                    {
                        if (now < sentAt || now - sentAt >= AcknowledgementTimeoutMs)
                        {
                            increment(values[AcknowledgementTimeouts]);
                            reason = StopReason::AcknowledgementTimeout;
                            break;
                        }
                        if (native.lastCompletedSequence > pending)
                        {
                            reason = StopReason::NativeError;
                            break;
                        }
                        if (native.lastCompletedSequence == pending)
                        {
                            if (native.lastCommandResult != ClockResult::Ok || native.pendingSequence)
                            {
                                reason = StopReason::NativeError;
                                break;
                            }
                            if (pendingOperation == ClockOperation::Set)
                            {
                                if (native.sets != 1 || !targetReadback())
                                {
                                    values[SetReadbackMismatch] = 1;
                                    reason = StopReason::IncorrectReadback;
                                    break;
                                }
                                values[SetAcknowledged] = values[CoherentSetReadback] = 1;
                                values[SetAcknowledgementNativeTimer] = native.nativeTimer;
                                values[SetAcknowledgementHour] = native.hour;
                                values[SetAcknowledgementMinute] = native.minute;
                                values[SetAcknowledgementSeconds] = native.seconds;
                                values[SetAcknowledgementElapsedMs] = saturate(now - sentAt);
                            }
                            else
                            {
                                if (native.reads != static_cast<std::uint64_t>(values[ReadAcknowledged]) + 1)
                                {
                                    reason = StopReason::NativeError;
                                    break;
                                }
                                increment(values[ReadAcknowledged]);
                                if (!values[InitialReadAcknowledged])
                                {
                                    if (native.hour > 23 || native.minute > 59 || native.seconds > 59)
                                    {
                                        reason = StopReason::NativeError;
                                        break;
                                    }
                                    values[InitialReadAcknowledged] = 1;
                                    values[InitialHour] = native.hour;
                                    values[InitialMinute] = native.minute;
                                    values[InitialSeconds] = native.seconds;
                                    values[InitialNativeTimer] = native.nativeTimer;
                                    values[InitialLastMinuteChange] = native.lastMinuteChange;
                                    values[InitialMillisecondsPerGameMinute] = native.millisecondsPerGameMinute;
                                    SetPair(values, InitialReadSequenceLow, pending);
                                    if (native.millisecondsPerGameMinute != 1000)
                                    {
                                        values[RateRejectedBeforeWrite] = 1;
                                        reason = StopReason::UnsupportedRate;
                                        break;
                                    }
                                    values[TargetHour] = native.hour == 12 ? 6u : 12u;
                                    values[TargetMinute] = 15;
                                }
                                else
                                {
                                    if (!values[SetAcknowledged] || native.sets != 1 || !targetReadback())
                                    {
                                        values[SetReadbackMismatch] = 1;
                                        reason = StopReason::IncorrectReadback;
                                        break;
                                    }
                                    values[SubsequentReadbackVerified] = 1;
                                    increment(values[SubsequentReadAcknowledged]);
                                    if (native.nativeTimer != values[SetAcknowledgementNativeTimer])
                                        increment(values[TargetProgressSamples]);
                                    values[LastReadbackNativeTimer] = native.nativeTimer;
                                    values[LastReadbackHour] = native.hour;
                                    values[LastReadbackMinute] = native.minute;
                                    values[LastReadbackSeconds] = native.seconds;
                                }
                            }
                            pending = 0;
                        }
                        else if (now < sentAt || now - sentAt >= AcknowledgementTimeoutMs)
                        {
                            increment(values[AcknowledgementTimeouts]);
                            reason = StopReason::AcknowledgementTimeout;
                            break;
                        }
                    }
                    if (now < workloadStarted || now - workloadStarted >= request.durationMs)
                    {
                        if (pending)
                        {
                            increment(values[AcknowledgementTimeouts]);
                            reason = StopReason::AcknowledgementTimeout;
                        }
                        else if (values[SetAcknowledged] && values[SubsequentReadbackVerified] && values[TargetProgressSamples])
                        {
                            values[WorkloadComplete] = 1;
                            reason = StopReason::Completed;
                        }
                        else
                            reason = StopReason::NativeError;
                        break;
                    }
                    if (!pending)
                    {
                        ClockCommandV1 command;
                        command.generation = generation;
                        command.sequence = ++nextCommand;
                        command.operation = values[InitialReadAcknowledged] && !values[SetSubmitted] ? ClockOperation::Set : ClockOperation::Read;
                        if (command.operation == ClockOperation::Set)
                        {
                            command.hour = values[TargetHour];
                            command.minute = values[TargetMinute];
                            values[SetSubmitted] = 1;
                            SetPair(values, SetSequenceLow, command.sequence);
                        }
                        if (!owner.Submit(command))
                        {
                            increment(values[SubmitFailures]);
                            reason = StopReason::SubmitError;
                            break;
                        }
                        pending = command.sequence;
                        pendingOperation = command.operation;
                        sentAt = now;
                        if (command.operation == ClockOperation::Read)
                            increment(values[ReadSubmitted]);
                    }
                }
                values[Phase] = 1;
                publish();
                if (reason == StopReason::Running)
                    clock.Wait(HeartbeatMs);
            }
        }
        catch (...)
        {
            generation = native.generation;
            if (values[BeginAttempted] && !generation)
                values[RefusedNoGeneration] = 1;
            reason = StopReason::Exception;
        }
        const auto cleanupStarted = clock.Now();
        values[CleanupPending] = values[BeginAttempted] ? 1u : 0u;
        while (values[BeginAttempted])
        {
            increment(values[CleanupAttempts]);
            bool retired = false;
            try
            {
                if (!generation)
                {
                    if (owner.Reset())
                    {
                        values[ResetSucceeded] = 1;
                        retired = true;
                    }
                }
                else
                {
                    if (owner.End(generation))
                        values[EndSucceeded] = 1;
                    increment(values[NativeStateReads]);
                    if (owner.Read(native) && copy())
                        retired = values[EndSucceeded] && native.restored && !native.frozen && !native.hooked && !native.bound && !native.active &&
                                  !native.ready && !native.inFlight && !native.patchInstalled && !native.retainedPeerThreads && !native.patchRecoveryPending &&
                                  !native.dataRecoveryPending && !native.protectionRestorePending && !native.pendingSequence;
                    else
                    {
                        values[NativeStateValid] = 0;
                        increment(values[ReadFailures]);
                    }
                }
            }
            catch (...)
            {
                reason = StopReason::Exception;
            }
            if (retired)
            {
                if (generation)
                {
                    // End certifies restoration of the provider's earlier
                    // captured values/phase. The producer's initial Read is a
                    // later observation and cannot establish their equality.
                    values[ProviderRestorationVerified] = 1;
                    values[ProviderCapturedSnapshotNotCompared] = 1;
                }
                break;
            }
            const auto now = elapsed();
            if (now < cleanupStarted || now - cleanupStarted >= CleanupDeadlineMs)
            {
                values[CleanupDeadlineExceeded] = 1;
                reason = StopReason::CleanupError;
            }
            values[Phase] = 3;
            publish();
            clock.Wait(values[CleanupDeadlineExceeded] ? CleanupBackoffMs : HeartbeatMs);
        }
        values[ExactlyOneSetVerified] =
            values[SetSubmitted] == 1 && values[SetAcknowledged] == 1 && maxSets == 1 && !values[ForbiddenOperationObserved] ? 1u : 0u;
        // Successful providers retain counters. Keep their wire state intact
        // rather than manufacturing terminal counters if a fixture clears it.
        values[ReadOnlyVerified] = 0;
        values[CleanupPending] = 0;
        values[CleanupComplete] = 1;
        values[Phase] = 2;
        publish();
        return values;
    }

    Snapshot Run(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept;
    Snapshot PublishNotEntered(const DefinitiveInputReference::Request& request, bool adapterStopped) noexcept;
}
