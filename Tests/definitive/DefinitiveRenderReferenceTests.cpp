/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute Core's actual value-only render lifecycle without the game
 *****************************************************************************/

#include "../../Client/core/DefinitiveRenderReference.h"
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <vector>

namespace
{
    using namespace DefinitiveRenderReference;
    unsigned int checks = 0;
    void         Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    }
    struct Clock
    {
        std::uint64_t now = 1000;
        std::uint32_t regularWait = 50;
        std::uint64_t Now() const noexcept { return now; }
        void          Wait(std::uint32_t milliseconds) noexcept { now += milliseconds == 50 ? regularWait : milliseconds; }
    };
    struct Observer
    {
        Clock&        clock;
        unsigned int  begins = 0, reads = 0, ends = 0, busyEnds = 0;
        bool          began = false, ended = false, rejectBegin = false, rejectRead = false, rejectFinalRead = false;
        bool          nativeFailure = false, throwRead = false, throwFirstEnd = false;
        std::uint32_t lateEnd = 0;
        bool          Begin()
        {
            ++begins;
            began = !rejectBegin;
            return began;
        }
        bool Read(Snapshot& snapshot)
        {
            ++reads;
            if (!began || rejectRead || (ended && rejectFinalRead))
                return false;
            if (throwRead && !ended)
                throw std::runtime_error("controlled read exception");
            snapshot[Bound] = snapshot[ModulePinned] = snapshot[BackendPinnedExpected] = 1;
            snapshot[Active] = ended ? 0u : 1u;
            snapshot[NativeCleanupPending] = ends && !ended ? 1u : 0u;
            snapshot[GenerationLow] = 7;
            snapshot[GenerationHigh] = 2;
            snapshot[PresentCallbacksLow] = 123;
            snapshot[AcceptedSnapshotsLow] = 120;
            snapshot[Width] = 1280;
            snapshot[Height] = 720;
            snapshot[NativePresence] = 15;
            snapshot[NativeOriginalReturned] = 1;
            snapshot[NativeOriginalResult] = 0;  // A returned false bool is not an observer failure.
            snapshot[FailureReason] = nativeFailure ? 4u : 0u;
            return true;
        }
        bool End()
        {
            ++ends;
            if (throwFirstEnd && ends == 1)
                throw std::runtime_error("controlled End exception");
            if (ends <= busyEnds)
                return false;
            clock.now += lateEnd;
            ended = true;
            return true;
        }
    };
    struct Request
    {
        std::uint32_t                                            durationMs = MaximumDurationMs;
        std::function<bool()>                                    cancelled;
        std::function<void(const std::uint32_t*, std::uint32_t)> publish;
    };
    struct Fixture
    {
        Clock                 clock;
        Observer              observer{clock};
        Request               request;
        std::vector<Snapshot> publications;
        bool                  cancel = false, throwPublish = false;
        Fixture()
        {
            request.cancelled = [&] { return cancel; };
            request.publish = [&](const std::uint32_t* state, std::uint32_t count)
            {
                Check(count == FieldCount, "Core must publish the exact forty-field contract");
                if (throwPublish && state[Phase] == 1)
                    throw std::runtime_error("controlled publication failure");
                Snapshot copy{};
                std::copy_n(state, FieldCount, copy.begin());
                if (!publications.empty())
                    Check(copy[Sequence] > publications.back()[Sequence], "every publication must advance sequence");
                if (copy[Phase] == 3)
                    Check(!observer.ended && copy[CleanupPending] == 1 && copy[CleanupComplete] == 0, "pending snapshots cannot imply a released native owner");
                publications.push_back(copy);
            };
        }
        Snapshot Run() { return DefinitiveRenderReference::Run(observer, clock, request); }
    };
    void Terminal(const Fixture& fixture, const Snapshot& state, StopReason reason, bool clean = true)
    {
        Check(fixture.observer.ended && fixture.observer.ends > 0, "every exit must finish actual observer End");
        Check(state[Version] == 1 && state[Phase] == 2 && state[CleanupPending] == 0 && state[CleanupComplete] == (clean ? 1u : 0u),
              "terminal publication must accurately describe cleanup");
        Check(state[Reason] == static_cast<std::uint32_t>(reason), "stop reason must survive the cleanup path");
        Check(state[Reserved] == 0 && state[ReservedHigh] == 0, "reserved fields must remain neutral");
    }
}

int main()
{
    try
    {
        {
            Fixture    f;
            const auto state = f.Run();
            Terminal(f, state, StopReason::TimeBudget);
            Check(state[SampleCount] == 200 && state[ElapsedMs] == 10000 && f.publications.size() == 201,
                  "ten-second budget must allow exactly two hundred fifty-millisecond observations");
            Check(state[Bound] == 1 && state[Active] == 0 && state[ModulePinned] == 1 && state[GenerationHigh] == 2 && state[PresentCallbacksLow] == 123 &&
                      state[AcceptedSnapshotsLow] == 120 && state[NativeOriginalResult] == 0,
                  "final state must preserve actual ended counters and a valid false native result");
        }
        {
            Fixture f;
            f.cancel = true;
            const auto state = f.Run();
            Terminal(f, state, StopReason::Cancelled);
            Check(f.observer.begins == 0 && state[ObserverStarted] == 0 && state[SampleCount] == 0 && state[Bound] == 0,
                  "known cancellation must not install an observer or invent native state");
        }
        {
            Fixture f;
            f.request.cancelled = [&] { return f.clock.now >= 1150; };
            const auto state = f.Run();
            Terminal(f, state, StopReason::Cancelled);
            Check(state[SampleCount] == 3 && state[ElapsedMs] == 150, "cancellation must stop before another native read/publication");
        }
        {
            Fixture f;
            f.observer.rejectBegin = true;
            Terminal(f, f.Run(), StopReason::BeginError);
            Check(f.observer.ends == 1, "failed Begin must still attempt cleanup of a partial lease");
        }
        {
            Fixture f;
            f.observer.rejectRead = true;
            Terminal(f, f.Run(), StopReason::CleanupError, false);
        }
        {
            Fixture f;
            f.observer.rejectFinalRead = true;
            Terminal(f, f.Run(), StopReason::CleanupError, false);
        }
        {
            Fixture f;
            f.observer.nativeFailure = true;
            Terminal(f, f.Run(), StopReason::ReadError);
        }
        {
            Fixture f;
            f.observer.throwRead = true;
            Terminal(f, f.Run(), StopReason::Exception);
        }
        {
            Fixture f;
            f.throwPublish = true;
            Terminal(f, f.Run(), StopReason::Exception);
        }
        {
            Fixture f;
            f.clock.regularWait = 1001;
            Terminal(f, f.Run(), StopReason::PulseGap);
        }
        {
            Fixture f;
            f.clock.regularWait = 1;
            Terminal(f, f.Run(), StopReason::SampleBudget);
        }
        {
            Fixture f;
            f.observer.busyEnds = 3;
            const auto state = f.Run();
            Terminal(f, state, StopReason::TimeBudget);
            Check(state[CleanupAttempts] == 4 && state[CleanupDeadlineExceeded] == 0 && f.publications.size() == 204,
                  "brief cleanup retries must retain the owner and final actual readback");
        }
        {
            Fixture f;
            f.observer.busyEnds = 42;
            const auto state = f.Run();
            Terminal(f, state, StopReason::CleanupError);
            Check(state[CleanupDeadlineExceeded] == 1 && state[CleanupAttempts] == 43 && state[Bound] == 1 && state[Active] == 0,
                  "deadline must stay failed even after safe eventual End and actual final state");
        }
        {
            Fixture f;
            f.observer.lateEnd = 2001;
            const auto state = f.Run();
            Terminal(f, state, StopReason::CleanupError);
            Check(state[CleanupDeadlineExceeded] == 1, "late successful End must not erase the cleanup deadline");
        }
        {
            Fixture f;
            f.observer.throwFirstEnd = true;
            Terminal(f, f.Run(), StopReason::Exception);
            Check(f.observer.ends == 2, "unexpected End exception must retain the owner for another cleanup attempt");
        }
        std::printf("{\"success\":true,\"checks\":%u,\"game_operations\":false}\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
