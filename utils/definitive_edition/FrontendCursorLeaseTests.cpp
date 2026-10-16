/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real cursor-lease policy with owned callback/retirement fixtures
 *****************************************************************************/
#include "../../Client/core/FrontendCursorLeasePolicy.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace
{
    std::atomic<unsigned> checks{0};
    void                  Check(bool condition, const char* label)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks.load(), label);
            std::exit(1);
        }
    }

    // This fixture executes the production policy and callback ownership order,
    // then records passthrough instead of calling the OS. No test installs a
    // SetCursorPos detour, changes cursor position or changes cursor clipping.
    struct CallbackFixture
    {
        FrontendCursorLease::Policy policy;
        unsigned                    originalCalls = 0;
        int                         lastX = 0, lastY = 0;

        bool Invoke(std::uint64_t now, bool focused, bool ownedCaller, int x, int y)
        {
            policy.EnterCallback();
            struct Leave
            {
                FrontendCursorLease::Policy& policy;
                ~Leave() { policy.LeaveCallback(); }
            } leave{policy};
            if (policy.Suppress(now, focused, ownedCaller))
                return true;
            ++originalCalls;
            lastX = x;
            lastY = y;
            return false;
        }
    };

    void AdmissionExpiryAndRouting()
    {
        using namespace FrontendCursorLease;
        CallbackFixture fixture;
        auto&           policy = fixture.policy;
        Check(!policy.Read().active && policy.Read().ended && policy.Read().revoked, "unstarted cursor lease is harmless");
        Check(!policy.Begin(0, 1000), "zero owner refused");
        Check(!policy.Begin(41, UINT64_MAX - LifetimeMs + 1), "deadline overflow refused");
        Check(policy.Begin(41, 1000), "owning producer begins bounded cursor lease");
        const auto generation = policy.Read().generation;
        Check(generation != 0 && policy.Read().lifetimeMs == 500, "generation and fixed lifetime reported");
        Check(!policy.Begin(41, 1001), "active cursor owner cannot be replaced");
        Check(!policy.Renew(42, generation, 1001, true), "foreign producer cannot renew");
        Check(!policy.Renew(41, generation + 1, 1001, true), "foreign generation cannot renew");
        Check(!policy.Complete(42) && policy.Read().active, "foreign cleanup cannot revoke live owner");
        Check(fixture.Invoke(1001, true, true, 11, 22), "main image warp suppressed within foreground family");
        Check(fixture.originalCalls == 0, "suppression never calls recording original");
        Check(!fixture.Invoke(1002, true, false, 33, 44), "caller outside admitted image passes through");
        Check(fixture.originalCalls == 1 && fixture.lastX == 33 && fixture.lastY == 44, "passthrough preserves exact requested coordinates");
        Check(policy.Read().renewals == 0 && policy.Read().inFlight == 0, "callbacks neither renew nor retain completed ownership");
        Check(fixture.Invoke(1499, true, true, 1, 2), "suppression remains valid one millisecond before expiry");
        Check(!fixture.Invoke(1500, true, true, 3, 4), "exact 500 millisecond deadline passes through");
        Check(policy.Read().reason == static_cast<unsigned>(Reason::Expired), "exact deadline retains expiry reason");
        Check(!policy.Renew(41, generation, 1500, true), "expired cursor lease cannot resurrect");
        Check(!fixture.Invoke(1501, true, true, 5, 6), "expired owned callback remains passthrough");
        const auto stopped = policy.Read();
        Check(stopped.callbacks == stopped.suppressed + stopped.passthrough && stopped.callbacks == 5, "every suppressed and original callback is accounted");
        Check(policy.Complete(41) && policy.Read().ended, "quiescent expired lease explicitly retires");
        Check(!policy.Begin(42, 2000), "replacement producer cannot inherit old owner identity");
        Check(policy.Begin(41, 2000) && policy.Read().generation > generation, "explicit new lifetime gets increasing generation");
        Check(!policy.Renew(41, generation, 2001, true) && policy.Read().active, "retired token cannot renew or revoke replacement lifetime");
        Check(policy.Complete(41), "replacement owner retires");
    }

    void ProducerClockAndFocus()
    {
        using namespace FrontendCursorLease;
        CallbackFixture fixture;
        auto&           policy = fixture.policy;
        Check(policy.Begin(7, 1000), "producer clock case begins");
        auto generation = policy.Read().generation;
        Check(policy.Renew(7, generation, 1400, true), "timely owner refresh accepted");
        Check(!policy.Renew(7, generation, 1399, true), "owner clock rollback revokes");
        Check(policy.Read().reason == static_cast<unsigned>(Reason::ClockRollback) && !policy.Read().active, "owner rollback is irreversible evidence");
        Check(!fixture.Invoke(1401, true, true, 1, 2) && policy.Complete(7), "clock failure passes through and retires");
        Check(policy.Begin(7, 2000), "focus case begins");
        generation = policy.Read().generation;
        Check(!fixture.Invoke(2001, false, true, 3, 4), "owned caller outside foreground window family passes through");
        Check(policy.Read().reason == static_cast<unsigned>(Reason::FocusLost), "focus family loss revokes immediately");
        Check(!policy.Renew(7, generation, 2002, true), "returning focus cannot silently rearm old generation");
        Check(!fixture.Invoke(2003, true, true, 5, 6) && policy.Complete(7), "focus-failed lifetime remains passthrough until explicit retirement");
        Check(policy.Begin(7, 3000), "unordered callback clocks case begins");
        generation = policy.Read().generation;
        Check(policy.Renew(7, generation, 3100, true), "producer publishes later sampled time");
        Check(fixture.Invoke(3050, true, true, 1, 2), "earlier callback sample does not manufacture owner rollback");
        Check(fixture.Invoke(3049, true, true, 1, 2), "different callback threads may sample time in either order");
        Check(policy.Read().active && policy.Read().renewals == 1, "callback clock samples do not extend deadline");
        Check(!fixture.Invoke(3600, true, true, 1, 2), "renewed deadline expires at exactly 500 milliseconds");
        Check(policy.Complete(7), "unordered callback case retires");
    }

    void CallbacksNeverRenew()
    {
        CallbackFixture fixture;
        Check(fixture.policy.Begin(9, 10000), "callback-only workload begins");
        for (std::uint64_t now = 10001; now < 10500; ++now)
            Check(fixture.Invoke(now, true, true, 0, 0), "frequent callbacks remain bounded by original expiry");
        Check(!fixture.Invoke(10500, true, true, 0, 0), "499 callbacks cannot extend producer's original lease");
        Check(fixture.policy.Read().renewals == 0 && fixture.policy.Read().suppressed == 499 && fixture.originalCalls == 1,
              "callback traffic never masquerades as independent renewal");
        Check(fixture.policy.Complete(9), "callback-only workload retires");
    }

    void BusyPassthroughAndRetirement()
    {
        using namespace FrontendCursorLease;
        for (unsigned attempt = 0; attempt < 200; ++attempt)
        {
            const bool explicitMtaCall = attempt >= 100;
            Policy     policy;
            Check(policy.Begin(31, 1000), "owned passthrough cleanup case begins");
            std::mutex              mutex;
            std::condition_variable changed;
            bool                    enteredOriginal = false, finishOriginal = false;
            std::atomic<bool>       suppressed{true};
            std::thread             callback(
                [&]
                {
                    policy.EnterCallback();
                    if (explicitMtaCall)
                    {
                        // MTA's explicit original-call path bypasses suppression,
                        // but owns its whole forwarded frame until return too.
                        policy.RecordPassthrough();
                        suppressed.store(false);
                    }
                    else
                        suppressed.store(policy.Suppress(1001, true, false));
                    // The dummy original is blocked under full callback ownership.
                    // Passthrough must retain its trampoline just like suppression.
                    std::unique_lock<std::mutex> lock(mutex);
                    enteredOriginal = true;
                    changed.notify_one();
                    changed.wait(lock, [&] { return finishOriginal; });
                    lock.unlock();
                    policy.LeaveCallback();
                });
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return enteredOriginal; });
            }
            Check(!suppressed.load() && policy.Read().inFlight == 1 && policy.Read().passthrough == 1,
                  "outside-image or explicit MTA original remains in flight throughout passthrough");
            Check(policy.Read().callbacks == 1 && policy.Read().suppressed == 0 && policy.Read().renewals == 0 && policy.Read().active,
                  "forwarding records ownership without suppression or implicit renewal");
            Check(!policy.Complete(31) && !policy.Read().ended && policy.Read().revoked && !policy.Read().active,
                  "cleanup Busy disarms but retains owner while original is in flight");
            Check(!policy.Begin(31, 1002), "Busy callback prevents generation reuse");
            {
                std::lock_guard<std::mutex> lock(mutex);
                finishOriginal = true;
            }
            changed.notify_one();
            callback.join();
            Check(policy.Read().inFlight == 0 && policy.Complete(31) && policy.Read().ended, "same owner retries cleanup after original returns");
        }
    }
}

int main()
{
    AdmissionExpiryAndRouting();
    ProducerClockAndFocus();
    CallbacksNeverRenew();
    BusyPassthroughAndRetirement();
    std::printf("Passed %u frontend cursor lease policy checks. No OS cursor writes or game hooks.\n", checks.load());
    return 0;
}
