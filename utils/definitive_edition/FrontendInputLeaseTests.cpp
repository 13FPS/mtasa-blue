/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Lease-policy races and unchanged production native trial regression
 *****************************************************************************/

// Include the actual callback implementation to test its mode/freshness branch
// without weakening the production executable pin or installing game hooks.
// Existing NativeMovementInputTests independently cover scoped restoration.
#define MTA_TEST_FRONTEND_INPUT_LEASE_ACCESS
#include "../../Client/game_de/NativeMovementInput.cpp"
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace mta::game_de;

namespace mta::game_de
{
    struct FrontendInputLeasePolicyTestAccess
    {
        static std::uint64_t Deadline(const FrontendInputLeasePolicy& policy) { return policy.deadline.load(); }
        static bool Observe(FrontendInputLeasePolicy& policy, std::uint64_t now, std::uint64_t observed) { return policy.ObserveDeadline(now, observed); }
    };
}

namespace
{
    unsigned checks = 0;
    void     Check(bool condition, const char* label)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, label);
            std::exit(1);
        }
    }

    void LeasePolicyTests()
    {
        const MtaInputState      zero;
        FrontendInputLeasePolicy policy;
        Check(!policy.Begin(0, 7, 1000, true, zero), "lease rejects zero generation");
        Check(!policy.Begin(1, 0, 1000, true, zero), "lease rejects zero producer");
        Check(!policy.Begin(1, 7, 1000, false, zero), "lease requires foreground family before admission");
        MtaInputState movement;
        movement.current.LeftStickY = -128;
        Check(!policy.Begin(1, 7, 1000, true, movement), "lease cannot begin with movement");
        Check(policy.Begin(1, 7, 1000, true, zero), "neutral lease begins with generation and producer");
        Check(!policy.Begin(2, 7, 1000, true, zero), "active generation cannot be replaced");
        Check(!policy.Renew(2, 7, 1001, true, zero), "foreign generation cannot renew");
        Check(!policy.Renew(1, 8, 1001, true, zero), "foreign producer cannot renew");
        Check(policy.Read().refreshedAtMs == 1000 && !policy.Revoked(), "foreign callers neither extend nor revoke current lease");
        Check(policy.Check(1499, true), "lease remains valid before fixed expiry boundary");
        Check(!policy.Check(1500, true), "lease expires at exact 500 millisecond boundary");
        Check(policy.Read().reason == static_cast<unsigned>(FrontendInputLeaseReason::Expired), "expiry reason is retained");
        Check(!policy.Renew(1, 7, 1500, true, zero), "expired token cannot resurrect");
        Check(!policy.Begin(1, 7, 1501, true, zero), "old generation cannot be restarted");
        Check(policy.Begin(2, 7, 2000, true, zero), "new generation follows explicitly revoked lease");
        Check(policy.Renew(2, 7, 2499, true, zero), "producer can renew immediately before expiry");
        Check(policy.Check(2998, true), "renewal extends only its short lease");
        Check(!policy.Check(2999, true), "renewed lease expires at exact boundary");
        Check(policy.Begin(3, 7, 3000, true, zero), "next independent lease begins");
        movement = {};
        movement.previous.ButtonCross = 255;
        Check(!policy.Renew(3, 7, 3001, true, movement), "previous input must also be exactly neutral");
        Check(policy.Read().reason == static_cast<unsigned>(FrontendInputLeaseReason::NonNeutral), "non-neutral owner publication permanently revokes");
        Check(!policy.Renew(3, 7, 3002, true, zero), "neutral retry cannot erase invalid publication");
        Check(policy.Begin(4, 7, 4000, true, zero), "clock regression case begins");
        Check(policy.Check(4100, true), "consumer observes increasing time");
        Check(!policy.Check(4099, true), "consumer clock rollback disarms");
        Check(policy.Read().reason == static_cast<unsigned>(FrontendInputLeaseReason::ClockRollback), "clock rollback reason retained");
        Check(policy.Begin(5, 7, 5000, true, zero), "producer clock case begins");
        Check(policy.Renew(5, 7, 5100, true, zero), "producer clock advances");
        Check(!policy.Renew(5, 7, 5099, true, zero), "producer clock rollback disarms");
        Check(policy.Begin(6, 7, 6000, true, zero), "cross-actor ordering case begins");
        Check(policy.Renew(6, 7, 6100, true, zero), "producer publishes later sampled clock first");
        Check(policy.Check(6050, true), "earlier consumer sample is not actor-local rollback");
        Check(!policy.Check(6101, false), "focus loss disarms before next input scope");
        Check(!policy.Check(6102, true) && !policy.Renew(6, 7, 6102, true, zero), "focus return never silently rearms revoked token");

        // Simulated long runtime uses the production policy, not a larger
        // timeout. Every individual renewal is still bounded to 500 ms.
        Check(policy.Begin(7, 7, 7000, true, zero), "long runtime begins neutral");
        for (std::uint64_t now = 7100; now <= 127000; now += 100)
        {
            Check(policy.Renew(7, 7, now, true, zero), "long runtime producer renews short lease");
            Check(policy.Check(now + 1, true), "long runtime callback remains eligible beyond old 35 seconds");
        }
        Check(policy.Read().renewals == 1200, "long runtime accounts exact renewal count");
        policy.Revoke(FrontendInputLeaseReason::Stopped);
        Check(!policy.Renew(7, 7, 127001, true, zero), "explicit stop prevents subsequent renewal");

        FrontendInputLeasePolicy renewalRace;
        for (std::uint64_t generation = 1; generation <= 100; ++generation)
        {
            const auto base = generation * 1000;
            Check(renewalRace.Begin(generation, 7, base, true, zero), "timely renewal race begins");
            const auto observed = FrontendInputLeasePolicyTestAccess::Deadline(renewalRace);
            Check(renewalRace.Renew(generation, 7, base + 499, true, zero), "timely renewal wins before stale expiration CAS");
            Check(FrontendInputLeasePolicyTestAccess::Observe(renewalRace, base + 500, observed) && !renewalRace.Revoked(),
                  "old callback deadline observation cannot expire a timely renewed lease");
            Check(!renewalRace.Check(base + 999, true), "renewed deadline still expires exactly");
        }

        // Stop and renewal may race on different real threads. Publication can
        // finish first, but no interleaving may clear the irreversible stop.
        for (std::uint64_t generation = 8; generation < 108; ++generation)
        {
            Check(policy.Begin(generation, 7, 200000, true, zero), "race starts new generation");
            std::atomic<bool> go{false};
            std::thread       renewer(
                [&]
                {
                    while (!go.load(std::memory_order_acquire))
                        std::this_thread::yield();
                    policy.Renew(generation, 7, 200001, true, zero);
                });
            std::thread stopper(
                [&]
                {
                    while (!go.load(std::memory_order_acquire))
                        std::this_thread::yield();
                    policy.Revoke(FrontendInputLeaseReason::Stopped);
                });
            go.store(true, std::memory_order_release);
            renewer.join();
            stopper.join();
            Check(policy.Revoked() && !policy.Read().active && !policy.Renew(generation, 7, 200002, true, zero),
                  "concurrent stop cannot be reversed by racing renewal");
        }

        // The actual production freshness branch still expires ordinary
        // movement at 35 seconds and stale publication at two seconds.
        runtime.frontendLease = false;
        runtime.started = GetTickCount64() - maximumTrialMs;
        runtime.active.store(true);
        ClearCounters();
        Check(!Fresh(GetTickCount64()) && runtime.counters.reason.load() == static_cast<unsigned>(Failure::Expired),
              "legacy native movement retains absolute 35 second cap");
        runtime.started = GetTickCount64();
        runtime.active.store(true);
        ClearCounters();
        Check(!Fresh(GetTickCount64() - maximumInputAgeMs - 1) && runtime.counters.reason.load() == static_cast<unsigned>(Failure::StaleInput),
              "legacy native movement retains two second publication freshness");
        FrontendInputLeaseRequestV1 request;
        Check(!NativeMovementInput::ValidateFrontendLeaseRequest(request), "invalid root HWND cannot admit native lease");
        runtime.frontendLease = true;
        runtime.frontendWindow = nullptr;
        const auto generation = runtime.generation.load() + 1;
        runtime.frontendPolicy.Revoke(FrontendInputLeaseReason::Stopped);
        Check(runtime.frontendPolicy.Begin(generation, GetCurrentThreadId(), GetTickCount64(), true, zero), "production lease focus case prepared");
        runtime.active.store(true);
        ClearCounters();
        Check(!Fresh(GetTickCount64()) && !runtime.active.load() &&
                  runtime.frontendPolicy.Read().reason == static_cast<unsigned>(FrontendInputLeaseReason::FocusLost),
              "production callback rejects invalid foreground family without applying input");
        runtime.frontendLease = false;
    }
}

int main()
{
    LeasePolicyTests();
    std::printf("Passed %u native frontend lease checks. No GTA or game hooks.\n", checks);
    return 0;
}
