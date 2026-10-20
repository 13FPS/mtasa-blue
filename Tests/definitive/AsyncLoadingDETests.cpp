/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify DE startup/model-loading timing without running the game
 *****************************************************************************/
#include "../../Client/game_de/CAsyncLoadingDE.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace
{
    std::int64_t nowMs = 0;
    unsigned     clockCalls = 0;
    unsigned     checks = 0;

    std::int64_t ReadClock() noexcept
    {
        ++clockCalls;
        return nowMs;
    }
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "DE async-loading check failed: %s\n", message);
            std::abort();
        }
    }
}

unsigned int RunAsyncLoadingDETests()
{
    using mta::game_de::CAsyncLoadingDE;
    CAsyncLoadingDE loading(ReadClock);
    Check(loading.IsEnabled(), "startup default allows loading");
    Check(clockCalls == 0, "untimed queries do not sample a clock");
    loading.SetScriptOverride(false, false);
    Check(loading.IsEnabled(), "unforced script preference preserves MTA default");
    loading.SetScriptOverride(false, true);
    Check(!loading.IsEnabled(), "forced script override disables loading");
    loading.SetScriptOverride(true, true);
    nowMs = 0xfffffff0LL;
    loading.Suspend(true, 32);
    Check(clockCalls == 1, "timed suspension samples once");
    nowMs = 0x100000010LL;
    Check(!loading.IsEnabled(), "32-bit wrap and exact deadline remain suspended");
    Check(loading.IsEnabled(true), "ignore-suspend honors enabled script override");
    Check(!loading.IsEnabled(), "ignore-suspend at equality does not clear suspension");
    ++nowMs;
    Check(loading.IsEnabled(true), "post-deadline ignore query processes expiry");
    const auto afterExpiry = clockCalls;
    Check(loading.IsEnabled(), "expiry persists into ordinary queries");
    Check(clockCalls == afterExpiry, "expired policy does not keep reading time");
    loading.Suspend(true, 0);
    Check(!loading.IsEnabled(), "zero delay is indefinite suspension");
    loading.Suspend(false, 99);
    Check(loading.IsEnabled(), "unsuspend ignores a supplied delay");
    Check(clockCalls == afterExpiry, "indefinite suspension and clearing need no clock");
    nowMs = 500;
    loading.Suspend(true, 100);
    loading.Suspend(true, 0);
    nowMs = 1000;
    Check(!loading.IsEnabled(), "indefinite suspension replaces an old deadline");
    loading.Suspend(true, 100);
    nowMs = 1101;
    loading.SetScriptOverride(false, true);
    Check(!loading.IsEnabled(), "expiry does not override forced script disable");
    loading.SetScriptOverride(false, false);
    Check(loading.IsEnabled(), "unforcing uses the existing MTA default after expiry");
    loading.Suspend(true, 0);
    nowMs = std::numeric_limits<std::int64_t>::max();
    bool overflow = false;
    try
    {
        loading.Suspend(true, 1);
    }
    catch (const std::overflow_error&)
    {
        overflow = true;
    }
    Check(overflow, "overflow is explicit");
    Check(!loading.IsEnabled(), "overflow preserves the previous suspension");
    CAsyncLoadingDE defaults;
    Check(defaults.IsEnabled(), "real monotonic clock supports the startup policy");
    CAsyncLoadingDE nullClock(nullptr);
    Check(nullClock.IsEnabled(), "null injection uses the default clock");
    std::printf("DE async-loading clock checks: %u\n", checks);
    return checks;
}
