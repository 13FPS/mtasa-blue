/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Value-only lifecycle for Core's first native clock reference
 *****************************************************************************/
#pragma once

#include "DefinitiveInputReference.h"
#include <game/CGameClockService.h>
#include <array>
#include <cstdint>

class CGameAdapterModule;

namespace DefinitiveClockReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 96;
    inline constexpr std::uint32_t MaximumDurationMs = 10000;
    inline constexpr std::uint32_t ReadyTimeoutMs = 10000;
    inline constexpr std::uint32_t HeartbeatMs = 50;
    inline constexpr std::uint32_t MaximumPulseGapMs = 250;
    inline constexpr std::uint32_t AcknowledgementTimeoutMs = 1000;
    inline constexpr std::uint32_t CleanupDeadlineMs = 2000;
    inline constexpr std::uint32_t CleanupBackoffMs = 250;

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
        // Fields88..95 stay zero. This reference never creates a CGame facade.
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
        PreflightRejected
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
        if (values[Version] != ApiVersion || values[Phase] != 2 || !values[CleanupComplete] || values[CleanupPending])
            return false;
        if (values[NotEntered])
            return !values[BeginAttempted] && values[AdapterStopped];
        if (!values[BeginAttempted])
            return true;
        if (values[RefusedNoGeneration])
            return values[ResetSucceeded] != 0;
        return values[EndSucceeded] && values[NativeStateValid] && values[NativeRestored] && !values[NativeFrozen] && !values[NativeHooked] &&
               !values[NativeBound] && !values[NativeActive] && !values[NativeReady] && !values[NativeInFlight] && !values[PatchInstalled] &&
               !values[RetainedPeerThreads] && !values[PatchRecoveryPending] && !values[DataRecoveryPending] && !values[ProtectionRestorePending] &&
               !values[PendingSequenceLow] && !values[PendingSequenceHigh];
    }

    inline bool CopyNative(Snapshot& values, const mta::game_de::ClockServiceStateV1& state, std::uint64_t generation) noexcept
    {
        using namespace mta::game_de;
        if (state.size != sizeof(state) || state.version != ClockServiceVersionV1 || !generation || state.generation != generation || state.reserved[0] ||
            state.reserved[1] || state.bound > 1 || state.hooked > 1 || state.active > 1 || state.ready > 1 || state.stopping > 1 || state.restored > 1 ||
            state.frozen > 1 || state.modulePinned > 1 || state.patchInstalled > 1 || state.patchRecoveryPending > 1 || state.dataRecoveryPending > 1 ||
            state.protectionRestorePending > 1 || static_cast<std::uint32_t>(state.failure) > static_cast<std::uint32_t>(ClockResult::OriginalException) ||
            static_cast<std::uint32_t>(state.lastCommandResult) > static_cast<std::uint32_t>(ClockResult::OriginalException))
            return false;
        values[NativeStateValid] = 1;
        SetPair(values, GenerationLow, state.generation);
        values[BackendPinnedExpected] = state.modulePinned;
        values[EngineThread] = state.engineThread;
        values[ProducerThread] = state.producerThread;
        values[NativeBound] = state.bound;
        values[NativeHooked] = state.hooked;
        values[NativeActive] = state.active;
        values[NativeReady] = state.ready;
        values[NativeStopping] = state.stopping;
        values[NativeRestored] = state.restored;
        values[NativeFrozen] = state.frozen;
        values[NativeFailure] = static_cast<std::uint32_t>(state.failure);
        values[LastCommandResult] = static_cast<std::uint32_t>(state.lastCommandResult);
        values[Hour] = state.hour;
        values[Minute] = state.minute;
        values[Seconds] = state.seconds;
        values[NativeTimer] = state.nativeTimer;
        values[LastMinuteChange] = state.lastMinuteChange;
        values[MillisecondsPerGameMinute] = state.millisecondsPerGameMinute;
        SetPair(values, LastCompletedSequenceLow, state.lastCompletedSequence);
        SetPair(values, PendingSequenceLow, state.pendingSequence);
        SetPair(values, CallbacksLow, state.callbacks);
        SetPair(values, QualifiedCallbacksLow, state.qualifiedCallbacks);
        SetPair(values, ForeignCallbacksLow, state.foreignCallbacks);
        SetPair(values, NestedCallbacksLow, state.nestedCallbacks);
        SetPair(values, ReadsLow, state.reads);
        SetPair(values, SetsLow, state.sets);
        SetPair(values, FreezesLow, state.freezes);
        SetPair(values, UnfreezesLow, state.unfreezes);
        SetPair(values, ResetsLow, state.resets);
        SetPair(values, RenewalsLow, state.renewals);
        SetPair(values, RestoreAttemptsLow, state.restoreAttempts);
        SetPair(values, RestoreFailuresLow, state.restoreFailures);
        values[NativeInFlight] = state.inFlight;
        values[PatchInstalled] = state.patchInstalled;
        values[RetainedPeerThreads] = state.retainedPeerThreads;
        values[PatchRecoveryPending] = state.patchRecoveryPending;
        values[DataRecoveryPending] = state.dataRecoveryPending;
        values[ProtectionRestorePending] = state.protectionRestorePending;
        return true;
    }

    // The worker supplies atomic value-copy callbacks. This lifecycle performs
    // no logging, GUI work, module loading or allocation after native Begin.
    // Its stack and Core owner survive cancellation until native cleanup proves
    // retirement; a deadline changes diagnostics/backoff, never ownership.
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
        std::uint64_t       generation = 0, nextCommand = 0, pending = 0, sentAt = 0;
        std::uint64_t       workloadStarted = 0;
        bool                ready = false, readOnly = true;
        bool                haveTimer = false;
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
            if (native.sets || native.freezes || native.unfreezes || native.resets || native.frozen || native.patchInstalled || native.patchRecoveryPending ||
                native.dataRecoveryPending || native.protectionRestorePending)
                readOnly = false;
            if (haveTimer && native.nativeTimer != previousTimer)
                increment(values[NativeTimerProgressSamples]);
            previousTimer = native.nativeTimer;
            haveTimer = true;
            return true;
        };
        if (!request.durationMs || request.durationMs > MaximumDurationMs || !request.cancelled || !request.publish)
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
                if (!readOnly)
                {
                    reason = StopReason::UnexpectedWrites;
                    break;
                }
                if (native.failure != ClockResult::Ok || !native.active || !native.bound || !native.hooked)
                {
                    reason = StopReason::NativeError;
                    break;
                }
                if (native.ready && (!native.engineThread || !native.qualifiedCallbacks || !native.modulePinned))
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
                        if (native.lastCompletedSequence > pending)
                        {
                            reason = StopReason::NativeError;
                            break;
                        }
                        if (native.lastCompletedSequence == pending)
                        {
                            if (native.lastCommandResult != ClockResult::Ok || !native.reads)
                            {
                                reason = StopReason::NativeError;
                                break;
                            }
                            increment(values[ReadAcknowledged]);
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
                        else
                        {
                            values[WorkloadComplete] = 1;
                            reason = values[ReadAcknowledged] && values[NativeTimerProgressSamples] ? StopReason::Completed : StopReason::NativeError;
                        }
                        break;
                    }
                    if (!pending)
                    {
                        ClockCommandV1 command;
                        command.generation = generation;
                        command.sequence = ++nextCommand;
                        command.operation = ClockOperation::Read;
                        if (!owner.Submit(command))
                        {
                            increment(values[SubmitFailures]);
                            reason = StopReason::SubmitError;
                            break;
                        }
                        pending = command.sequence;
                        sentAt = now;
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
                break;
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
        values[ReadOnlyVerified] = readOnly && values[BeginSucceeded] && values[ReadAcknowledged] ? 1u : 0u;
        values[CleanupPending] = 0;
        values[CleanupComplete] = 1;
        values[Phase] = 2;
        publish();
        return values;
    }

    Snapshot Run(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept;
    Snapshot PublishNotEntered(const DefinitiveInputReference::Request& request, bool adapterStopped) noexcept;
}
