/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share actual MTA streaming request and wrapper-reference sequencing
 ****************************************************************************/
#pragma once
#include <cstddef>
#include "CAnimBlock.h"
#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace MtaGame
{
    // These algorithms own only the existing wrapper's CPU counters. Native
    // loaded state, streaming, interface reference increments and removal are
    // mandatory caller operations: no cached/fallback native state is supplied.
    // Exceptions propagate at their original boundary, preserving preceding
    // effects and the classic ordering of subsequent counter changes.
    class CModelRequestPolicy final
    {
    public:
        static constexpr bool IsUpgradeModel(std::uint32_t model) noexcept
        {
            // Both classic RequestModel and HasModelLoaded use their distinct
            // upgrade routines for this exact inclusive SDK model interval.
            return model >= 1000 && model <= 1193;
        }

        template <class IsLoaded, class Dependencies, class RequestNative, class LoadNative, class Report>
        static void Request(EModelRequestType type, const char* tag, IsLoaded&& isLoaded, Dependencies&& dependencies, RequestNative&& requestNative,
                            LoadNative&& loadNative, Report&& report)
        {
            if (isLoaded())
                return;
            // Existing vehicle-animation/skin dependencies belong to the real
            // model implementation and run before either streaming request.
            dependencies(type, tag);
            if (type == BLOCKING)
            {
                requestNative(std::uint32_t{0x16});
                loadNative(true, tag);
                if (!isLoaded())
                {
                    int attempts = 0;
                    // Preserve all ten retry opportunities, priority changes,
                    // short-circuit query calls and postincrement reporting.
                    // A fully exhausted loop reports 11, as classic does.
                    while (attempts++ < 10 && !isLoaded())
                    {
                        const bool onlyPriority = (attempts < 3 || attempts & 1);
                        loadNative(onlyPriority, tag);
                    }
                    report(isLoaded(), attempts, tag);
                }
            }
            else
                requestNative(std::uint32_t{0x06});
        }

        template <class Counter, class Pending, class DoIsLoaded, class IncrementNative>
        static bool IsLoaded(Counter& references, Pending& pending, DoIsLoaded&& doIsLoaded, IncrementNative&& incrementNative)
        {
            CheckCounters<Counter, Pending>();
            static_cast<void>(references);  // The classic invariant is assert-only in release builds.
            if (!doIsLoaded())
                return false;
            if (pending)
            {
                assert(references > 0);
                // Refresh/increment the genuine interface first. The pending
                // acknowledgement is cleared only after that effect returns.
                incrementNative();
                pending = 0;
            }
            return true;
        }

        template <class Counter, class Pending, class IsLoaded, class AsyncEnabled, class Request, class IncrementNative>
        static void AddRef(Counter& references, Pending& pending, EModelRequestType type, const char* tag, IsLoaded&& isLoaded, AsyncEnabled&& asyncEnabled,
                           Request&& request, IncrementNative&& incrementNative)
        {
            CheckCounters<Counter, Pending>();
            if (!isLoaded())
            {
                if (asyncEnabled())
                    request(type, tag);
                else
                    request(BLOCKING, tag);
            }
            if (references == 0)
            {
                assert(!pending);
                if (isLoaded())
                    incrementNative();
                else
                    pending = 1;
            }
            // One native reference backs any positive wrapper count. Retain
            // the actual unsigned DWORD increment, including its wrap behavior.
            ++references;
        }

        template <class Counter, class Pending, class IsLoaded, class RemoveNative>
        static void RemoveRef(Counter& references, Pending& pending, std::uint32_t model, IsLoaded&& isLoaded, RemoveNative&& removeNative)
        {
            CheckCounters<Counter, Pending>();
            if (references > 0)
                --references;
            if (references == 0 && pending)
            {
                // Cancellation of an unfulfilled native reference precedes
                // loaded-state observation and does not issue native removal.
                pending = 0;
                return;
            }
            if (references == 0 && model != 0 && isLoaded())
                removeNative();
        }

    private:
        template <class Counter, class Pending>
        static constexpr void CheckCounters() noexcept
        {
            static_assert(std::is_unsigned_v<Counter> && std::numeric_limits<Counter>::digits == 32);
            static_assert(std::is_unsigned_v<Pending> && std::numeric_limits<Pending>::digits == 32);
        }
    };
}
