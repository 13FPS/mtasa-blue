/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Deterministic coverage of the production bounded clock Set policy
 ****************************************************************************/
#include "../../Client/core/DefinitiveClockSetReference.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace AllocationWatch
{
    std::atomic<bool>     enabled{false};
    std::atomic<unsigned> requests{0};
}
void* operator new(std::size_t size)
{
    if (AllocationWatch::enabled.load())
        AllocationWatch::requests.fetch_add(1);
    if (void* value = std::malloc(size ? size : 1))
        return value;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void* value) noexcept
{
    std::free(value);
}
void operator delete[](void* value) noexcept
{
    std::free(value);
}
void operator delete(void* value, std::size_t) noexcept
{
    std::free(value);
}
void operator delete[](void* value, std::size_t) noexcept
{
    std::free(value);
}

namespace
{
    namespace Ref = DefinitiveClockSetReference;
    using namespace mta::game_de;
    unsigned checks = 0;
    struct Failure
    {
        const char* message;
    };
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw Failure{message};
    }
    std::uint64_t Pair(const Ref::Snapshot& values, Ref::Field low) noexcept
    {
        return values[low] | (static_cast<std::uint64_t>(values[low + 1]) << 32);
    }

    struct FakeClock
    {
        std::uint64_t now = 1000;
        unsigned      waits = 0, backoffs = 0, gapAtWait = 0, reverseAtWait = 0;
        std::uint64_t Now() const noexcept { return now; }
        void          Wait(std::uint32_t duration) noexcept
        {
            ++waits;
            if (duration == Ref::CleanupBackoffMs)
                ++backoffs;
            if (reverseAtWait == waits)
                now -= 500;
            else
                now += gapAtWait == waits ? Ref::MaximumPulseGapMs + 1 : duration;
        }
    };
    struct FakeOwner
    {
        static constexpr std::uint64_t Generation = 0x200000007ull;
        FakeClock&                     clock;
        ClockServiceStateV1            state;
        ClockCommandV1                 command;
        std::uint64_t                  beganAt = 0, submittedAt = 0, setAt = 0, focusLostAt = 0, lastSubmitted = 0;
        unsigned                       begins = 0, renews = 0, reads = 0, submits = 0, ends = 0, resets = 0, setCommands = 0;
        unsigned                       readyDelay = 0, acknowledgementDelay = 50, rate = 1000, initialHour = 14;
        unsigned                       appliedHour = 0, appliedMinute = 0;
        unsigned                       failRenewAt = 0, failReadAt = 0, failSubmitAt = 0, invalidReadAt = 0, foreignReadAt = 0;
        unsigned                       throwAt = 0, busyEnds = 0, lyingEnds = 0, cleanupReadFailures = 0, failResets = 0, taint = 0;
        bool                           rejectBegin = false, noGeneration = false, beginWrongSize = false, ending = false, focused = true;
        bool                           noCallbacks = false, missingEngine = false, missingPin = false, neverReady = false;
        bool                           neverAcknowledge = false, futureAcknowledgement = false, failedAcknowledgement = false;
        bool                           emptyReadAcknowledgement = false, setWithoutEffect = false, wrongSetMinute = false, fixedTimer = false;
        bool                           staleSetAcknowledgement = false, pendingAfterAcknowledgement = false, clearCountersOnEnd = false;
        bool                           canonical = true, canonicalRequest = true;
        explicit FakeOwner(FakeClock& value) : clock(value) {}
        bool Focused() const noexcept { return focused && (!focusLostAt || clock.now < focusLostAt); }
        bool Begin(const ClockServiceRequestV1& request, ClockServiceStateV1& output)
        {
            ++begins;
            beganAt = clock.now;
            canonicalRequest = request.size == sizeof(request) && request.version == ClockServiceVersionV1 &&
                               request.maximumDurationMs >= Ref::ReadyTimeoutMs + Ref::MaximumDurationMs && !request.reserved[0] && !request.reserved[1] &&
                               !request.reserved[2];
            state.generation = noGeneration ? 0 : Generation;
            state.producerThread = 31;
            state.bound = state.hooked = state.active = 1;
            state.modulePinned = missingPin ? 0u : 1u;
            state.hour = initialHour;
            state.minute = 35;
            state.seconds = 7;
            state.nativeTimer = 100;
            state.lastMinuteChange = 80;
            state.millisecondsPerGameMinute = rate;
            output = state;
            if (beginWrongSize)
                output.size = 0;
            if (throwAt == 1)
                throw 1;
            return !rejectBegin;
        }
        bool Renew(std::uint64_t generation)
        {
            canonical = canonical && generation == Generation;
            ++renews;
            if (throwAt == 2 && renews == 1)
                throw 2;
            if (renews == failRenewAt)
                return false;
            ++state.renewals;
            return true;
        }
        bool Read(ClockServiceStateV1& output)
        {
            ++reads;
            if (throwAt == 3 && reads == 1)
                throw 3;
            if (reads == failReadAt)
                return false;
            if (ending && cleanupReadFailures)
            {
                --cleanupReadFailures;
                return false;
            }
            if (!ending)
            {
                if (!noCallbacks)
                {
                    ++state.callbacks;
                    ++state.qualifiedCallbacks;
                }
                state.engineThread = noCallbacks || missingEngine ? 0u : 17u;
                state.ready = !neverReady && clock.now - beganAt >= readyDelay ? 1u : 0u;
                if (!fixedTimer)
                    state.nativeTimer = 100 + static_cast<std::uint32_t>(clock.now - beganAt);
                if (setAt && !setWithoutEffect)
                {
                    state.hour = appliedHour;
                    state.minute = appliedMinute + static_cast<std::uint32_t>((clock.now - setAt) / 1000);
                }
                if (command.sequence && !neverAcknowledge && clock.now - submittedAt >= acknowledgementDelay)
                {
                    if (!(command.operation == ClockOperation::Set && staleSetAcknowledgement))
                        state.lastCompletedSequence = command.sequence + (futureAcknowledgement ? 1u : 0u);
                    state.lastCommandResult = failedAcknowledgement ? ClockResult::WriteFailure : ClockResult::Ok;
                    state.pendingSequence = pendingAfterAcknowledgement ? command.sequence : 0;
                    if (command.operation == ClockOperation::Read)
                    {
                        if (!emptyReadAcknowledgement)
                            ++state.reads;
                    }
                    else
                    {
                        ++state.sets;
                        setAt = clock.now;
                        appliedHour = command.hour;
                        appliedMinute = command.minute;
                        if (!setWithoutEffect)
                        {
                            state.hour = command.hour;
                            state.minute = wrongSetMinute ? 44u : command.minute;
                            state.lastMinuteChange = state.nativeTimer;
                        }
                    }
                    command.sequence = 0;
                }
                switch (taint)
                {
                    case 1:
                        state.freezes = 1;
                        break;
                    case 2:
                        state.unfreezes = 1;
                        break;
                    case 3:
                        state.resets = 1;
                        break;
                    case 4:
                        state.frozen = 1;
                        break;
                    case 5:
                        state.patchInstalled = 1;
                        break;
                    case 6:
                        state.patchRecoveryPending = 1;
                        break;
                    case 7:
                        state.protectionRestorePending = 1;
                        break;
                    case 8:
                        state.sets = 2;
                        break;
                    default:
                        break;
                }
            }
            output = state;
            if (reads == invalidReadAt)
                output.reserved[0] = 1;
            if (reads == foreignReadAt)
                ++output.generation;
            return true;
        }
        bool Submit(const ClockCommandV1& value)
        {
            ++submits;
            canonical = canonical && value.size == sizeof(value) && value.version == ClockServiceVersionV1 && value.generation == Generation &&
                        value.sequence == lastSubmitted + 1 && !value.reserved &&
                        (value.operation == ClockOperation::Read
                             ? !value.hour && !value.minute
                             : value.operation == ClockOperation::Set && value.hour == (initialHour == 12 ? 6u : 12u) && value.minute == 15);
            lastSubmitted = value.sequence;
            if (value.operation == ClockOperation::Set)
                ++setCommands;
            if (throwAt == 4 && submits == 1)
                throw 4;
            if (submits == failSubmitAt)
                return false;
            command = value;
            submittedAt = clock.now;
            state.pendingSequence = value.sequence;
            return true;
        }
        bool End(std::uint64_t generation)
        {
            canonical = canonical && generation == Generation;
            ++ends;
            ending = true;
            if (throwAt == 5 && ends == 1)
                throw 5;
            if (ends <= busyEnds + lyingEnds)
            {
                state.active = state.ready = 0;
                state.stopping = 1;
                state.retainedPeerThreads = 2;
                state.inFlight = 1;
                return ends > busyEnds;
            }
            // Provider restoration certifies its own capture. Deliberately
            // return different values from Core's initial Read to catch a
            // false claim that these two observations were the same snapshot.
            state.bound = state.hooked = state.active = state.ready = state.stopping = state.frozen = state.inFlight = 0;
            state.patchInstalled = state.retainedPeerThreads = state.patchRecoveryPending = state.dataRecoveryPending = state.protectionRestorePending = 0;
            state.pendingSequence = 0;
            state.restored = 1;
            state.hour = 10;
            state.minute = 2;
            state.seconds = 3;
            if (clearCountersOnEnd)
                state.sets = state.freezes = state.unfreezes = state.resets = 0;
            return true;
        }
        bool Reset()
        {
            ++resets;
            if (resets <= failResets)
                return false;
            state = {};
            return true;
        }
        bool Clean() const noexcept
        {
            return !state.bound && !state.hooked && !state.active && !state.ready && !state.inFlight && !state.retainedPeerThreads && !state.pendingSequence &&
                   !state.patchInstalled && !state.patchRecoveryPending && !state.dataRecoveryPending && !state.protectionRestorePending && !state.frozen;
        }
    };
    struct Fixture;
    Fixture* current = nullptr;
    bool     Cancelled();
    void     Published(const std::uint32_t*, std::uint32_t);
    struct Fixture
    {
        FakeClock     clock;
        FakeOwner     owner{clock};
        Ref::Request  request{250, &Cancelled, &Published};
        Ref::Snapshot result{}, last{};
        unsigned      publications = 0, pendingPublications = 0, throwPublishAt = 0;
        std::uint64_t cancelAt = 0;
        bool          cancelInitially = false, valid = true, terminalOwned = false;
        void          Run()
        {
            current = this;
            AllocationWatch::requests.store(0);
            AllocationWatch::enabled.store(true);
            result = Ref::RunLifecycle(owner, clock, request);
            AllocationWatch::enabled.store(false);
            current = nullptr;
            Check(!AllocationWatch::requests.load(), "lifecycle allocated");
            Check(Ref::Retired(result), "returned before retirement");
            Check(owner.canonical && (!owner.begins || owner.canonicalRequest), "noncanonical SDK command/request");
            Check(valid && !terminalOwned, "publication released retained native owner");
            Check(!result[Ref::ReadOnlyVerified] && !result[Ref::FullMtaReady], "Set trial promoted read-only/full MTA");
            if (request.publish)
                Check(publications && last == result, "terminal publication missing or unequal");
            Check(owner.setCommands <= 1, "submitted multiple Sets");
        }
        void Reason(Ref::StopReason value) const { Check(result[Ref::Reason] == static_cast<std::uint32_t>(value), "unexpected stop reason"); }
    };
    bool Cancelled()
    {
        return current->cancelInitially || (current->cancelAt && current->clock.now >= current->cancelAt);
    }
    void Published(const std::uint32_t* values, std::uint32_t count)
    {
        auto& f = *current;
        if (count != Ref::FieldCount || f.publications > 2048)
        {
            f.valid = false;
            return;
        }
        ++f.publications;
        for (std::uint32_t i = 0; i < count; ++i)
            f.last[i] = values[i];
        f.valid = f.valid && f.last[Ref::Version] == Ref::ApiVersion && f.last[Ref::Sequence] == f.publications;
        for (std::uint32_t i = 122; i < count; ++i)
            f.valid = f.valid && !values[i];
        if (values[Ref::Phase] == 3)
        {
            ++f.pendingPublications;
            f.valid = f.valid && values[Ref::CleanupPending] && !values[Ref::CleanupComplete] && !Ref::Retired(f.last) && !Ref::Completed(f.last);
        }
        if (values[Ref::Phase] == 2 && f.owner.begins && !f.owner.Clean())
            f.terminalOwned = true;
        if (f.throwPublishAt == f.publications)
            throw 6;
    }

    void TestSuccess()
    {
        for (unsigned hour : {0u, 12u, 14u, 23u})
        {
            Fixture f;
            f.owner.initialHour = hour;
            f.Run();
            f.Reason(Ref::StopReason::Completed);
            Check(Ref::Completed(f.result), "successful Set did not satisfy production predicate");
            Check(f.owner.setCommands == 1 && f.result[Ref::SetAcknowledged] == 1 && f.result[Ref::CoherentSetReadback], "Set not proven once");
            Check(f.result[Ref::InitialHour] == hour && f.result[Ref::TargetHour] == (hour == 12 ? 6u : 12u), "non-distinct target hour");
            Check(f.result[Ref::Hour] == 10 && f.result[Ref::ProviderRestorationVerified] && f.result[Ref::ProviderCapturedSnapshotNotCompared],
                  "provider capture falsely equated with producer Read");
            Check(Pair(f.result, Ref::SetSequenceLow) == 2 && Pair(f.result, Ref::InitialReadSequenceLow) == 1, "command sequence evidence missing");
            for (std::uint32_t index :
                 {Ref::WorkloadComplete, Ref::InitialReadAcknowledged, Ref::CoherentSetReadback, Ref::SubsequentReadbackVerified,
                  Ref::ProviderRestorationVerified, Ref::ExactlyOneSetVerified, Ref::TargetProgressSamples, Ref::ProviderCapturedSnapshotNotCompared})
            {
                auto changed = f.result;
                changed[index] = 0;
                Check(!Ref::Completed(changed), "success predicate accepted missing proof");
            }
            for (std::uint32_t index :
                 {Ref::ReadOnlyVerified, Ref::FullMtaReady, Ref::ForbiddenOperationObserved, Ref::SetReadbackMismatch, Ref::RateRejectedBeforeWrite,
                  Ref::SetsHigh, Ref::FreezesLow, Ref::UnfreezesLow, Ref::ResetsLow, Ref::CleanupPending, Ref::RetainedPeerThreads, Ref::PatchInstalled,
                  Ref::DataRecoveryPending, Ref::ProtectionRestorePending, Ref::PendingSequenceLow, Ref::NativeFailure, Ref::LastCommandResult,
                  static_cast<Ref::Field>(122), static_cast<Ref::Field>(127)})
            {
                auto changed = f.result;
                changed[index] = 1;
                Check(!Ref::Completed(changed), "success predicate accepted taint");
            }
        }
        Fixture f;
        f.request.durationMs = Ref::MaximumDurationMs;
        f.Run();
        f.Reason(Ref::StopReason::Completed);
        Check(Ref::Completed(f.result) && f.result[Ref::SubsequentReadAcknowledged] > 50, "full-duration recipe lost progress");
    }
    void TestPreflight()
    {
        for (unsigned kind = 0; kind < 6; ++kind)
        {
            Fixture f;
            if (kind == 0)
                f.request.durationMs = 0;
            if (kind == 1)
                f.request.durationMs = Ref::MaximumDurationMs + 1;
            if (kind == 2)
                f.request.cancelled = nullptr;
            if (kind == 3)
                f.request.publish = nullptr;
            if (kind == 4)
                f.cancelInitially = true;
            if (kind == 5)
                f.owner.focused = false;
            f.Run();
            f.Reason(kind < 4 ? Ref::StopReason::InvalidRequest : kind == 4 ? Ref::StopReason::Cancelled : Ref::StopReason::FocusLost);
            Check(!f.owner.begins && !f.owner.submits && !Ref::Completed(f.result), "preflight entered native service");
        }
        for (unsigned rate : {0u, 999u, 1001u, 2000u})
        {
            Fixture f;
            f.owner.rate = rate;
            f.Run();
            f.Reason(Ref::StopReason::UnsupportedRate);
            Check(f.result[Ref::RateRejectedBeforeWrite] && f.result[Ref::InitialReadAcknowledged] && !f.owner.setCommands && !Ref::Completed(f.result),
                  "unsupported rate wrote time");
        }
    }
    void TestFailures()
    {
        for (unsigned kind = 0; kind < 17; ++kind)
        {
            Fixture         f;
            Ref::StopReason expected = Ref::StopReason::NativeError;
            switch (kind)
            {
                case 0:
                    f.owner.rejectBegin = true;
                    expected = Ref::StopReason::BeginError;
                    break;
                case 1:
                    f.owner.beginWrongSize = true;
                    break;
                case 2:
                    f.owner.noGeneration = true;
                    break;
                case 3:
                    f.owner.neverReady = true;
                    expected = Ref::StopReason::ReadyTimeout;
                    break;
                case 4:
                    f.owner.missingEngine = true;
                    break;
                case 5:
                    f.owner.missingPin = true;
                    break;
                case 6:
                    f.owner.noCallbacks = true;
                    break;
                case 7:
                    f.owner.failRenewAt = 1;
                    expected = Ref::StopReason::LeaseError;
                    break;
                case 8:
                    f.owner.failReadAt = 1;
                    expected = Ref::StopReason::ReadError;
                    break;
                case 9:
                    f.owner.invalidReadAt = 2;
                    break;
                case 10:
                    f.owner.foreignReadAt = 2;
                    break;
                case 11:
                    f.owner.failSubmitAt = 1;
                    expected = Ref::StopReason::SubmitError;
                    break;
                case 12:
                    f.owner.failSubmitAt = 2;
                    expected = Ref::StopReason::SubmitError;
                    break;
                case 13:
                    f.cancelAt = 1050;
                    expected = Ref::StopReason::Cancelled;
                    break;
                case 14:
                    f.cancelAt = 1150;
                    expected = Ref::StopReason::Cancelled;
                    break;
                case 15:
                    f.owner.focusLostAt = 1150;
                    expected = Ref::StopReason::FocusLost;
                    break;
                case 16:
                    f.clock.gapAtWait = 2;
                    expected = Ref::StopReason::PulseGap;
                    break;
            }
            f.Run();
            f.Reason(expected);
            Check(!Ref::Completed(f.result), "failed recipe completed");
            if (kind == 13)
                Check(!f.owner.setCommands, "cancel before Set still wrote");
            if (kind == 14)
                Check(f.owner.setCommands == 1 && f.result[Ref::ProviderRestorationVerified], "cancel after Set skipped provider restoration");
        }
        for (unsigned kind = 0; kind < 10; ++kind)
        {
            Fixture         f;
            Ref::StopReason expected = Ref::StopReason::NativeError;
            switch (kind)
            {
                case 0:
                    f.owner.neverAcknowledge = true;
                    f.request.durationMs = 2000;
                    expected = Ref::StopReason::AcknowledgementTimeout;
                    break;
                case 1:
                    f.owner.futureAcknowledgement = true;
                    break;
                case 2:
                    f.owner.emptyReadAcknowledgement = true;
                    break;
                case 3:
                    f.owner.failedAcknowledgement = true;
                    break;
                case 4:
                    f.owner.setWithoutEffect = true;
                    expected = Ref::StopReason::IncorrectReadback;
                    break;
                case 5:
                    f.owner.wrongSetMinute = true;
                    expected = Ref::StopReason::IncorrectReadback;
                    break;
                case 6:
                    f.owner.staleSetAcknowledgement = true;
                    f.request.durationMs = 2000;
                    expected = Ref::StopReason::AcknowledgementTimeout;
                    break;
                case 7:
                    f.owner.fixedTimer = true;
                    break;
                case 8:
                    f.owner.pendingAfterAcknowledgement = true;
                    break;
                case 9:
                    f.owner.acknowledgementDelay = 1100;
                    f.request.durationMs = 2000;
                    expected = Ref::StopReason::AcknowledgementTimeout;
                    break;
            }
            f.Run();
            f.Reason(expected);
            Check(!Ref::Completed(f.result), "stale/incoherent acknowledgement completed");
        }
        for (unsigned taint = 1; taint <= 8; ++taint)
        {
            Fixture f;
            f.owner.taint = taint;
            f.owner.clearCountersOnEnd = true;
            f.Run();
            f.Reason(Ref::StopReason::UnexpectedWrites);
            Check(f.result[Ref::ForbiddenOperationObserved] && !Ref::Completed(f.result), "provider cleanup erased forbidden-operation evidence");
        }
    }
    void TestCleanup()
    {
        for (unsigned kind = 0; kind < 5; ++kind)
        {
            Fixture f;
            if (kind == 0)
                f.owner.busyEnds = 60;
            if (kind == 1)
                f.owner.lyingEnds = 3;
            if (kind == 2)
                f.owner.cleanupReadFailures = 3;
            if (kind == 3)
            {
                f.owner.rejectBegin = true;
                f.owner.noGeneration = true;
                f.owner.failResets = 3;
            }
            if (kind == 4)
            {
                f.owner.rejectBegin = true;
                f.owner.busyEnds = 3;
            }
            f.Run();
            if (kind == 0)
            {
                f.Reason(Ref::StopReason::CleanupError);
                Check(f.owner.ends == 61 && f.pendingPublications == 60 && f.clock.backoffs && f.result[Ref::CleanupDeadlineExceeded],
                      "cleanup deadline abandoned retained owner");
            }
            if (kind == 1)
                Check(f.owner.ends == 4 && f.pendingPublications == 3, "lying End replaced retirement proof");
            if (kind == 2)
                Check(f.owner.ends == 4 && f.result[Ref::ReadFailures] == 3, "failed cleanup Read lost ownership");
            if (kind == 3)
                Check(f.owner.resets == 4 && !f.owner.ends && f.result[Ref::ResetSucceeded], "zero generation skipped Reset");
            if (kind == 4)
                Check(f.owner.ends == 4 && f.pendingPublications == 3, "failed Begin released debt");
        }
        for (unsigned kind = 1; kind <= 6; ++kind)
        {
            Fixture f;
            if (kind < 6)
                f.owner.throwAt = kind;
            else
                f.throwPublishAt = 1;
            f.Run();
            f.Reason(Ref::StopReason::Exception);
            Check(f.owner.Clean() && !Ref::Completed(f.result), "exception abandoned owner or completed recipe");
        }
    }
}
int main()
{
    const char* error = nullptr;
    try
    {
        TestSuccess();
        TestPreflight();
        TestFailures();
        TestCleanup();
    }
    catch (const Failure& failure)
    {
        error = failure.message;
    }
    catch (...)
    {
        error = "unexpected fixture exception";
    }
    AllocationWatch::enabled.store(false);
    std::printf(
        "{\"success\":%s,\"checks\":%u,\"policy\":\"production_RunLifecycle\",\"fakeOwnerAndClock\":true,"
        "\"gameLaunched\":false,\"nativeClockWritten\":false,\"fullMtaReady\":false,\"error\":",
        error ? "false" : "true", checks);
    if (error)
        std::printf("\"%s\"", error);
    else
        std::printf("null");
    std::puts("}");
    return error ? 1 : 0;
}
