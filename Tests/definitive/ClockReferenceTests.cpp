/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise Core's production clock lifecycle without a game process
 *****************************************************************************/
#include "../../Client/core/DefinitiveClockReference.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>

namespace AllocationWatch
{
    std::atomic<bool>     enabled{false};
    std::atomic<unsigned> requests{0};
}

void* operator new(std::size_t size)
{
    if (AllocationWatch::enabled.load(std::memory_order_relaxed))
        AllocationWatch::requests.fetch_add(1, std::memory_order_relaxed);
    if (void* result = std::malloc(size ? size : 1))
        return result;
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
    namespace Ref = DefinitiveClockReference;
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
        unsigned      waits = 0;
        unsigned      gapAtWait = 0;
        unsigned      reverseAtWait = 0;
        unsigned      backoffs = 0;
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

    // This owner intentionally keeps debt visible after End. A fixture that
    // simply cleared state would miss premature Core/module retirement.
    struct FakeOwner
    {
        static constexpr std::uint64_t Generation = 0x200000007ull;
        FakeClock&                     clock;
        ClockServiceStateV1            state;
        ClockServiceRequestV1          receivedRequest;
        std::uint64_t                  beganAt = 0, submittedAt = 0, submittedSequence = 0, lastSubmitted = 0;
        std::uint64_t                  focusLostAt = 0;
        std::uint32_t                  readyDelay = 0, acknowledgementDelay = 50;
        unsigned                       begins = 0, renews = 0, reads = 0, submits = 0, ends = 0, resets = 0;
        unsigned                       failRenewAt = 0, failReadAt = 0, failSubmitAt = 0;
        unsigned                       invalidReadAt = 0, foreignReadAt = 0;
        unsigned                       throwBegin = 0, throwRenewAt = 0, throwReadAt = 0, throwSubmitAt = 0, throwEndAt = 0;
        unsigned                       busyEnds = 0, lyingEnds = 0, failResets = 0, cleanupReadFailures = 0;
        unsigned                       taint = 0;
        bool                           rejectBegin = false, noGeneration = false, beginWrongSize = false;
        bool                           focused = true, noCallbacks = false, neverReady = false, missingEngine = false, missingPin = false;
        bool                           neverAcknowledge = false, futureAcknowledgement = false, emptyReadAcknowledgement = false;
        bool                           failedAcknowledgement = false, fixedTimer = false, canonicalCommands = true, canonicalRequest = true;
        bool                           ending = false;

        explicit FakeOwner(FakeClock& value) : clock(value) {}
        bool Focused() const noexcept { return focused && (!focusLostAt || clock.now < focusLostAt); }
        bool Begin(const ClockServiceRequestV1& request, ClockServiceStateV1& output)
        {
            ++begins;
            receivedRequest = request;
            canonicalRequest = request.size == sizeof(request) && request.version == ClockServiceVersionV1 &&
                               request.maximumDurationMs >= Ref::MaximumDurationMs + Ref::ReadyTimeoutMs && !request.reserved[0] && !request.reserved[1] &&
                               !request.reserved[2];
            beganAt = clock.now;
            state.generation = noGeneration ? 0 : Generation;
            state.producerThread = 31;
            state.bound = state.hooked = state.active = 1;
            state.modulePinned = missingPin ? 0u : 1u;
            state.nativeTimer = 100;
            state.hour = 14;
            state.minute = 35;
            state.seconds = 7;
            state.lastMinuteChange = 80;
            state.millisecondsPerGameMinute = 1000;
            output = state;
            if (beginWrongSize)
                output.size = 0;
            if (throwBegin)
                throw 1;
            return !rejectBegin;
        }
        bool Renew(std::uint64_t generation)
        {
            ++renews;
            canonicalCommands = canonicalCommands && generation == Generation;
            if (throwRenewAt == renews)
                throw 2;
            if (failRenewAt == renews)
                return false;
            ++state.renewals;
            return true;
        }
        bool Read(ClockServiceStateV1& output)
        {
            ++reads;
            if (throwReadAt == reads)
                throw 3;
            if (failReadAt == reads)
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
                state.engineThread = missingEngine || noCallbacks ? 0u : 17u;
                state.ready = !neverReady && clock.now - beganAt >= readyDelay ? 1u : 0u;
                if (!fixedTimer)
                    state.nativeTimer = 100 + static_cast<std::uint32_t>(clock.now - beganAt);
                if (submittedSequence && !neverAcknowledge && clock.now - submittedAt >= acknowledgementDelay)
                {
                    state.lastCompletedSequence = submittedSequence + (futureAcknowledgement ? 1u : 0u);
                    state.lastCommandResult = failedAcknowledgement ? ClockResult::ReadFailure : ClockResult::Ok;
                    state.pendingSequence = 0;
                    if (!emptyReadAcknowledgement)
                        ++state.reads;
                    submittedSequence = 0;
                }
                switch (taint)
                {
                    case 1:
                        state.sets = 1;
                        break;
                    case 2:
                        state.freezes = 1;
                        break;
                    case 3:
                        state.unfreezes = 1;
                        break;
                    case 4:
                        state.resets = 1;
                        break;
                    case 5:
                        state.frozen = 1;
                        break;
                    case 6:
                        state.patchInstalled = 1;
                        break;
                    case 7:
                        state.patchRecoveryPending = 1;
                        break;
                    case 8:
                        state.dataRecoveryPending = 1;
                        break;
                    case 9:
                        state.protectionRestorePending = 1;
                        break;
                    default:
                        break;
                }
            }
            output = state;
            if (invalidReadAt == reads)
                output.reserved[0] = 1;
            if (foreignReadAt == reads)
                ++output.generation;
            return true;
        }
        bool Submit(const ClockCommandV1& command)
        {
            ++submits;
            canonicalCommands = canonicalCommands && command.size == sizeof(command) && command.version == ClockServiceVersionV1 &&
                                command.generation == Generation && command.sequence == lastSubmitted + 1 && command.operation == ClockOperation::Read &&
                                !command.hour && !command.minute && !command.reserved;
            lastSubmitted = command.sequence;
            if (throwSubmitAt == submits)
                throw 4;
            if (failSubmitAt == submits)
                return false;
            submittedSequence = command.sequence;
            submittedAt = clock.now;
            state.pendingSequence = command.sequence;
            return true;
        }
        bool End(std::uint64_t generation)
        {
            ++ends;
            ending = true;
            canonicalCommands = canonicalCommands && generation == Generation;
            if (throwEndAt == ends)
                throw 5;
            if (ends <= busyEnds + lyingEnds)
            {
                state.active = state.ready = 0;
                state.stopping = 1;
                state.retainedPeerThreads = 2;
                state.inFlight = 1;
                return ends > busyEnds;
            }
            const auto old = state;
            state = {};
            state.generation = Generation;
            state.modulePinned = old.modulePinned;
            state.restored = 1;
            state.nativeTimer = old.nativeTimer;
            state.lastCompletedSequence = old.lastCompletedSequence;
            state.lastCommandResult = old.lastCommandResult;
            state.reads = old.reads;
            state.callbacks = old.callbacks;
            state.qualifiedCallbacks = old.qualifiedCallbacks;
            // Counters may be cleared by a provider. Read-only proof must remain
            // sticky after Core observed a write or outstanding recovery debt.
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
    Fixture* currentFixture = nullptr;
    bool     Cancelled();
    void     Published(const std::uint32_t* values, std::uint32_t count);
    struct Fixture
    {
        FakeClock     clock;
        FakeOwner     owner{clock};
        Ref::Request  request{200, &Cancelled, &Published};
        Ref::Snapshot lastPublication{};
        unsigned      publicationCount = 0, pendingPublications = 0;
        unsigned      throwPublishAt = 0;
        std::uint64_t cancelAt = 0;
        bool          cancelInitially = false, publicationValid = true, terminalOwned = false;
        Ref::Snapshot result{};
        void          Run()
        {
            currentFixture = this;
            AllocationWatch::requests.store(0);
            AllocationWatch::enabled.store(true);
            result = Ref::RunLifecycle(owner, clock, request);
            AllocationWatch::enabled.store(false);
            currentFixture = nullptr;
            Check(AllocationWatch::requests.load() == 0, "lifecycle/publication allocated");
            Check(Ref::Retired(result), "lifecycle returned before retirement");
            Check(owner.canonicalCommands, "lifecycle submitted noncanonical Read");
            Check(!terminalOwned, "terminal publication still owned native state");
            Check(publicationValid, "publication protocol invalid");
            Check(result[Ref::FullMtaReady] == 0, "clock reference promoted full MTA");
            if (request.publish)
            {
                Check(publicationCount != 0, "missing terminal publication");
                Check(lastPublication == result, "terminal publication differs");
            }
            if (owner.begins)
                Check(owner.canonicalRequest, "invalid native Begin request");
        }
        void Reason(Ref::StopReason expected) const { Check(result[Ref::Reason] == static_cast<std::uint32_t>(expected), "unexpected stop reason"); }
    };
    bool Cancelled()
    {
        return currentFixture->cancelInitially || (currentFixture->cancelAt && currentFixture->clock.now >= currentFixture->cancelAt);
    }
    void Published(const std::uint32_t* values, std::uint32_t count)
    {
        auto& fixture = *currentFixture;
        if (fixture.publicationCount >= 1024 || count != Ref::FieldCount)
        {
            fixture.publicationValid = false;
            return;
        }
        ++fixture.publicationCount;
        auto& copy = fixture.lastPublication;
        for (std::uint32_t index = 0; index < count; ++index)
            copy[index] = values[index];
        fixture.publicationValid = fixture.publicationValid && copy[Ref::Version] == Ref::ApiVersion && copy[Ref::Sequence] == fixture.publicationCount;
        for (std::uint32_t index = 88; index < Ref::FieldCount; ++index)
            fixture.publicationValid = fixture.publicationValid && !copy[index];
        if (copy[Ref::Phase] == 3)
        {
            ++fixture.pendingPublications;
            fixture.publicationValid = fixture.publicationValid && copy[Ref::CleanupPending] && !copy[Ref::CleanupComplete];
        }
        if (copy[Ref::Phase] == 2 && fixture.owner.begins && !fixture.owner.Clean())
            fixture.terminalOwned = true;
        if (fixture.throwPublishAt == fixture.publicationCount)
            throw 6;
    }

    void TestCopyAndRetirement()
    {
        ClockServiceStateV1 state;
        state.generation = FakeOwner::Generation;
        state.modulePinned = 1;
        state.engineThread = 17;
        state.producerThread = 31;
        state.hour = 12;
        state.minute = 34;
        state.seconds = 56;
        state.nativeTimer = 1234;
        state.lastMinuteChange = 1200;
        state.millisecondsPerGameMinute = 2000;
        state.callbacks = 0x123456789abcdef0ull;
        state.qualifiedCallbacks = 0x1111111122222222ull;
        state.foreignCallbacks = 0x3333333344444444ull;
        state.nestedCallbacks = 0x5555555566666666ull;
        state.reads = 0x7777777788888888ull;
        state.sets = 0x99999999aaaaaaaauLL;
        state.freezes = 0xbbbbbbbbccccccccull;
        state.unfreezes = 0xddddddddeeeeeeeeull;
        state.resets = 0x1234567812345678ull;
        state.renewals = 0xabcdef01abcdef01ull;
        state.restoreAttempts = 0x2222222233333333ull;
        state.restoreFailures = 0x4444444455555555ull;
        state.lastCompletedSequence = 0x6666666677777777ull;
        state.pendingSequence = 0x8888888899999999ull;
        Ref::Snapshot values{};
        Check(Ref::CopyNative(values, state, state.generation), "valid native copy rejected");
        Check(Pair(values, Ref::GenerationLow) == state.generation, "generation truncated");
        const std::array<std::pair<Ref::Field, std::uint64_t>, 15> pairs{{{Ref::CallbacksLow, state.callbacks},
                                                                          {Ref::QualifiedCallbacksLow, state.qualifiedCallbacks},
                                                                          {Ref::ForeignCallbacksLow, state.foreignCallbacks},
                                                                          {Ref::NestedCallbacksLow, state.nestedCallbacks},
                                                                          {Ref::ReadsLow, state.reads},
                                                                          {Ref::SetsLow, state.sets},
                                                                          {Ref::FreezesLow, state.freezes},
                                                                          {Ref::UnfreezesLow, state.unfreezes},
                                                                          {Ref::ResetsLow, state.resets},
                                                                          {Ref::RenewalsLow, state.renewals},
                                                                          {Ref::RestoreAttemptsLow, state.restoreAttempts},
                                                                          {Ref::RestoreFailuresLow, state.restoreFailures},
                                                                          {Ref::LastCompletedSequenceLow, state.lastCompletedSequence},
                                                                          {Ref::PendingSequenceLow, state.pendingSequence},
                                                                          {Ref::GenerationLow, state.generation}}};
        for (const auto& pair : pairs)
            Check(Pair(values, pair.first) == pair.second, "64-bit native counter truncated");
        Check(values[Ref::Hour] == 12 && values[Ref::Minute] == 34 && values[Ref::Seconds] == 56, "clock values misplaced");
        Check(values[Ref::NativeTimer] == 1234 && values[Ref::LastMinuteChange] == 1200 && values[Ref::MillisecondsPerGameMinute] == 2000,
              "native timer metadata misplaced");
        Check(values[Ref::EngineThread] == 17 && values[Ref::ProducerThread] == 31 && values[Ref::BackendPinnedExpected], "owner metadata misplaced");
        for (unsigned index = 0; index < 19; ++index)
        {
            auto bad = state;
            auto generation = state.generation;
            switch (index)
            {
                case 0:
                    bad.size = 0;
                    break;
                case 1:
                    ++bad.version;
                    break;
                case 2:
                    ++bad.generation;
                    break;
                case 3:
                    generation = 0;
                    break;
                case 4:
                    bad.reserved[0] = 1;
                    break;
                case 5:
                    bad.reserved[1] = 1;
                    break;
                case 6:
                    bad.bound = 2;
                    break;
                case 7:
                    bad.hooked = 2;
                    break;
                case 8:
                    bad.active = 2;
                    break;
                case 9:
                    bad.ready = 2;
                    break;
                case 10:
                    bad.stopping = 2;
                    break;
                case 11:
                    bad.restored = 2;
                    break;
                case 12:
                    bad.frozen = 2;
                    break;
                case 13:
                    bad.modulePinned = 2;
                    break;
                case 14:
                    bad.patchInstalled = 2;
                    break;
                case 15:
                    bad.patchRecoveryPending = 2;
                    break;
                case 16:
                    bad.dataRecoveryPending = 2;
                    break;
                case 17:
                    bad.protectionRestorePending = 2;
                    break;
                default:
                    bad.failure = static_cast<ClockResult>(99);
                    break;
            }
            const auto before = values;
            Check(!Ref::CopyNative(values, bad, generation), "malformed native state accepted");
            Check(values == before, "failed native validation changed publication");
        }
        auto bad = state;
        bad.lastCommandResult = static_cast<ClockResult>(99);
        Check(!Ref::CopyNative(values, bad, state.generation), "invalid command result accepted");

        Ref::Snapshot clean{};
        clean[Ref::Version] = Ref::ApiVersion;
        clean[Ref::Phase] = 2;
        clean[Ref::CleanupComplete] = clean[Ref::BeginAttempted] = clean[Ref::EndSucceeded] = clean[Ref::NativeStateValid] = clean[Ref::NativeRestored] = 1;
        Check(Ref::Retired(clean), "clean retirement rejected");
        const std::array<Ref::Field, 13> dirty{{Ref::CleanupPending, Ref::NativeFrozen, Ref::NativeHooked, Ref::NativeBound, Ref::NativeActive,
                                                Ref::NativeReady, Ref::NativeInFlight, Ref::PatchInstalled, Ref::RetainedPeerThreads, Ref::PatchRecoveryPending,
                                                Ref::DataRecoveryPending, Ref::ProtectionRestorePending, Ref::PendingSequenceLow}};
        for (const auto field : dirty)
        {
            auto pending = clean;
            pending[field] = 1;
            Check(!Ref::Retired(pending), "retirement ignored native obligation");
        }
        const std::array<Ref::Field, 4> required{{Ref::CleanupComplete, Ref::EndSucceeded, Ref::NativeStateValid, Ref::NativeRestored}};
        for (const auto field : required)
        {
            auto pending = clean;
            pending[field] = 0;
            Check(!Ref::Retired(pending), "retirement lacked positive proof");
        }
        auto pending = clean;
        pending[Ref::PendingSequenceHigh] = 1;
        Check(!Ref::Retired(pending), "high pending sequence ignored");
        pending = {};
        pending[Ref::Version] = 1;
        pending[Ref::Phase] = 2;
        pending[Ref::CleanupComplete] = pending[Ref::NotEntered] = 1;
        Check(!Ref::Retired(pending), "not-entered retired before adapter stopped");
        pending[Ref::AdapterStopped] = 1;
        Check(Ref::Retired(pending), "safe not-entered retirement rejected");
        pending[Ref::BeginAttempted] = 1;
        Check(!Ref::Retired(pending), "not-entered concealed attempted Begin");
    }

    void TestSuccess()
    {
        Fixture fixture;
        fixture.Run();
        fixture.Reason(Ref::StopReason::Completed);
        Check(fixture.result[Ref::ReadyObserved] && fixture.result[Ref::WorkloadComplete] && fixture.result[Ref::ReadOnlyVerified],
              "successful workload missing proof");
        Check(fixture.result[Ref::ReadAcknowledged] == 4 && fixture.result[Ref::ReadSubmitted] == 4, "Read acknowledgements not matched");
        Check(fixture.result[Ref::NativeTimerProgressSamples] > 0, "native timer did not advance");
        Check(fixture.result[Ref::WorkloadElapsedMs] == 200 && fixture.owner.ends == 1, "workload duration/cleanup incorrect");
        Check(fixture.owner.receivedRequest.maximumDurationMs <= ClockServiceMaximumDurationMsV1, "native duration unsupported");
    }

    void TestPreflight()
    {
        for (unsigned kind = 0; kind < 4; ++kind)
        {
            Fixture fixture;
            if (kind == 0)
                fixture.request.durationMs = 0;
            if (kind == 1)
                fixture.request.durationMs = Ref::MaximumDurationMs + 1;
            if (kind == 2)
                fixture.request.cancelled = nullptr;
            if (kind == 3)
                fixture.request.publish = nullptr;
            fixture.Run();
            fixture.Reason(Ref::StopReason::InvalidRequest);
            Check(!fixture.owner.begins && !fixture.owner.ends && !fixture.owner.resets, "invalid request touched provider");
        }
        {
            Fixture fixture;
            fixture.cancelInitially = true;
            fixture.Run();
            fixture.Reason(Ref::StopReason::Cancelled);
            Check(!fixture.owner.begins, "initial cancellation entered provider");
        }
        {
            Fixture fixture;
            fixture.owner.focused = false;
            fixture.Run();
            fixture.Reason(Ref::StopReason::FocusLost);
            Check(!fixture.owner.begins && fixture.result[Ref::FocusLosses] == 1, "initial focus loss entered provider");
        }
    }

    void TestRuntimeFailures()
    {
        const std::array<Ref::StopReason, 10> reasons{
            {Ref::StopReason::Cancelled, Ref::StopReason::FocusLost, Ref::StopReason::LeaseError, Ref::StopReason::ReadError, Ref::StopReason::SubmitError,
             Ref::StopReason::PulseGap, Ref::StopReason::PulseGap, Ref::StopReason::NativeError, Ref::StopReason::NativeError, Ref::StopReason::NativeError}};
        for (unsigned kind = 0; kind < reasons.size(); ++kind)
        {
            Fixture fixture;
            switch (kind)
            {
                case 0:
                    fixture.cancelAt = 1100;
                    break;
                case 1:
                    fixture.owner.focusLostAt = 1100;
                    break;
                case 2:
                    fixture.owner.failRenewAt = 2;
                    break;
                case 3:
                    fixture.owner.failReadAt = 2;
                    break;
                case 4:
                    fixture.owner.failSubmitAt = 1;
                    break;
                case 5:
                    fixture.clock.gapAtWait = 1;
                    break;
                case 6:
                    fixture.clock.reverseAtWait = 1;
                    break;
                case 7:
                    fixture.owner.invalidReadAt = 1;
                    break;
                case 8:
                    fixture.owner.foreignReadAt = 1;
                    break;
                default:
                    fixture.owner.beginWrongSize = true;
                    break;
            }
            fixture.Run();
            fixture.Reason(reasons[kind]);
            Check(fixture.owner.begins == 1 && fixture.owner.ends >= 1, "runtime failure skipped cleanup");
            Check(!fixture.result[Ref::WorkloadComplete], "failed workload promoted completion");
        }
        for (unsigned kind = 0; kind < 3; ++kind)
        {
            Fixture fixture;
            if (kind == 0)
            {
                fixture.owner.noCallbacks = true;
                fixture.owner.neverReady = true;
            }
            if (kind == 1)
                fixture.owner.missingEngine = true;
            if (kind == 2)
                fixture.owner.missingPin = true;
            fixture.Run();
            fixture.Reason(kind == 0 ? Ref::StopReason::ReadyTimeout : Ref::StopReason::NativeError);
            Check(!fixture.owner.submits && !fixture.result[Ref::ReadyObserved], "unqualified callback admitted Read");
            if (kind == 0)
                Check(fixture.clock.now >= 1000 + Ref::ReadyTimeoutMs && fixture.owner.renews >= 200, "readiness did not heartbeat until deadline");
        }
        {
            Fixture fixture;
            fixture.owner.readyDelay = 500;
            fixture.Run();
            fixture.Reason(Ref::StopReason::Completed);
            Check(fixture.result[Ref::ElapsedMs] == 700 && fixture.result[Ref::WorkloadElapsedMs] == 200, "readiness consumed workload budget");
        }
    }

    void TestAcknowledgements()
    {
        {
            Fixture fixture;
            fixture.request.durationMs = 600;
            fixture.owner.acknowledgementDelay = 200;
            fixture.Run();
            fixture.Reason(Ref::StopReason::Completed);
            Check(fixture.result[Ref::ReadAcknowledged] == 3 && fixture.owner.renews == 13, "slow callback lost heartbeat or acknowledgements");
        }
        for (unsigned kind = 0; kind < 6; ++kind)
        {
            Fixture fixture;
            switch (kind)
            {
                case 0:
                    fixture.owner.neverAcknowledge = true;
                    fixture.request.durationMs = 2000;
                    break;
                case 1:
                    fixture.owner.futureAcknowledgement = true;
                    break;
                case 2:
                    fixture.owner.emptyReadAcknowledgement = true;
                    break;
                case 3:
                    fixture.owner.failedAcknowledgement = true;
                    break;
                case 4:
                    fixture.owner.fixedTimer = true;
                    break;
                default:
                    fixture.owner.acknowledgementDelay = 300;
                    break;
            }
            fixture.Run();
            fixture.Reason(kind == 0 || kind == 5 ? Ref::StopReason::AcknowledgementTimeout : Ref::StopReason::NativeError);
            if (kind == 0 || kind == 5)
                Check(fixture.result[Ref::AcknowledgementTimeouts] == 1, "missing acknowledgement timeout not recorded");
            if (kind != 4)
                Check(!fixture.result[Ref::ReadAcknowledged], "unproven Read acknowledgement accepted");
            Check(!fixture.result[Ref::WorkloadComplete] || kind == 4, "failed acknowledgement completed workload");
        }
        for (unsigned taint = 1; taint <= 9; ++taint)
        {
            Fixture fixture;
            fixture.owner.taint = taint;
            fixture.Run();
            fixture.Reason(Ref::StopReason::UnexpectedWrites);
            Check(!fixture.result[Ref::ReadOnlyVerified], "cleanup erased read-only taint");
        }
    }

    void TestCleanup()
    {
        {
            Fixture fixture;
            fixture.owner.rejectBegin = true;
            fixture.owner.busyEnds = 3;
            fixture.Run();
            fixture.Reason(Ref::StopReason::BeginError);
            Check(!fixture.result[Ref::BeginSucceeded] && fixture.owner.ends == 4 && fixture.pendingPublications == 3, "failed Begin debt was abandoned");
        }
        {
            Fixture fixture;
            fixture.owner.rejectBegin = fixture.owner.noGeneration = true;
            fixture.owner.failResets = 3;
            fixture.Run();
            fixture.Reason(Ref::StopReason::BeginError);
            Check(fixture.owner.resets == 4 && !fixture.owner.ends && fixture.result[Ref::RefusedNoGeneration] && fixture.result[Ref::ResetSucceeded],
                  "generation-free refusal skipped Reset proof");
        }
        {
            Fixture fixture;
            fixture.owner.noGeneration = true;
            fixture.Run();
            fixture.Reason(Ref::StopReason::NativeError);
            Check(fixture.owner.resets == 1, "successful Begin without generation skipped Reset");
        }
        {
            Fixture fixture;
            fixture.owner.busyEnds = 60;
            fixture.Run();
            fixture.Reason(Ref::StopReason::CleanupError);
            Check(fixture.owner.ends == 61 && fixture.pendingPublications == 60, "cleanup deadline terminated native retries");
            Check(fixture.result[Ref::CleanupDeadlineExceeded] && fixture.clock.backoffs > 0, "cleanup deadline failed to select backoff");
            Check(fixture.clock.now > 1000 + Ref::CleanupDeadlineMs, "retirement returned before retained peers released");
        }
        {
            Fixture fixture;
            fixture.owner.lyingEnds = 3;
            fixture.Run();
            fixture.Reason(Ref::StopReason::Completed);
            Check(fixture.owner.ends == 4 && fixture.pendingPublications == 3, "End success replaced clean state proof");
        }
        {
            Fixture fixture;
            fixture.owner.cleanupReadFailures = 3;
            fixture.Run();
            fixture.Reason(Ref::StopReason::Completed);
            Check(fixture.owner.ends == 4 && fixture.result[Ref::ReadFailures] == 3, "failed cleanup Read allowed premature retirement");
        }
        {
            Fixture fixture;
            fixture.owner.foreignReadAt = 6;
            fixture.Run();
            fixture.Reason(Ref::StopReason::Completed);
            Check(fixture.owner.ends == 2 && fixture.result[Ref::ReadFailures] == 1, "foreign cleanup generation allowed retirement");
        }
    }

    void TestExceptions()
    {
        for (unsigned kind = 0; kind < 6; ++kind)
        {
            Fixture fixture;
            switch (kind)
            {
                case 0:
                    fixture.owner.throwBegin = 1;
                    break;
                case 1:
                    fixture.owner.throwRenewAt = 1;
                    break;
                case 2:
                    fixture.owner.throwReadAt = 1;
                    break;
                case 3:
                    fixture.owner.throwSubmitAt = 1;
                    break;
                case 4:
                    fixture.owner.throwEndAt = 1;
                    break;
                default:
                    fixture.throwPublishAt = 1;
                    break;
            }
            fixture.Run();
            fixture.Reason(Ref::StopReason::Exception);
            Check(fixture.owner.ends >= 1 && fixture.owner.Clean(), "exception abandoned native ownership");
            if (kind == 4)
                Check(fixture.owner.ends == 2, "throwing End was not retried");
        }
    }
}

int main()
{
    const char* error = nullptr;
    try
    {
        TestCopyAndRetirement();
        TestSuccess();
        TestPreflight();
        TestRuntimeFailures();
        TestAcknowledgements();
        TestCleanup();
        TestExceptions();
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
