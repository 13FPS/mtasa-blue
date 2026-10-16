/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Deterministic independent host expiry and stale-generation checks
 *****************************************************************************/
#define MTA_TEST_PERSISTENT_FRONTEND_HOST_ACCESS
#include "../../Client/definitive/PersistentFrontendHostLease.h"
#include <cstdio>
#include <stdexcept>

namespace deonline
{
    struct PersistentFrontendHostLeaseTestAccess
    {
        static void Exhaust(PersistentFrontendHostLease& lease) { lease.generation = UINT32_MAX - 1; }
    };
}
int main()
{
    unsigned   checks = 0;
    const auto check = [&](bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    };
    try
    {
        deonline::PersistentFrontendHostLease lease;
        check(!lease.Fresh(1, 0) && !lease.Heartbeat(1, 1, 0), "unbegun lease is unavailable");
        check(!lease.Begin(0, 0), "zero owner rejected");
        const auto first = lease.Begin(7, 100);
        check(first == 1 && lease.Generation() == first, "nonzero monotone initial generation");
        check(lease.Fresh(first, 100) && lease.Fresh(first, 199), "fresh strictly inside one-second budget");
        check(!lease.Heartbeat(8, first, 200) && !lease.Heartbeat(7, first + 1, 200), "foreign owner and wrong generation cannot renew");
        check(lease.Fresh(first, 200), "invalid caller does not revoke valid owner");
        check(lease.Heartbeat(7, first, 201) && lease.Fresh(first, 200), "host-ahead sample does not manufacture worker rollback");
        check(lease.Fresh(first, 1099), "observer remains fresh late inside initial budget");
        check(lease.Heartbeat(7, first, 1000) && lease.Fresh(first, 1999), "host alone extends deadline");
        check(!lease.Fresh(first, 2000), "exact boundary expires");
        check(!lease.Heartbeat(7, first, 2000) && !lease.Fresh(first, 2001), "expired generation is irrevocable");
        check(!lease.Begin(8, 2001), "foreign owner cannot replace generation");
        const auto second = lease.Begin(7, 2001);
        check(second == first + 1 && !lease.Fresh(first, 2001), "new generation cannot revive old token");
        check(!lease.Heartbeat(7, first, 2002) && lease.Heartbeat(7, second, 2002), "only current generation renews");
        lease.Revoke();
        check(!lease.Fresh(second, 2002) && !lease.Heartbeat(7, second, 2003), "explicit revocation cannot renew");
        const auto third = lease.Begin(7, 4000);
        check(third == second + 1, "fresh attempts consume a new token");
        check(!lease.Heartbeat(7, third, 3999) && !lease.Fresh(third, 4000), "regressed owner clock revokes");
        const auto fourth = lease.Begin(7, 5000);
        check(!lease.Fresh(fourth, 4999) && !lease.Heartbeat(7, fourth, 5000), "regressed observer clock cannot revive");
        check(!lease.Begin(7, UINT64_MAX - 999), "overflowing expiry is refused");
        const auto fifth = lease.Begin(7, 6000);
        check(fifth == fourth + 1 && !lease.Heartbeat(7, fifth, UINT64_MAX), "overflowing heartbeat revokes");
        deonline::PersistentFrontendHostLeaseTestAccess::Exhaust(lease);
        check(lease.Begin(7, 7000) == UINT32_MAX, "last representable generation is admitted");
        lease.Revoke();
        check(!lease.Begin(7, 8000) && !lease.Heartbeat(7, 1, 8000), "generation exhaustion never wraps to stale token");
        std::printf("{\"success\":true,\"checks\":%u,\"game_launched\":false}\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
