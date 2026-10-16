/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exchange a generation-tagged script-owned control lease as values
 *****************************************************************************/
#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include "../core/DefinitiveGuiFrontendReference.h"

namespace deonline
{
    class FrontendControlMailbox
    {
    public:
        using Snapshot = DefinitiveGuiFrontendReference::ControlSnapshot;
        enum class ScriptAction
        {
            Acknowledge,
            Heartbeat,
            Revoke,
            Restore
        };

        bool PrepareAttempt(std::uint32_t scriptThread) noexcept
        {
            Guard guard(lock);
            if (!guard.acquired || !scriptThread || (owner && owner != scriptThread) || (values[Requested] && !values[Restored]))
                return false;
            owner = scriptThread;
            worker = 0;
            prepared = true;
            values = {};
            requestedAt = heartbeatAt = lastObservedAt = scriptObservedAt = workerObservedAt = 0;
            scriptClockObserved = workerClockObserved = false;
            return true;
        }

        bool Worker(std::uint32_t action, std::uint32_t workerThread, std::uint64_t now, std::uint32_t* output, std::uint32_t count) noexcept
        {
            using Action = DefinitiveGuiFrontendReference::ControlAction;
            if (!workerThread || !output || count != values.size() ||
                (action != static_cast<std::uint32_t>(Action::Request) && action != static_cast<std::uint32_t>(Action::Poll) &&
                 action != static_cast<std::uint32_t>(Action::InputDetached)))
                return false;
            Guard guard(lock);
            if (!guard.acquired || !prepared || workerThread == owner || (worker && worker != workerThread))
                return false;
            worker = workerThread;
            Observe(now, false);
            if (action == static_cast<std::uint32_t>(Action::Request))
            {
                if (values[Requested] || values[Detached] || values[Fault] || generation == UINT32_MAX)
                    return false;
                values[Generation] = ++generation;
                values[Requested] = 1;
                requestedAt = lastObservedAt;
            }
            else if (action == static_cast<std::uint32_t>(Action::InputDetached))
            {
                // Native input changes stay exclusively on the script thread.
                // The worker publishes GUI detachment even while GPU drain is
                // pending, so restoration never waits for another game frame.
                values[Detached] = values[Revoked] = 1;
                values[Fresh] = 0;
                if (!values[Requested])
                    values[Restored] = 1;  // Nothing was ever offered to the script.
            }
            std::copy(values.begin(), values.end(), output);
            return true;
        }

        bool Capture(std::uint32_t scriptThread, std::uint64_t now, Snapshot& output) noexcept
        {
            Guard guard(lock);
            if (!guard.acquired || !prepared || !scriptThread || scriptThread != owner)
                return false;
            Observe(now, true);
            output = values;
            return true;
        }

        bool Script(ScriptAction action, std::uint32_t scriptThread, std::uint32_t requestedGeneration, std::uint64_t now) noexcept
        {
            Guard guard(lock);
            if (!guard.acquired || !prepared || !scriptThread || scriptThread != owner || !requestedGeneration || requestedGeneration != values[Generation] ||
                !values[Requested] || values[Restored])
                return false;
            Observe(now, true);
            switch (action)
            {
                case ScriptAction::Acknowledge:
                    if (values[Acknowledged] || values[Revoked] || values[Detached] || values[Fault])
                        return false;
                    values[Acknowledged] = values[Fresh] = 1;
                    heartbeatAt = lastObservedAt;
                    return true;
                case ScriptAction::Heartbeat:
                    if (!values[Acknowledged] || !values[Fresh] || values[Revoked] || values[Detached] || values[Fault])
                        return false;
                    heartbeatAt = lastObservedAt;
                    return true;
                case ScriptAction::Revoke:
                    // Revoke is idempotent for cleanup that races the worker's
                    // own stop. It can never renew an expired generation.
                    values[Revoked] = 1;
                    values[Fresh] = 0;
                    return true;
                case ScriptAction::Restore:
                    // Even an unacknowledged request must be explicitly retired
                    // after GUI detachment. The script can prove a no-write
                    // restoration without claiming that acknowledgement occurred.
                    if (!values[Revoked] || !values[Detached])
                        return false;
                    values[Restored] = 1;
                    values[Fresh] = 0;
                    return true;
            }
            return false;
        }

        bool CaptureFinalOnWorker(std::uint32_t workerThread, std::uint64_t now, Snapshot& output) noexcept
        {
            if (!workerThread)
                return false;
            // Only the final post-InitializeCore ownership check may wait. At
            // this point native peer holds and GUI callbacks have ended; a
            // momentary script capture must not quarantine safely retired code.
            Guard guard(lock, true);
            if (!prepared || workerThread == owner || (worker && worker != workerThread))
                return false;
            worker = workerThread;
            Observe(now, false);
            output = values;
            return true;
        }

    private:
#ifdef MTA_TEST_FRONTEND_CONTROL_ACCESS
        friend struct FrontendControlMailboxTestAccess;
#endif
        using Fields = DefinitiveGuiFrontendReference::ControlField;
        static constexpr auto Generation = Fields::Generation, Requested = Fields::Requested, Acknowledged = Fields::Acknowledged;
        static constexpr auto Fresh = Fields::Fresh, Revoked = Fields::Revoked, Detached = Fields::Detached, Restored = Fields::Restored, Fault = Fields::Fault;
        struct Guard
        {
            SRWLOCK& lock;
            bool     acquired;
            explicit Guard(SRWLOCK& value, bool finalWorker = false) noexcept : lock(value), acquired(false)
            {
                if (finalWorker)
                {
                    AcquireSRWLockExclusive(&lock);
                    acquired = true;
                }
                else
                    acquired = TryAcquireSRWLockExclusive(&lock) != FALSE;
            }
            ~Guard()
            {
                if (acquired)
                    ReleaseSRWLockExclusive(&lock);
            }
        };
        void Observe(std::uint64_t now, bool script) noexcept
        {
            auto& actorTime = script ? scriptObservedAt : workerObservedAt;
            auto& actorObserved = script ? scriptClockObserved : workerClockObserved;
            if (actorObserved && now < actorTime)
                values[Fault] = values[Revoked] = 1;
            actorTime = now;
            actorObserved = true;
            // A caller samples time before taking this nonblocking lock. Another
            // thread may publish a later sample first; that ordering is not a
            // clock regression. Check each actor and age against their maximum.
            now = (std::max)(now, lastObservedAt);
            lastObservedAt = now;
            if (values[Requested] && !values[Restored] && !values[Revoked])
            {
                const auto timestamp = values[Acknowledged] ? heartbeatAt : requestedAt;
                const auto maximum =
                    values[Acknowledged] ? DefinitiveGuiFrontendReference::HeartbeatExpiryMs : DefinitiveGuiFrontendReference::ControlAcquireMs;
                if (now < timestamp || now - timestamp > maximum)
                    values[Revoked] = 1;
            }
            values[Fresh] = values[Acknowledged] && !values[Revoked] && !values[Detached] && !values[Restored] && !values[Fault] ? 1u : 0u;
        }
        SRWLOCK       lock = SRWLOCK_INIT;
        Snapshot      values{};
        std::uint32_t owner = 0, worker = 0, generation = 0;
        std::uint64_t requestedAt = 0, heartbeatAt = 0, lastObservedAt = 0, scriptObservedAt = 0, workerObservedAt = 0;
        bool          prepared = false, scriptClockObserved = false, workerClockObserved = false;
    };
}
