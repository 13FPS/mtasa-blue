/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Value-only Core application-frame and startup dictionary observation
 ****************************************************************************/
#pragma once
#include "DefinitiveClockReference.h"
#include <game/CGameApplicationFrameObservation.h>
#include <cmath>
#include <cstring>

class CGameAdapterModule;
namespace DefinitiveApplicationFrameReference
{
    inline constexpr std::uint32_t ApiVersion = 1, FieldCount = 128;
    inline constexpr std::uint32_t MaximumDurationMs = 10000, ReadyTimeoutMs = 10000, HeartbeatMs = 50;
    inline constexpr std::uint32_t MaximumPulseGapMs = 250, CleanupDeadlineMs = 2000, CleanupBackoffMs = 250;
    using StopReason = DefinitiveClockReference::StopReason;
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    struct Request
    {
        std::uint32_t                             durationMs = MaximumDurationMs;
        DefinitiveInputReference::CancelCallback  cancelled = nullptr;
        DefinitiveInputReference::PublishCallback publish = nullptr;
    };
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
        // The original ownership prefix keeps its reserved words unchanged.
        ObservationReads = 96,
        ObservationValid,
        FramesLow,
        FramesHigh,
        RawNativeState,
        DeltaBits,
        FrontendFramesLow,
        FrontendFramesHigh,
        PlayingFramesLow,
        PlayingFramesHigh,
        LoadingLookupsLow,
        LoadingLookupsHigh,
        LoadingKeyPresentLow,
        LoadingKeyPresentHigh,
        LoadingNonNullLow,
        LoadingNonNullHigh,
        SimpleFadeLookupsLow,
        SimpleFadeLookupsHigh,
        SimpleFadeKeyPresentLow,
        SimpleFadeKeyPresentHigh,
        SimpleFadeNonNullLow,
        SimpleFadeNonNullHigh,
        LookupFailuresLow,
        LookupFailuresHigh,
        LastLookupMask,
        ObservationProgressSamples,
        ObservationReadFailures,
        InvalidObservationSamples,
        DictionaryVerified,
        // Words125..127 stay zero. No native address or CGame is published.
    };

    inline void SetPair(Snapshot& values, Field low, std::uint64_t value) noexcept
    {
        values[low] = static_cast<std::uint32_t>(value);
        values[low + 1] = static_cast<std::uint32_t>(value >> 32);
    }
    inline bool CopyNative(Snapshot& values, const mta::game_de::ClockServiceStateV1& state, std::uint64_t generation) noexcept
    {
        // Reuse ownership validation without upgrading the clock report's
        // command/acknowledgement semantics or changing its 96-word schema.
        DefinitiveClockReference::Snapshot prefix{};
        for (std::size_t i = 0; i < prefix.size(); ++i)
            prefix[i] = values[i];
        if (!DefinitiveClockReference::CopyNative(prefix, state, generation))
            return false;
        for (std::size_t i = 0; i < prefix.size(); ++i)
            values[i] = prefix[i];
        return true;
    }
    inline bool Retired(const Snapshot& values) noexcept
    {
        DefinitiveClockReference::Snapshot prefix{};
        for (std::size_t i = 0; i < prefix.size(); ++i)
            prefix[i] = values[i];
        return DefinitiveClockReference::Retired(prefix);
    }
    inline bool Completed(const Snapshot& values) noexcept
    {
        return Retired(values) && values[Reason] == static_cast<std::uint32_t>(StopReason::Completed) && values[WorkloadComplete] && values[ReadOnlyVerified] &&
               values[ObservationValid] && values[ReadyObserved] && values[ObservationProgressSamples] && (values[FramesLow] || values[FramesHigh]) &&
               !values[FullMtaReady];
    }
    inline bool CopyObservation(Snapshot& values, const mta::game_de::ApplicationFrameObservationStateV1& state, std::uint64_t generation,
                                std::uint32_t engineThread) noexcept
    {
        using namespace mta::game_de;
        float delta = 0;
        std::memcpy(&delta, &state.deltaBits, sizeof(delta));
        const auto mask = state.lastLookupMask;
        if (state.size != sizeof(state) || state.version != ApplicationFrameObservationVersionV1 || !generation || state.generation != generation ||
            state.reserved[0] || state.reserved[1] || state.reserved[2] || state.reserved[3] || state.nativeState > 9 || mask > 63 ||
            (state.frames && (!state.engineThread || state.engineThread != engineThread || !std::isfinite(delta))) || state.frontendFrames > state.frames ||
            state.playingFrames > state.frames - state.frontendFrames || state.loadingLookups > state.frames || state.simpleFadeLookups > state.frames ||
            state.loadingKeyPresent > state.loadingLookups || state.loadingNonNull > state.loadingKeyPresent ||
            state.simpleFadeKeyPresent > state.simpleFadeLookups || state.simpleFadeNonNull > state.simpleFadeKeyPresent ||
            ((mask & LoadingPresent) && !(mask & LoadingObserved)) || ((mask & LoadingNonNull) && !(mask & LoadingPresent)) ||
            ((mask & SimpleFadePresent) && !(mask & SimpleFadeObserved)) || ((mask & SimpleFadeNonNull) && !(mask & SimpleFadePresent)))
            return false;
        values[ObservationValid] = 1;
        SetPair(values, FramesLow, state.frames);
        values[RawNativeState] = state.nativeState;
        values[DeltaBits] = state.deltaBits;
        SetPair(values, FrontendFramesLow, state.frontendFrames);
        SetPair(values, PlayingFramesLow, state.playingFrames);
        SetPair(values, LoadingLookupsLow, state.loadingLookups);
        SetPair(values, LoadingKeyPresentLow, state.loadingKeyPresent);
        SetPair(values, LoadingNonNullLow, state.loadingNonNull);
        SetPair(values, SimpleFadeLookupsLow, state.simpleFadeLookups);
        SetPair(values, SimpleFadeKeyPresentLow, state.simpleFadeKeyPresent);
        SetPair(values, SimpleFadeNonNullLow, state.simpleFadeNonNull);
        SetPair(values, LookupFailuresLow, state.lookupFailures);
        values[LastLookupMask] = mask;
        values[DictionaryVerified] = state.loadingLookups && state.simpleFadeLookups && !state.lookupFailures ? 1u : 0u;
        return true;
    }
    template <typename Owner, typename Clock>
    Snapshot RunLifecycle(Owner& owner, Clock& clock, const Request& request) noexcept
    {
        using namespace mta::game_de;
        static_assert(noexcept(clock.Now()) && noexcept(clock.Wait(std::uint32_t{})));
        Snapshot values{};
        values[Version] = ApiVersion;
        auto                               reason = StopReason::Running;
        const auto                         started = clock.Now();
        auto                               lastPulse = started;
        std::uint64_t                      generation = 0, workloadStarted = 0, previousFrames = 0;
        bool                               ready = false, readOnly = true;
        ClockServiceStateV1                native;
        ApplicationFrameObservationStateV1 observation, previousObservation;
        bool                               haveObservation = false;
        const auto saturate = [](std::uint64_t value) noexcept { return static_cast<std::uint32_t>(value > 0xffffffffu ? 0xffffffffu : value); };
        const auto increment = [](std::uint32_t& value) noexcept
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
            if (native.reads || native.sets || native.freezes || native.unfreezes || native.resets || native.frozen || native.patchInstalled ||
                native.patchRecoveryPending || native.dataRecoveryPending || native.protectionRestorePending || native.lastCompletedSequence ||
                native.pendingSequence)
                readOnly = false;
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
                if (native.failure != ClockResult::Ok || !native.active || !native.bound || !native.hooked ||
                    (native.ready && (!native.engineThread || !native.qualifiedCallbacks || !native.modulePinned)))
                {
                    reason = StopReason::NativeError;
                    break;
                }
                increment(values[ObservationReads]);
                if (!owner.ReadObservation(generation, observation))
                {
                    values[ObservationValid] = 0;
                    increment(values[ObservationReadFailures]);
                    reason = StopReason::ReadError;
                    break;
                }
                const bool regressed =
                    haveObservation &&
                    (observation.frames < previousObservation.frames || observation.frontendFrames < previousObservation.frontendFrames ||
                     observation.playingFrames < previousObservation.playingFrames || observation.loadingLookups < previousObservation.loadingLookups ||
                     observation.loadingKeyPresent < previousObservation.loadingKeyPresent || observation.loadingNonNull < previousObservation.loadingNonNull ||
                     observation.simpleFadeLookups < previousObservation.simpleFadeLookups ||
                     observation.simpleFadeKeyPresent < previousObservation.simpleFadeKeyPresent ||
                     observation.simpleFadeNonNull < previousObservation.simpleFadeNonNull || observation.lookupFailures < previousObservation.lookupFailures);
                if (regressed || !CopyObservation(values, observation, generation, native.engineThread))
                {
                    values[ObservationValid] = 0;
                    increment(values[InvalidObservationSamples]);
                    reason = StopReason::NativeError;
                    break;
                }
                if (previousFrames && observation.frames > previousFrames)
                    increment(values[ObservationProgressSamples]);
                previousFrames = observation.frames;
                previousObservation = observation;
                haveObservation = true;
                if (!ready && native.ready && observation.frames)
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
                    if (now < workloadStarted || now - workloadStarted >= request.durationMs)
                    {
                        values[WorkloadComplete] = 1;
                        reason = values[ObservationProgressSamples] ? StopReason::Completed : StopReason::NativeError;
                        break;
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
        // Cleanup deadlines alter diagnostics/backoff, never native ownership.
        // Keep the Core stack and callback context until retirement is proved.
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
        values[ReadOnlyVerified] = readOnly && values[BeginSucceeded] && values[ObservationValid] ? 1u : 0u;
        values[CleanupPending] = 0;
        values[CleanupComplete] = 1;
        values[Phase] = 2;
        publish();
        return values;
    }
    Snapshot Run(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept;
    Snapshot PublishNotEntered(const DefinitiveInputReference::Request& request, bool adapterStopped) noexcept;
}
