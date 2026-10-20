/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's asynchronous model-loading policy across game backends
 *****************************************************************************/
#pragma once

#include <cstdint>

namespace MtaGame
{
    // This is MTA-owned scheduling policy, not native streaming state. Keep it
    // separate from each engine's model requests so both backends retain the
    // script override and temporary suspension behavior used by client startup.
    class CAsyncLoadingPolicy
    {
    public:
        void SetScriptOverride(bool enabled, bool forced) noexcept
        {
            scriptEnabled = enabled;
            scriptForced = forced;
        }

        void Suspend(bool value, std::int64_t absoluteDeadlineMs = 0) noexcept
        {
            suspended = value;
            // The caller keeps its existing CTickCount addition and clock
            // domain. Zero means indefinite; clearing suspension cancels an
            // earlier deadline rather than letting it affect a later request.
            autoUnsuspendAt = value ? absoluteDeadlineMs : 0;
        }

        bool HasAutoUnsuspendDeadline() const noexcept { return autoUnsuspendAt != 0; }

        bool IsEnabled(bool ignoreSuspend = false, std::int64_t nowMs = 0) noexcept
        {
            // Expiry is processed even for an ignore-suspension query. The
            // original policy uses strict >, so equality remains suspended.
            if (autoUnsuspendAt != 0 && nowMs > autoUnsuspendAt)
            {
                autoUnsuspendAt = 0;
                suspended = false;
            }
            if (suspended && !ignoreSuspend)
                return false;
            return scriptForced ? scriptEnabled : true;
        }

    private:
        bool         scriptEnabled = false;
        bool         scriptForced = false;
        bool         suspended = false;
        std::int64_t autoUnsuspendAt = 0;
    };
}
