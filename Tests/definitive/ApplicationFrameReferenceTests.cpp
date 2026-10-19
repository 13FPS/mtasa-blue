/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual value-only application observer lifecycle without a game
 ****************************************************************************/
#include "../../Client/core/DefinitiveApplicationFrameReference.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace AllocationWatch
{
    std::atomic<bool>     enabled{false};
    std::atomic<unsigned> requests{0};
}
void* operator new(std::size_t size)
{
    if (AllocationWatch::enabled.load())
        ++AllocationWatch::requests;
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
    namespace Ref = DefinitiveApplicationFrameReference;
    using namespace mta::game_de;
    unsigned checks = 0;
    struct Failure
    {
        const char* message;
    };
    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw Failure{message};
    }
    struct Clock
    {
        std::uint64_t now = 1000;
        unsigned      waits = 0, gapAt = 0, backoffs = 0;
        std::uint64_t Now() const noexcept { return now; }
        void          Wait(std::uint32_t ms) noexcept
        {
            ++waits;
            if (ms == Ref::CleanupBackoffMs)
                ++backoffs;
            now += waits == gapAt ? Ref::MaximumPulseGapMs + 1 : ms;
        }
    };
    struct Owner
    {
        static constexpr std::uint64_t     generation = 0x200000007ull;
        Clock&                             clock;
        ClockServiceStateV1                state;
        ApplicationFrameObservationStateV1 observation;
        unsigned                           begins = 0, renews = 0, reads = 0, observations = 0, ends = 0, resets = 0;
        unsigned                           busyEnds = 0, lyingEnds = 0, failReset = 0, cleanupReadFailures = 0;
        unsigned                           readyDelay = 0, failRenew = 0, failRead = 0, failObservation = 0, invalidObservation = 0, taint = 0;
        unsigned                           throwBegin = 0, throwObservation = 0, throwEnd = 0;
        bool reject = false, noGeneration = false, focused = true, neverReady = false, ending = false, stall = false, absent = false, unavailable = false;
        std::uint64_t beganAt = 0, focusAt = 0;
        bool          canonical = true;
        explicit Owner(Clock& value) : clock(value) {}
        bool Focused() const noexcept { return focused && (!focusAt || clock.now < focusAt); }
        bool Begin(const ClockServiceRequestV1& request, ClockServiceStateV1& output)
        {
            ++begins;
            beganAt = clock.now;
            canonical = canonical && request.size == sizeof(request) && request.version == ClockServiceVersionV1 &&
                        request.maximumDurationMs >= Ref::MaximumDurationMs + Ref::ReadyTimeoutMs && !request.reserved[0] && !request.reserved[1] &&
                        !request.reserved[2];
            state.generation = noGeneration ? 0 : generation;
            state.bound = state.hooked = state.active = state.restored = state.modulePinned = 1;
            state.producerThread = 31;
            output = state;
            if (throwBegin)
                throw 1;
            return !reject;
        }
        bool Renew(std::uint64_t value)
        {
            canonical = canonical && value == generation;
            ++renews;
            ++state.renewals;
            return renews != failRenew;
        }
        bool Read(ClockServiceStateV1& output)
        {
            ++reads;
            if (reads == failRead)
                return false;
            if (ending && cleanupReadFailures)
            {
                --cleanupReadFailures;
                return false;
            }
            if (!ending)
            {
                state.ready = !neverReady && clock.now - beganAt >= readyDelay;
                if (state.ready)
                {
                    state.engineThread = 17;
                    ++state.callbacks;
                    ++state.qualifiedCallbacks;
                }
                if (taint == 1)
                    state.reads = 1;
                if (taint == 2)
                    state.sets = 1;
                if (taint == 3)
                    state.pendingSequence = 1;
                if (taint == 4)
                    state.reserved[0] = 1;
            }
            output = state;
            return true;
        }
        bool ReadObservation(std::uint64_t value, ApplicationFrameObservationStateV1& output)
        {
            ++observations;
            canonical = canonical && value == generation;
            if (observations == throwObservation)
                throw 2;
            if (observations == failObservation)
                return false;
            observation.generation = generation;
            if (state.ready)
            {
                observation.engineThread = 17;
                observation.nativeState = 9;
                if (!stall || !observation.frames)
                    ++observation.frames;
                observation.playingFrames = observation.frames;
                const float delta = 0.03125f;
                std::memcpy(&observation.deltaBits, &delta, sizeof(delta));
                if (unavailable)
                {
                    observation.lookupFailures = observation.frames;
                    observation.lastLookupMask = 0;
                }
                else
                {
                    observation.loadingLookups = observation.simpleFadeLookups = observation.frames;
                    observation.lastLookupMask = LoadingObserved | SimpleFadeObserved;
                    if (!absent)
                    {
                        observation.loadingKeyPresent = observation.loadingNonNull = observation.frames;
                        observation.simpleFadeKeyPresent = observation.simpleFadeNonNull = observation.frames;
                        observation.lastLookupMask = 63;
                    }
                }
            }
            output = observation;
            if (observations == 3)
            {
                if (invalidObservation == 1)
                    output.version = 2;
                if (invalidObservation == 2)
                    ++output.generation;
                if (invalidObservation == 3)
                    output.reserved[3] = 1;
                if (invalidObservation == 4)
                    output.engineThread = 18;
                if (invalidObservation == 5)
                    output.nativeState = 10;
                if (invalidObservation == 6)
                    output.deltaBits = 0x7fc00000;
                if (invalidObservation == 7)
                    output.loadingNonNull = output.frames + 1;
                if (invalidObservation == 8)
                    output.lastLookupMask = LoadingNonNull;
                if (invalidObservation == 9)
                    output.frames = 1;
            }
            return true;
        }
        bool End(std::uint64_t value)
        {
            ++ends;
            ending = true;
            canonical = canonical && value == generation;
            if (ends == throwEnd)
                throw 3;
            if (ends <= busyEnds + lyingEnds)
            {
                state.active = state.ready = 0;
                state.stopping = 1;
                state.inFlight = 1;
                state.retainedPeerThreads = 2;
                return ends > busyEnds;
            }
            const auto prior = state;
            state = {};
            state.generation = generation;
            state.restored = state.modulePinned = 1;
            state.callbacks = prior.callbacks;
            state.qualifiedCallbacks = prior.qualifiedCallbacks;
            return true;
        }
        bool Reset()
        {
            ++resets;
            if (resets <= failReset)
                return false;
            state = {};
            return true;
        }
        bool Clean() const noexcept { return !state.bound && !state.hooked && !state.active && !state.ready && !state.inFlight && !state.retainedPeerThreads; }
        // Deliberately no Submit method: the production template must remain
        // usable by an owner incapable of issuing any CClock command.
    };
    struct Fixture;
    Fixture* current = nullptr;
    bool     Cancelled();
    void     Publish(const std::uint32_t*, std::uint32_t);
    struct Fixture
    {
        Clock         clock;
        Owner         owner{clock};
        Ref::Request  request{200, &Cancelled, &Publish};
        Ref::Snapshot result{}, last{};
        unsigned      publications = 0, pending = 0, throwPublish = 0;
        bool          cancel = false, valid = true, terminalOwned = false;
        std::uint64_t cancelAt = 0;
        void          Run()
        {
            current = this;
            AllocationWatch::requests = 0;
            AllocationWatch::enabled = true;
            result = Ref::RunLifecycle(owner, clock, request);
            AllocationWatch::enabled = false;
            current = nullptr;
            Check(!AllocationWatch::requests, "lifecycle allocated after native admission");
            Check(Ref::Retired(result) && !terminalOwned, "lifecycle returned/published terminal before actual retirement");
            Check(owner.canonical && valid, "generation/request/publication contract changed");
            Check(!result[Ref::ReadSubmitted] && !result[Ref::ReadAcknowledged] && !result[Ref::FullMtaReady],
                  "observer invented clock acknowledgements or full MTA readiness");
            if (request.publish)
                Check(publications && result == last, "final publication disagreed with returned report");
        }
        void Reason(Ref::StopReason reason) { Check(result[Ref::Reason] == static_cast<std::uint32_t>(reason), "unexpected lifecycle stop reason"); }
    };
    bool Cancelled()
    {
        return current->cancel || (current->cancelAt && current->clock.now >= current->cancelAt);
    }
    void Publish(const std::uint32_t* values, std::uint32_t count)
    {
        auto& f = *current;
        ++f.publications;
        if (count != Ref::FieldCount)
        {
            f.valid = false;
            return;
        }
        for (std::size_t i = 0; i < f.last.size(); ++i)
            f.last[i] = values[i];
        f.valid = f.valid && f.last[Ref::Version] == Ref::ApiVersion && f.last[Ref::Sequence] == f.publications;
        for (unsigned i = 88; i < 96; ++i)
            f.valid = f.valid && !f.last[i];
        for (unsigned i = 125; i < 128; ++i)
            f.valid = f.valid && !f.last[i];
        if (f.last[Ref::Phase] == 3)
        {
            ++f.pending;
            f.valid = f.valid && f.last[Ref::CleanupPending] && !f.last[Ref::CleanupComplete];
        }
        if (f.last[Ref::Phase] == 2 && f.owner.begins && !f.owner.Clean())
            f.terminalOwned = true;
        if (f.publications == f.throwPublish)
            throw 4;
    }
    void Tests()
    {
        for (unsigned diagnostic = 0; diagnostic != 3; ++diagnostic)
        {
            Fixture f;
            f.owner.absent = diagnostic == 1;
            f.owner.unavailable = diagnostic == 2;
            f.Run();
            f.Reason(Ref::StopReason::Completed);
            Check(Ref::Completed(f.result) && f.result[Ref::WorkloadComplete] && f.result[Ref::ReadOnlyVerified] && f.result[Ref::ObservationProgressSamples],
                  "normal actual frame observation did not complete without clock commands");
            Check(f.result[Ref::DictionaryVerified] == (diagnostic == 2 ? 0u : 1u), "dictionary availability was confused with workload retirement");
            if (diagnostic == 1)
                Check(!f.result[Ref::LoadingNonNullLow] && !f.result[Ref::SimpleFadeNonNullLow], "absent keys acquired nonnull type authority");
        }
        for (unsigned early = 0; early != 5; ++early)
        {
            Fixture f;
            if (early == 0)
                f.cancel = true;
            if (early == 1)
                f.owner.focused = false;
            if (early == 2)
                f.request.durationMs = 0;
            if (early == 3)
                f.request.cancelled = nullptr;
            if (early == 4)
                f.request.publish = nullptr;
            f.Run();
            Check(!f.owner.begins && !Ref::Completed(f.result), "pre-admission refusal still began the native owner");
        }
        for (unsigned failure = 0; failure != 9; ++failure)
        {
            Fixture f;
            if (failure == 0)
                f.owner.reject = true;
            if (failure == 1)
            {
                f.owner.reject = true;
                f.owner.noGeneration = true;
                f.owner.failReset = 3;
            }
            if (failure == 2)
                f.owner.neverReady = true;
            if (failure == 3)
                f.owner.failRenew = 2;
            if (failure == 4)
                f.owner.failRead = 2;
            if (failure == 5)
                f.owner.failObservation = 2;
            if (failure == 6)
                f.clock.gapAt = 1;
            if (failure == 7)
                f.owner.focusAt = 1100;
            if (failure == 8)
                f.cancelAt = 1100;
            f.Run();
            Check(!Ref::Completed(f.result), "refused/expired/cancelled observation reported workload success");
            if (failure == 1)
                Check(f.owner.resets == 4 && !f.owner.ends, "generation-less Begin debt did not require actual Reset");
            if (failure == 2)
                f.Reason(Ref::StopReason::ReadyTimeout);
        }
        for (unsigned invalid = 1; invalid <= 9; ++invalid)
        {
            Fixture f;
            f.owner.invalidObservation = invalid;
            f.Run();
            f.Reason(Ref::StopReason::NativeError);
            Check(!f.result[Ref::ObservationValid], "invalid native snapshot retained a success flag");
        }
        for (unsigned taint = 1; taint <= 4; ++taint)
        {
            Fixture f;
            f.owner.taint = taint;
            f.Run();
            Check(!Ref::Completed(f.result), "clock mutation or malformed ownership admitted observation success");
        }
        for (unsigned exception = 0; exception != 4; ++exception)
        {
            Fixture f;
            if (exception == 0)
                f.owner.throwBegin = 1;
            if (exception == 1)
                f.owner.throwObservation = 2;
            if (exception == 2)
                f.owner.throwEnd = 1;
            if (exception == 3)
                f.throwPublish = 1;
            f.Run();
            Check(!Ref::Completed(f.result), "exception falsely completed observation workload");
        }
        {
            Fixture f;
            f.owner.busyEnds = 48;
            f.owner.lyingEnds = 3;
            f.owner.cleanupReadFailures = 2;
            f.Run();
            Check(f.result[Ref::CleanupDeadlineExceeded] && f.pending && f.clock.backoffs && f.owner.ends > 51,
                  "deadline/lying End/read failures discarded actual retained native cleanup debt");
            Check(Ref::Retired(f.result) && !Ref::Completed(f.result), "cleanup recovery was confused with workload success");
        }
        {
            Fixture f;
            f.owner.stall = true;
            f.Run();
            f.Reason(Ref::StopReason::NativeError);
            Check(!Ref::Completed(f.result), "unchanging frame counter passed the observation workload");
        }
        {
            Fixture f;
            f.owner.readyDelay = 9500;
            f.Run();
            Check(Ref::Completed(f.result) && f.result[Ref::ElapsedMs] >= 9700, "readiness incorrectly consumed the observation window");
        }
    }
}
int main()
{
    try
    {
        Tests();
    }
    catch (const Failure& failure)
    {
        std::fprintf(stderr, "FAIL: %s\n", failure.message);
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::fputs("FAIL: unexpected fixture exception\n", stderr);
        return EXIT_FAILURE;
    }
    std::puts("Production application-frame reference lifecycle fixture completed");
    std::printf("Passed %u application-frame reference checks\n", checks);
    return EXIT_SUCCESS;
}
