/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify client startup/model-request policy without a game backend
 *****************************************************************************/
#include "../../Shared/sdk/CAsyncLoadingPolicy.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <type_traits>

unsigned int RunAsyncLoadingDETests();

namespace
{
    unsigned int checks = 0;

    void Check(bool condition, const char* description)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    using MtaGame::CAsyncLoadingPolicy;
    static_assert(std::is_nothrow_default_constructible_v<CAsyncLoadingPolicy>);
    CAsyncLoadingPolicy policy;
    Check(policy.IsEnabled(), "Default startup permits asynchronous requests");
    Check(!policy.HasAutoUnsuspendDeadline(), "Default state has no expiry");

    // Lua's enabled preference takes effect only when forced. Suspension is a
    // separate override, and ignoring it must not ignore a script's forced off.
    for (const bool enabled : {false, true})
        for (const bool forced : {false, true})
        {
            policy.SetScriptOverride(enabled, forced);
            policy.Suspend(false);
            Check(policy.IsEnabled() == (!forced || enabled), "Script force and preference precedence");
            policy.Suspend(true);
            Check(!policy.IsEnabled(), "Suspension overrides every script preference");
            Check(policy.IsEnabled(true) == (!forced || enabled), "Ignoring suspension preserves script force");
            Check(!policy.HasAutoUnsuspendDeadline(), "Indefinite suspension has no implicit timeout");
        }

    // These sequences follow the ped/vehicle temporary blocking-loading window:
    // the boundary itself stays blocked, and the next query releases it.
    policy.SetScriptOverride(true, false);
    policy.Suspend(true, 6000);
    Check(policy.HasAutoUnsuspendDeadline(), "Temporary model load schedules expiry");
    Check(!policy.IsEnabled(false, 1000), "Temporary model load starts suspended");
    Check(!policy.IsEnabled(false, 6000), "Exact deadline remains suspended");
    Check(policy.HasAutoUnsuspendDeadline(), "Exact deadline does not consume expiry");
    Check(policy.IsEnabled(false, 6001), "First query after deadline unsuspends");
    Check(!policy.HasAutoUnsuspendDeadline(), "Expiry retires the deadline");
    Check(policy.IsEnabled(false, 0), "An expired deadline cannot resuspend on a later earlier clock value");

    policy.Suspend(true, 10);
    Check(policy.IsEnabled(true, 11), "Ignore-suspension query also processes expiry");
    Check(policy.IsEnabled(false, 0), "Expiry processed by diagnostic query remains retired");
    policy.SetScriptOverride(false, true);
    policy.Suspend(true, 10);
    Check(!policy.IsEnabled(true, 11), "Expiry does not undo forced synchronous loading");
    Check(!policy.HasAutoUnsuspendDeadline(), "Forced synchronous mode still retires expired deadline");
    policy.SetScriptOverride(true, false);
    Check(policy.IsEnabled(), "Removing script force exposes the already expired suspension");

    policy.Suspend(true, 50);
    policy.Suspend(false, 100);
    Check(!policy.HasAutoUnsuspendDeadline() && policy.IsEnabled(), "Explicit resume cancels an earlier deadline");
    policy.Suspend(true, 10);
    policy.Suspend(true);
    Check(!policy.HasAutoUnsuspendDeadline() && !policy.IsEnabled(false, 100), "Indefinite suspend replaces an earlier timer");
    policy.Suspend(true, 200);
    Check(!policy.IsEnabled(false, 100), "A replacement timer is not affected by the old expiry");
    policy.Suspend(true, 300);
    Check(!policy.IsEnabled(false, 250), "Extending suspension replaces the old deadline");
    Check(policy.IsEnabled(false, 301), "Replacement timer eventually expires");

    // CTickCount uses signed 64-bit milliseconds. Values spanning the 32-bit
    // wrap must retain their full width on both x86 and x64 client builds.
    constexpr std::int64_t after32BitWrap = (std::int64_t{1} << 32) + 5000;
    policy.Suspend(true, after32BitWrap);
    Check(!policy.IsEnabled(false, after32BitWrap - 1), "64-bit deadline is not narrowed before comparison");
    Check(!policy.IsEnabled(false, after32BitWrap), "64-bit deadline preserves strict equality");
    Check(policy.IsEnabled(false, after32BitWrap + 1), "64-bit deadline expires after 32-bit wrap");
    policy.Suspend(true, (std::numeric_limits<std::int64_t>::max)());
    Check(!policy.IsEnabled(false, (std::numeric_limits<std::int64_t>::max)()), "Largest signed deadline stays suspended at equality");
    Check(!policy.IsEnabled(false, (std::numeric_limits<std::int64_t>::min)()), "Earlier signed clock value does not fabricate expiry");
    policy.Suspend(true, -5);
    Check(!policy.IsEnabled(false, -5), "Signed CTickCount equality is preserved");
    Check(policy.IsEnabled(false, -4), "Signed CTickCount ordering is preserved");
    policy.Suspend(true, 0);
    Check(!policy.HasAutoUnsuspendDeadline() && !policy.IsEnabled(false, after32BitWrap), "Zero is always the original no-deadline sentinel");

    // Policy belongs to a game instance; one session's forced/suspended state
    // cannot affect another instance or require native streaming availability.
    CAsyncLoadingPolicy fresh;
    Check(fresh.IsEnabled() && !fresh.HasAutoUnsuspendDeadline(), "A new game instance has independent defaults");
    checks += RunAsyncLoadingDETests();
    std::printf("Passed %u shared MTA async-loading policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
}
