/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA async-loading rules without depending on native game time
 *****************************************************************************/
#include "CAsyncLoadingDE.h"

#include <chrono>
#include <limits>
#include <stdexcept>

namespace mta::game_de
{
    std::int64_t CAsyncLoadingDE::MonotonicTime() noexcept
    {
        // Loading and paused frontends can stop the native game timer. MTA's
        // temporary loading suspension must still expire in elapsed real time.
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void CAsyncLoadingDE::Suspend(bool suspended, std::uint32_t delayMs)
    {
        std::int64_t deadline = 0;
        if (suspended && delayMs)
        {
            const auto now = clock();
            // Preserve the previous policy if an invalid clock cannot express
            // this deadline, rather than wrapping it into immediate expiry.
            if (now > std::numeric_limits<std::int64_t>::max() - delayMs)
                throw std::overflow_error("DE async-loading deadline overflow");
            deadline = now + delayMs;
        }
        policy.Suspend(suspended, deadline);
    }

    bool CAsyncLoadingDE::IsEnabled(bool ignoreSuspend) noexcept
    {
        return policy.IsEnabled(ignoreSuspend, policy.HasAutoUnsuspendDeadline() ? clock() : 0);
    }
}
