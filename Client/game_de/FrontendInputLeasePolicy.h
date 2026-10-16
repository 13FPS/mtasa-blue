/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Irreversible short neutral-input lease shared by producer/callback
 *****************************************************************************/
#pragma once
#include "../sdk/game/CGameFrontendInputLease.h"
#include <atomic>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    class FrontendInputLeasePolicy final
    {
    public:
        static bool Neutral(const MtaInputState& state) noexcept
        {
            const MtaInputState zero;
            return std::memcmp(&state, &zero, sizeof(state)) == 0;
        }

        // Begin is allowed only after the caller proves callback quiescence.
        // A new generation may replace a stopped lease, never resurrect it.
        bool Begin(std::uint64_t nextGeneration, std::uint32_t thread, std::uint64_t now, bool focused, const MtaInputState& state) noexcept
        {
            if (!nextGeneration || nextGeneration <= generation.load() || !thread || !focused || !Neutral(state) || !Revoked() ||
                now > (std::numeric_limits<std::uint64_t>::max)() - FrontendInputLeaseLifetimeMsV1)
                return false;
            generation.store(nextGeneration);
            producer.store(thread);
            refreshed.store(now);
            producerObserved.store(now);
            consumerObserved.store(now);
            renewals.store(0);
            reason.store(FrontendInputLeaseReason::None, std::memory_order_release);
            deadline.store(now + FrontendInputLeaseLifetimeMsV1, std::memory_order_release);
            return true;
        }

        bool Renew(std::uint64_t requested, std::uint32_t thread, std::uint64_t now, bool focused, const MtaInputState& state) noexcept
        {
            // A stale token or foreign caller cannot renew or revoke the live
            // owner. Invalid values from that owner permanently revoke it.
            if (!requested || requested != generation.load() || thread != producer.load() || Revoked())
                return false;
            if (!Neutral(state))
            {
                Revoke(FrontendInputLeaseReason::NonNeutral);
                return false;
            }
            if (!Check(now, focused, true) || now > (std::numeric_limits<std::uint64_t>::max)() - FrontendInputLeaseLifetimeMsV1)
                return false;
            auto expires = deadline.load(std::memory_order_acquire);
            do
            {
                if (!expires || Revoked())
                    return false;
            } while (!deadline.compare_exchange_weak(expires, now + FrontendInputLeaseLifetimeMsV1, std::memory_order_acq_rel));
            refreshed.store(now, std::memory_order_release);
            renewals.fetch_add(1);
            // Stop races publication without ever allowing renewal to clear
            // its irreversible reason. A reported success cannot reactivate it.
            return !Revoked();
        }

        bool Check(std::uint64_t now, bool focused, bool producerActor = false) noexcept
        {
            if (Revoked())
                return false;
            auto& observed = producerActor ? producerObserved : consumerObserved;
            if (now < observed.exchange(now))
                Revoke(FrontendInputLeaseReason::ClockRollback);
            else if (!focused)
                Revoke(FrontendInputLeaseReason::FocusLost);
            else
            {
                const auto expires = deadline.load(std::memory_order_acquire);
                // The other actor may publish a later clock sample first.
                // Actor-local rollback is checked above; cross-thread sample
                // ordering must not manufacture a false clock regression.
                return ObserveDeadline(now, expires);
            }
            return !Revoked();
        }

        void Revoke(FrontendInputLeaseReason failure) noexcept
        {
            auto expected = FrontendInputLeaseReason::None;
            reason.compare_exchange_strong(expected, failure, std::memory_order_acq_rel);
            deadline.exchange(0, std::memory_order_acq_rel);
        }
        bool Revoked() const noexcept
        {
            return reason.load(std::memory_order_acquire) != FrontendInputLeaseReason::None || !deadline.load(std::memory_order_acquire);
        }
        FrontendInputLeaseStateV1 Read() const noexcept
        {
            FrontendInputLeaseStateV1 state;
            state.generation = generation.load();
            state.producerThread = producer.load();
            state.reason = static_cast<std::uint32_t>(reason.load(std::memory_order_acquire));
            // Expiration wins the deadline CAS before publishing its reason.
            // A concurrent read must not report an active lease during that
            // short interval; the eventual final snapshot retains its reason.
            if (!deadline.load(std::memory_order_acquire) && !state.reason)
                state.reason = static_cast<std::uint32_t>(FrontendInputLeaseReason::NativeFailure);
            state.revoked = state.reason != 0;
            state.active = state.generation && !state.revoked;
            state.refreshedAtMs = refreshed.load(std::memory_order_acquire);
            state.renewals = renewals.load();
            return state;
        }

    private:
#ifdef MTA_TEST_FRONTEND_INPUT_LEASE_ACCESS
        friend struct FrontendInputLeasePolicyTestAccess;
#endif
        bool ObserveDeadline(std::uint64_t now, std::uint64_t expires) noexcept
        {
            while (expires && now >= expires)
            {
                // Renewal and expiry arbitrate the same atomic deadline. An
                // old callback observation cannot revoke a timely renewal
                // which won publication before this compare/exchange.
                if (deadline.compare_exchange_weak(expires, 0, std::memory_order_acq_rel))
                {
                    auto expected = FrontendInputLeaseReason::None;
                    reason.compare_exchange_strong(expected, FrontendInputLeaseReason::Expired, std::memory_order_acq_rel);
                    return false;
                }
            }
            return !Revoked();
        }
        std::atomic<std::uint64_t>            deadline{0};
        std::atomic<std::uint64_t>            generation{0}, refreshed{0}, producerObserved{0}, consumerObserved{0}, renewals{0};
        std::atomic<std::uint32_t>            producer{0};
        std::atomic<FrontendInputLeaseReason> reason{FrontendInputLeaseReason::Stopped};
    };
}
