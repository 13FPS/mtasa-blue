/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise completed-owner clock admission with the real Read lifecycle
 *****************************************************************************/
#include "../../Client/core/DefinitiveFrontendClockReference.h"
#include "../../Client/core/DefinitiveFrontendStartup.h"
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
    namespace Ref = DefinitiveFrontendClockReference;
    namespace Clock = DefinitiveClockReference;
    using namespace mta::game_de;
    using Status = Ref::Status;
    static_assert(Ref::ApiVersion == 1 && Clock::ApiVersion == 1 && Clock::FieldCount == 96);
    static_assert(DefinitiveFrontendStartup::ApiVersion == 1 && DefinitiveFrontendStartup::FieldCount == 32);
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
    struct FakeClock
    {
        std::uint64_t now = 1000;
        unsigned      waits = 0, backoffs = 0, gapAtWait = 0;
        std::uint64_t Now() const noexcept { return now; }
        void          Wait(std::uint32_t milliseconds) noexcept
        {
            ++waits;
            if (milliseconds == Clock::CleanupBackoffMs)
                ++backoffs;
            now += gapAtWait == waits ? Clock::MaximumPulseGapMs + 1 : milliseconds;
        }
    };
    struct NativeOwner
    {
        static constexpr std::uint64_t Generation = UINT64_C(0x300000005);
        FakeClock&                     clock;
        ClockServiceStateV1            state;
        std::uint64_t                  started = 0, pending = 0, lastSubmitted = 0;
        unsigned                       begins = 0, reads = 0, submits = 0, ends = 0, resets = 0, busyEnds = 0, failReadAt = 0;
        bool                           focused = true, noGeneration = false, rejectBegin = false, fixedTimer = false, canonical = true;
        explicit NativeOwner(FakeClock& value) : clock(value) {}
        bool Focused() const noexcept { return focused; }
        bool Begin(const ClockServiceRequestV1& request, ClockServiceStateV1& output)
        {
            ++begins;
            started = clock.now;
            canonical = canonical && request.size == sizeof(request) && request.version == ClockServiceVersionV1;
            state.generation = noGeneration ? 0 : Generation;
            state.producerThread = 31;
            state.bound = state.hooked = state.active = state.modulePinned = 1;
            state.hour = 10;
            state.minute = 20;
            state.seconds = 30;
            state.nativeTimer = 100;
            state.millisecondsPerGameMinute = 1000;
            output = state;
            return !rejectBegin;
        }
        bool Renew(std::uint64_t generation)
        {
            canonical = canonical && generation == Generation;
            ++state.renewals;
            return true;
        }
        bool Read(ClockServiceStateV1& output)
        {
            ++reads;
            if (reads == failReadAt)
                return false;
            if (!ends)
            {
                ++state.callbacks;
                ++state.qualifiedCallbacks;
                state.ready = 1;
                state.engineThread = 17;
                if (!fixedTimer)
                    state.nativeTimer = 100 + static_cast<std::uint32_t>(clock.now - started);
                if (pending)
                {
                    state.lastCompletedSequence = pending;
                    state.lastCommandResult = ClockResult::Ok;
                    state.pendingSequence = 0;
                    pending = 0;
                    ++state.reads;
                }
            }
            output = state;
            return true;
        }
        bool Submit(const ClockCommandV1& command)
        {
            ++submits;
            canonical = canonical && command.size == sizeof(command) && command.version == ClockServiceVersionV1 && command.operation == ClockOperation::Read &&
                        command.generation == Generation && command.sequence == lastSubmitted + 1 && !command.hour && !command.minute && !command.reserved;
            lastSubmitted = pending = command.sequence;
            state.pendingSequence = command.sequence;
            return true;
        }
        bool End(std::uint64_t generation)
        {
            ++ends;
            canonical = canonical && generation == Generation;
            if (ends <= busyEnds)
            {
                state.active = state.ready = 0;
                state.stopping = 1;
                state.inFlight = state.retainedPeerThreads = 1;
                return false;
            }
            state.bound = state.hooked = state.active = state.ready = state.stopping = state.inFlight = state.retainedPeerThreads = 0;
            state.pendingSequence = 0;
            state.restored = 1;
            return true;
        }
        bool Reset()
        {
            ++resets;
            state = {};
            return true;
        }
    };
    struct Owner
    {
        bool     running = true, backendStarted = true, borrowed = false, viewClean = true, wrongThread = false;
        bool     clockPending = false, lastClockRetired = true, accepting = true, failPrepare = false;
        unsigned preparations = 0, entries = 0, retirements = 0, viewRuns = 0, frontendAllocations = 0;
        Status   CheckClockAdmission() const noexcept
        {
            if (wrongThread)
                return Status::WrongThread;
            if (!running || !backendStarted)
                return Status::Stopped;
            return borrowed || !viewClean || clockPending || !lastClockRetired ? Status::Busy : Status::Ok;
        }
        Status PrepareClock() noexcept
        {
            ++preparations;
            accepting = false;
            if (failPrepare)
            {
                accepting = running && lastClockRetired;
                return Status::Failed;
            }
            return Status::Ok;
        }
        void EnterClock() noexcept
        {
            ++entries;
            clockPending = true;
            lastClockRetired = false;
            accepting = false;
        }
        void ClockRetired() noexcept
        {
            ++retirements;
            clockPending = false;
            lastClockRetired = true;
            accepting = running;
        }
        Status OtherOperation() const noexcept { return clockPending ? Status::Busy : Status::Ok; }
        Status RunView() noexcept
        {
            if (clockPending || !lastClockRetired || !accepting || borrowed)
                return Status::Busy;
            ++viewRuns;
            return backendStarted ? Status::Ok : Status::Failed;
        }
        bool CreateFrontend() noexcept
        {
            // Model the controller's required early admission: no allocation
            // may precede refusal while native peer cleanup can own a lock.
            if (!accepting || clockPending || borrowed)
                return false;
            ++frontendAllocations;
            return true;
        }
    };
    struct Fixture;
    Fixture* current = nullptr;
    bool     Cancelled();
    void     Published(const std::uint32_t*, std::uint32_t);
    struct Fixture
    {
        FakeClock     clock;
        NativeOwner   native{clock};
        Owner         owner;
        Ref::Request  request{200, &Cancelled, &Published};
        Ref::Snapshot snapshot{};
        Status        status = Status::Failed;
        unsigned      workCalls = 0, publications = 0, pendingPublications = 0;
        std::uint64_t cancelAt = 0;
        bool          cancelInitially = false, throwCancellation = false, reentry = false, callbackAdmissionValid = true, debtPastDeadline = false;
        void          Reenter() noexcept
        {
            auto never = [&]() noexcept -> Ref::Snapshot
            {
                ++workCalls;
                return {};
            };
            callbackAdmissionValid = callbackAdmissionValid && Ref::RunTransaction(owner, request, never) == Status::Busy &&
                                     owner.OtherOperation() == Status::Busy && owner.RunView() == Status::Busy && !owner.CreateFrontend() &&
                                     owner.clockPending && !owner.lastClockRetired && !owner.accepting && owner.backendStarted &&
                                     owner.frontendAllocations == 0;
        }
        void Run()
        {
            current = this;
            AllocationWatch::requests.store(0);
            AllocationWatch::enabled.store(true);
            status = Ref::RunTransaction(owner, request,
                                         [&]() noexcept
                                         {
                                             ++workCalls;
                                             snapshot = Clock::RunLifecycle(native, clock, request);
                                             return snapshot;
                                         });
            AllocationWatch::enabled.store(false);
            current = nullptr;
            Check(!AllocationWatch::requests.load(), "clock transaction allocated");
            Check(native.canonical, "noncanonical existing Read command");
            Check(callbackAdmissionValid, "callback reentry opened a borrowed frontend/native owner");
            Check(owner.backendStarted && owner.running, "clock transaction retired completed backend");
            if (workCalls)
            {
                Check(workCalls == 1 && Ref::RetiredForCompletedOwner(snapshot), "production Read returned without qualified retirement");
                Check(!owner.clockPending && owner.lastClockRetired && owner.accepting && owner.retirements == 1,
                      "retired clock did not reopen completed owner");
                Check(owner.RunView() == Status::Ok && owner.viewRuns == 1, "same completed owner cannot run its later view");
            }
        }
    };
    bool Cancelled()
    {
        if (current->reentry)
            current->Reenter();
        if (current->throwCancellation)
            throw 1;
        return current->cancelInitially || (current->cancelAt && current->clock.now >= current->cancelAt);
    }
    void Published(const std::uint32_t* values, std::uint32_t count)
    {
        auto& f = *current;
        ++f.publications;
        f.callbackAdmissionValid = f.callbackAdmissionValid && count == Clock::FieldCount && f.owner.clockPending && !f.owner.accepting &&
                                   f.owner.backendStarted && !f.owner.retirements;
        if (values[Clock::Phase] == 3)
        {
            ++f.pendingPublications;
            f.callbackAdmissionValid = f.callbackAdmissionValid && values[Clock::CleanupPending] && !values[Clock::CleanupComplete];
            if (values[Clock::CleanupDeadlineExceeded])
                f.debtPastDeadline = true;
        }
        if (f.reentry)
            f.Reenter();
    }

    void TestSuccessfulReuse()
    {
        Fixture f;
        f.reentry = true;
        f.Run();
        Check(f.status == Status::Ok && Ref::WorkloadSucceeded(f.snapshot), "qualified Read did not complete transaction");
        Check(f.native.begins == 1 && f.native.submits && f.native.ends == 1 && !f.native.resets, "did not reuse existing native Read lifecycle");
        Check(f.owner.preparations == 1 && f.owner.entries == 1 && f.publications, "completed owner preparation/entry/publication missing");
        for (const auto field : {Clock::BeginAttempted, Clock::BeginSucceeded, Clock::ReadyObserved, Clock::WorkloadComplete, Clock::ReadOnlyVerified,
                                 Clock::BackendPinnedExpected, Clock::NativeStateValid, Clock::ReadAcknowledged, Clock::NativeTimerProgressSamples,
                                 Clock::EngineThread, Clock::ProducerThread, Clock::GenerationLow, Clock::QualifiedCallbacksLow, Clock::ReadsLow})
        {
            auto changed = f.snapshot;
            changed[field] = 0;
            if (field == Clock::GenerationLow)
                changed[Clock::GenerationHigh] = 0;
            Check(!Ref::WorkloadSucceeded(changed), "missing Read proof satisfied completed-owner success");
        }
        for (const auto field :
             {Clock::CleanupDeadlineExceeded, Clock::NativeFailure, Clock::LastCommandResult, Clock::AcknowledgementTimeouts, Clock::RenewalFailures,
              Clock::FocusLosses, Clock::PulseGaps, Clock::ReadFailures, Clock::SubmitFailures, Clock::SetsLow, Clock::SetsHigh, Clock::FreezesLow,
              Clock::UnfreezesLow, Clock::ResetsLow, Clock::RestoreFailuresLow, Clock::FullMtaReady, Clock::NotEntered, Clock::AdapterStopped})
        {
            auto changed = f.snapshot;
            changed[field] = 1;
            Check(!Ref::WorkloadSucceeded(changed), "tainted Read proof satisfied completed-owner success");
        }
        for (unsigned field = 88; field < Clock::FieldCount; ++field)
        {
            auto changed = f.snapshot;
            changed[field] = 1;
            Check(!Ref::WorkloadSucceeded(changed), "reserved Read field accepted");
        }
        Owner shortWork;
        auto  longerRequest = f.request;
        longerRequest.durationMs = f.snapshot[Clock::WorkloadElapsedMs] + 1;
        Check(Ref::RunTransaction(shortWork, longerRequest, [&]() noexcept { return f.snapshot; }) == Status::Failed && !shortWork.clockPending &&
                  shortWork.lastClockRetired && shortWork.accepting,
              "shorter successful snapshot satisfied the requested workload budget");
    }
    void TestAdmission()
    {
        for (unsigned kind = 0; kind < 10; ++kind)
        {
            Fixture f;
            if (kind == 0)
                f.request.durationMs = 0;
            if (kind == 1)
                f.request.durationMs = Clock::MaximumDurationMs + 1;
            if (kind == 2)
                f.request.cancelled = nullptr;
            if (kind == 3)
                f.request.publish = nullptr;
            if (kind == 4)
                f.owner.borrowed = true;
            if (kind == 5)
                f.owner.viewClean = false;
            if (kind == 6)
                f.owner.clockPending = true;
            if (kind == 7)
                f.owner.lastClockRetired = false;
            if (kind == 8)
                f.owner.wrongThread = true;
            if (kind == 9)
                f.owner.failPrepare = true;
            f.Run();
            Check(!f.workCalls && !f.native.begins && !f.owner.entries && !f.publications, "refused admission entered workload/callbacks");
            Check(f.status == (kind < 4    ? Status::InvalidArgument
                               : kind < 8  ? Status::Busy
                               : kind == 8 ? Status::WrongThread
                                           : Status::Failed),
                  "wrong admission refusal status");
            if (kind == 9)
                Check(f.owner.accepting && f.owner.lastClockRetired, "safe preparation failure closed the completed owner");
        }
        Owner stopped;
        stopped.running = false;
        unsigned     calls = 0;
        Ref::Request request{200, &Cancelled, &Published};
        Check(Ref::RunTransaction(stopped, request,
                                  [&]() noexcept -> Ref::Snapshot
                                  {
                                      ++calls;
                                      return {};
                                  }) == Status::Stopped &&
                  !calls,
              "stopped owner admitted native workload");
    }
    void TestPreBeginRetirement()
    {
        for (unsigned kind = 0; kind < 3; ++kind)
        {
            Fixture f;
            if (kind == 0)
                f.cancelInitially = true;
            if (kind == 1)
                f.native.focused = false;
            if (kind == 2)
                f.throwCancellation = true;
            f.reentry = true;
            f.Run();
            Check(!f.native.begins && !f.snapshot[Clock::AdapterStopped] && !f.snapshot[Clock::BeginAttempted] &&
                      f.status == (kind == 2 ? Status::Failed : Status::Stopped),
                  "safe pre-Begin refusal stopped completed backend");
            auto refused = f.snapshot;
            refused[Clock::NotEntered] = 1;
            Check(!Clock::Retired(refused) && Ref::RetiredForCompletedOwner(refused), "completed-owner pre-Begin proof reused AdapterStopped shortcut");
            for (unsigned bad = 0; bad < 5; ++bad)
            {
                auto changed = refused;
                if (bad == 0)
                    changed[Clock::Version] = 2;
                if (bad == 1)
                    changed[Clock::Phase] = 3;
                if (bad == 2)
                    changed[Clock::Sequence] = 0;
                if (bad == 3)
                    changed[Clock::CleanupComplete] = 0;
                if (bad == 4)
                    changed[Clock::FocusLosses] = kind == 1 ? 0u : 1u;
                Check(!Ref::RetiredForCompletedOwner(changed), "pre-Begin retirement accepted malformed terminal metadata");
            }
            for (unsigned field = 0; field < Clock::FieldCount; ++field)
            {
                if (field == Clock::Version || field == Clock::Phase || field == Clock::Sequence || field == Clock::ElapsedMs || field == Clock::Reason ||
                    field == Clock::CleanupComplete || field == Clock::FocusLosses || field == Clock::NotEntered)
                    continue;
                auto changed = refused;
                changed[field] = 1;
                Check(!Ref::RetiredForCompletedOwner(changed), "pre-Begin proof accepted native state/debt or promoted workload");
            }
            refused[Clock::Reason] = static_cast<std::uint32_t>(Clock::StopReason::Completed);
            Check(!Ref::RetiredForCompletedOwner(refused), "pre-Begin refusal claimed successful workload");
        }
    }
    void TestRetainedAndFailedWork()
    {
        {
            Fixture f;
            f.native.busyEnds = 60;
            f.reentry = true;
            f.Run();
            Check(f.status == Status::Failed && f.native.ends == 61 && f.pendingPublications == 60 && f.debtPastDeadline && f.clock.backoffs,
                  "cleanup deadline released completed owner before retained native debt retired");
            Check(f.snapshot[Clock::CleanupDeadlineExceeded] && !Ref::WorkloadSucceeded(f.snapshot), "deadline cleanup promoted completed workload");
        }
        for (unsigned kind = 0; kind < 5; ++kind)
        {
            Fixture f;
            if (kind == 0)
                f.cancelAt = 1100;
            if (kind == 1)
                f.native.failReadAt = 1;
            if (kind == 2)
                f.native.fixedTimer = true;
            if (kind == 3)
                f.clock.gapAtWait = 1;
            if (kind == 4)
                f.native.rejectBegin = f.native.noGeneration = true;
            f.Run();
            Check(f.status == (kind == 0 ? Status::Stopped : Status::Failed) && !Ref::WorkloadSucceeded(f.snapshot),
                  "failed/cancelled workload promoted success");
            if (kind == 4)
                Check(f.native.resets == 1, "zero-generation refusal skipped existing reset retirement");
        }
        Owner         retained;
        Ref::Request  request{200, &Cancelled, &Published};
        Ref::Snapshot debt{};
        debt[Clock::Version] = 1;
        debt[Clock::Phase] = 3;
        debt[Clock::BeginAttempted] = 1;
        debt[Clock::CleanupPending] = 1;
        debt[Clock::RetainedPeerThreads] = 1;
        const auto status = Ref::RunTransaction(retained, request, [&]() noexcept { return debt; });
        Check(status == Status::Busy && retained.clockPending && !retained.lastClockRetired && !retained.accepting && !retained.retirements &&
                  retained.backendStarted && retained.RunView() == Status::Busy && retained.OtherOperation() == Status::Busy && !retained.CreateFrontend(),
              "nonretired return reopened controller, adapter or frontend admission");
    }
}
int main()
{
    const char* error = nullptr;
    try
    {
        TestSuccessfulReuse();
        TestAdmission();
        TestPreBeginRetirement();
        TestRetainedAndFailedWork();
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
        "{\"success\":%s,\"checks\":%u,\"policy\":\"production_RunTransaction_and_Read_RunLifecycle\",\"fakeOwnerAndClock\":true,"
        "\"gameLaunched\":false,\"nativeClockWritten\":false,\"fullMtaReady\":false,\"error\":",
        error ? "false" : "true", checks);
    if (error)
        std::printf("\"%s\"", error);
    else
        std::printf("null");
    std::puts("}");
    return error ? 1 : 0;
}
