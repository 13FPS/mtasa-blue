/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Independent script-owned lifetime for persistent Core input
 *****************************************************************************/
#pragma once
#include <atomic>
#include <cstdint>
#include <limits>

namespace deonline
{
    class PersistentFrontendHostLease
    {
    public:
        static constexpr std::uint64_t BudgetMs = 1000;

        std::uint32_t Begin(std::uint32_t thread, std::uint64_t now) noexcept
        {
            // Only the established script owner can issue a new generation.
            // Consuming the final token permanently refuses further attempts,
            // rather than admitting a stale callback after integer wraparound.
            if (!thread || (owner && owner != thread) || generation == UINT32_MAX || now > UINT64_MAX - BudgetMs)
                return 0;
            owner = thread;
            lastOwnerAt = now;
            const auto next = ++generation;
            activeGeneration.store(next);
            consumerObserved.store(now);
            deadline.store(now + BudgetMs);
            return next;
        }

        bool Heartbeat(std::uint32_t thread, std::uint32_t token, std::uint64_t now) noexcept
        {
            if (!thread || thread != owner || !token || token != activeGeneration.load())
                return false;
            if (now < lastOwnerAt || now > UINT64_MAX - BudgetMs)
            {
                Revoke();
                return false;
            }
            lastOwnerAt = now;
            auto previous = deadline.load();
            if (!previous || now >= previous)
            {
                // Expiry is irrevocable even if the script resumes later.
                deadline.compare_exchange_strong(previous, 0);
                return false;
            }
            return deadline.compare_exchange_strong(previous, now + BudgetMs);
        }

        bool Fresh(std::uint32_t token, std::uint64_t now) noexcept
        {
            if (!token || token != activeGeneration.load())
                return false;
            if (now < consumerObserved.exchange(now))
            {
                Revoke();
                return false;
            }
            auto previous = deadline.load();
            for (;;)
            {
                if (!previous)
                    return false;
                // Host and worker clocks are monotone separately. A concurrent
                // heartbeat may have sampled a later instant than this worker,
                // which must not manufacture rollback or expire a fresh lease.
                if (now < previous)
                    return true;
                // An earlier valid host renewal can win this CAS even though
                // this observer sampled expiry. Recheck its new deadline rather
                // than returning a false stop while that renewal is live.
                if (deadline.compare_exchange_strong(previous, 0))
                    return false;
            }
        }

        void          Revoke() noexcept { deadline.store(0); }
        std::uint32_t Generation() const noexcept { return activeGeneration.load(); }

    private:
#ifdef MTA_TEST_PERSISTENT_FRONTEND_HOST_ACCESS
        friend struct PersistentFrontendHostLeaseTestAccess;
#endif
        std::uint32_t              owner = 0, generation = 0;
        std::uint64_t              lastOwnerAt = 0;
        std::atomic<std::uint32_t> activeGeneration{0};
        std::atomic<std::uint64_t> deadline{0}, consumerObserved{0};
    };
}
