/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional read-only clock transaction on a completed MTA Core owner
 *****************************************************************************/
#pragma once

#include "DefinitiveClockReference.h"
#include "DefinitiveFrontend.h"

namespace DefinitiveFrontendClockReference
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = DefinitiveClockReference::FieldCount;
    inline constexpr std::uint32_t MaximumDurationMs = DefinitiveClockReference::MaximumDurationMs;
    using Status = DefinitiveFrontend::Status;
    using Request = DefinitiveClockReference::Request;
    using Snapshot = DefinitiveClockReference::Snapshot;
    using Run = Status (*)(void*, std::uint32_t, DefinitiveInputReference::CancelCallback, DefinitiveInputReference::PublishCallback);

    inline bool ValidRequest(const Request& request) noexcept
    {
        return request.durationMs && request.durationMs <= DefinitiveClockReference::MaximumDurationMs && request.cancelled && request.publish;
    }

    inline bool RetiredForCompletedOwner(const Snapshot& values) noexcept
    {
        namespace Clock = DefinitiveClockReference;
        if (values[Clock::BeginAttempted])
            return Clock::Retired(values);
        // The completed Core intentionally keeps its backend alive. A safe
        // cancellation before native Begin therefore cannot claim the older
        // rejecting-constructor proof of AdapterStopped. Qualify that narrow
        // case independently, with no generation, command or native debt.
        const auto reason = static_cast<Clock::StopReason>(values[Clock::Reason]);
        if (values[Clock::Version] != Clock::ApiVersion || values[Clock::Phase] != 2 || !values[Clock::Sequence] || values[Clock::CleanupComplete] != 1 ||
            values[Clock::NotEntered] > 1 ||
            (reason != Clock::StopReason::Cancelled && reason != Clock::StopReason::FocusLost && reason != Clock::StopReason::InvalidRequest &&
             reason != Clock::StopReason::Exception && reason != Clock::StopReason::PreflightRejected))
            return false;
        if (values[Clock::FocusLosses] != (reason == Clock::StopReason::FocusLost ? 1u : 0u))
            return false;
        for (std::uint32_t field = 0; field < Clock::FieldCount; ++field)
        {
            if (field == Clock::Version || field == Clock::Phase || field == Clock::Sequence || field == Clock::ElapsedMs || field == Clock::Reason ||
                field == Clock::CleanupComplete || field == Clock::FocusLosses || field == Clock::NotEntered)
                continue;
            if (values[field])
                return false;
        }
        return true;
    }

    inline bool WorkloadSucceeded(const Snapshot& values) noexcept
    {
        namespace Clock = DefinitiveClockReference;
        if (!RetiredForCompletedOwner(values) || values[Clock::Reason] != static_cast<std::uint32_t>(Clock::StopReason::Completed) ||
            values[Clock::BeginAttempted] != 1 || values[Clock::BeginSucceeded] != 1 || values[Clock::ReadyObserved] != 1 ||
            values[Clock::WorkloadComplete] != 1 || values[Clock::ReadOnlyVerified] != 1 || values[Clock::BackendPinnedExpected] != 1 ||
            values[Clock::NativeStateValid] != 1 || !values[Clock::EngineThread] || !values[Clock::ProducerThread] ||
            (!values[Clock::GenerationLow] && !values[Clock::GenerationHigh]) ||
            (!values[Clock::QualifiedCallbacksLow] && !values[Clock::QualifiedCallbacksHigh]) || (!values[Clock::ReadsLow] && !values[Clock::ReadsHigh]) ||
            !values[Clock::ReadAcknowledged] || values[Clock::ReadSubmitted] != values[Clock::ReadAcknowledged] || !values[Clock::NativeTimerProgressSamples] ||
            !values[Clock::WorkloadElapsedMs] || values[Clock::WorkloadElapsedMs] > Clock::MaximumDurationMs + Clock::MaximumPulseGapMs)
            return false;
        for (const auto field : {Clock::CleanupDeadlineExceeded,
                                 Clock::NativeFailure,
                                 Clock::LastCommandResult,
                                 Clock::AcknowledgementTimeouts,
                                 Clock::RenewalFailures,
                                 Clock::FocusLosses,
                                 Clock::PulseGaps,
                                 Clock::ReadFailures,
                                 Clock::SubmitFailures,
                                 Clock::RefusedNoGeneration,
                                 Clock::SetsLow,
                                 Clock::SetsHigh,
                                 Clock::FreezesLow,
                                 Clock::FreezesHigh,
                                 Clock::UnfreezesLow,
                                 Clock::UnfreezesHigh,
                                 Clock::ResetsLow,
                                 Clock::ResetsHigh,
                                 Clock::RestoreFailuresLow,
                                 Clock::RestoreFailuresHigh,
                                 Clock::FullMtaReady,
                                 Clock::NotEntered,
                                 Clock::AdapterStopped})
            if (values[field])
                return false;
        for (std::uint32_t field = 88; field < Clock::FieldCount; ++field)
            if (values[field])
                return false;
        return true;
    }

    // Owner supplies the genuine completed-controller admission and preparation
    // methods. Workload invokes the existing Read lifecycle; tests inject its
    // native owner and clock rather than add a shipping native-hook seam.
    template <typename Owner, typename Workload>
    Status RunTransaction(Owner& owner, const Request& request, Workload&& workload) noexcept
    {
        static_assert(noexcept(owner.CheckClockAdmission()) && noexcept(owner.PrepareClock()) && noexcept(owner.EnterClock()) &&
                      noexcept(owner.ClockRetired()) && noexcept(workload()));
        if (!ValidRequest(request))
            return Status::InvalidArgument;
        const auto admitted = owner.CheckClockAdmission();
        if (admitted != Status::Ok)
            return admitted;
        const auto prepared = owner.PrepareClock();
        if (prepared != Status::Ok)
            return prepared;
        // Close direct frontend borrowing before native Begin. The operation
        // lock alone does not cover the separate frontend session API.
        owner.EnterClock();
        const Snapshot result = workload();
        if (!RetiredForCompletedOwner(result))
            return Status::Busy;
        owner.ClockRetired();
        if (WorkloadSucceeded(result) && result[DefinitiveClockReference::WorkloadElapsedMs] >= request.durationMs)
            return Status::Ok;
        const auto reason = static_cast<DefinitiveClockReference::StopReason>(result[DefinitiveClockReference::Reason]);
        return reason == DefinitiveClockReference::StopReason::Cancelled || reason == DefinitiveClockReference::StopReason::FocusLost ? Status::Stopped
                                                                                                                                      : Status::Failed;
    }

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    Status RunOwner(void* handle, std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                    DefinitiveInputReference::PublishCallback publish) noexcept;
#endif
}
