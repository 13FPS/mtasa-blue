/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Independently expiring ownership of the existing MTA cursor policy
 *****************************************************************************/
#pragma once
#include <atomic>
#include <cstdint>
#include <limits>

namespace FrontendCursorLease
{
    inline constexpr std::uint32_t LifetimeMs = 500;
    enum class Reason : std::uint32_t
    {
        None,
        Expired,
        ClockRollback,
        FocusLost,
        Stopped
    };
    struct State
    {
        std::uint64_t generation = 0, renewals = 0, callbacks = 0, suppressed = 0, passthrough = 0, inFlight = 0;
        std::uint32_t ownerThread = 0, active = 0, revoked = 1, ended = 1, lifetimeMs = LifetimeMs, reason = 4;
    };

    class Policy
    {
    public:
        bool Begin(std::uint32_t thread, std::uint64_t now) noexcept
        {
            if (!thread || (owner.load() && owner.load() != thread) || !ended.load() || inFlight.load() || generation.load() == UINT64_MAX ||
                now > UINT64_MAX - LifetimeMs)
                return false;
            owner.store(thread);
            generation.fetch_add(1);
            renewals.store(0);
            callbacks.store(0);
            suppressed.store(0);
            passthrough.store(0);
            ownerObserved.store(now);
            reason.store(Reason::None);
            deadline.store(now + LifetimeMs);
            ended.store(false);
            return true;
        }
        bool Renew(std::uint32_t thread, std::uint64_t token, std::uint64_t now, bool focused) noexcept
        {
            if (!thread || thread != owner.load() || !token || token != generation.load() || ended.load())
                return false;
            if (now < ownerObserved.exchange(now) || now > UINT64_MAX - LifetimeMs)
            {
                Disarm(Reason::ClockRollback);
                return false;
            }
            if (!Fresh(now, focused))
                return false;
            auto previous = deadline.load();
            if (!previous || !deadline.compare_exchange_strong(previous, now + LifetimeMs))
                return false;
            renewals.fetch_add(1);
            return reason.load() == Reason::None && deadline.load() != 0;
        }
        void EnterCallback() noexcept
        {
            inFlight.fetch_add(1);
            callbacks.fetch_add(1);
        }
        void LeaveCallback() noexcept { inFlight.fetch_sub(1); }
        void RecordPassthrough() noexcept { passthrough.fetch_add(1); }
        bool Suppress(std::uint64_t now, bool focused, bool callerOwned) noexcept
        {
            // Different UE/overlay threads may sample time in either order.
            // Callbacks only observe the shared deadline; only the single
            // producer checks its own clock and may renew it.
            const bool blocked = callerOwned && Fresh(now, focused);
            (blocked ? suppressed : passthrough).fetch_add(1);
            return blocked;
        }
        void Disarm(Reason why = Reason::Stopped) noexcept
        {
            auto expected = Reason::None;
            reason.compare_exchange_strong(expected, why);
            deadline.exchange(0);
        }
        bool Complete(std::uint32_t thread) noexcept
        {
            if (!thread || thread != owner.load())
                return false;
            Disarm();
            if (inFlight.load())
                return false;
            // The hook owner calls this only after successful detach. A Busy
            // callback cannot turn a retained object into a reusable lease.
            ended.store(true);
            return true;
        }
        State Read() const noexcept
        {
            State result;
            result.generation = generation.load();
            result.ownerThread = owner.load();
            result.renewals = renewals.load();
            result.callbacks = callbacks.load();
            result.suppressed = suppressed.load();
            result.passthrough = passthrough.load();
            result.inFlight = inFlight.load();
            result.reason = static_cast<std::uint32_t>(reason.load());
            result.revoked = result.reason != 0 || !deadline.load();
            result.ended = ended.load();
            result.active = !result.ended && !result.revoked;
            return result;
        }

    private:
        bool Fresh(std::uint64_t now, bool focused) noexcept
        {
            if (ended.load() || reason.load() != Reason::None)
                return false;
            if (!focused)
            {
                Disarm(Reason::FocusLost);
                return false;
            }
            auto previous = deadline.load();
            for (;;)
            {
                if (!previous || reason.load() != Reason::None)
                    return false;
                if (now < previous)
                    return true;
                if (deadline.compare_exchange_strong(previous, 0))
                {
                    auto expected = Reason::None;
                    reason.compare_exchange_strong(expected, Reason::Expired);
                    return false;
                }
            }
        }
        std::atomic<std::uint32_t> owner{0};
        std::atomic<std::uint64_t> generation{0}, ownerObserved{0}, deadline{0};
        std::atomic<std::uint64_t> renewals{0}, callbacks{0}, suppressed{0}, passthrough{0}, inFlight{0};
        std::atomic<bool>          ended{true};
        std::atomic<Reason>        reason{Reason::Stopped};
    };
}
