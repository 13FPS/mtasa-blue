/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Apply MTA's existing model-loading policy with a pause-safe DE clock
 *****************************************************************************/
#pragma once

#include <CAsyncLoadingPolicy.h>
#include <cstdint>

namespace mta::game_de
{
    class CAsyncLoadingDE final
    {
    public:
        using Clock = std::int64_t (*)() noexcept;
        explicit CAsyncLoadingDE(Clock now = MonotonicTime) noexcept : clock(now ? now : MonotonicTime) {}

        void SetScriptOverride(bool enabled, bool forced) noexcept { policy.SetScriptOverride(enabled, forced); }
        void Suspend(bool suspended, std::uint32_t delayMs = 0);
        bool IsEnabled(bool ignoreSuspend = false) noexcept;

    private:
        static std::int64_t          MonotonicTime() noexcept;
        MtaGame::CAsyncLoadingPolicy policy;
        Clock                        clock;
    };
}
