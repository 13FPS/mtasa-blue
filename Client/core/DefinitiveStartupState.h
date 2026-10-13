/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the real DE Core startup attempt for its owning caller
 *****************************************************************************/

#pragma once

#include <cstdint>

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include <atomic>
    #include <windows.h>
#endif

namespace DefinitiveStartup
{
    enum class Phase : std::uint32_t
    {
        None,
        Entry,
        DirectoriesReady,
        BackendStarted,
        ExactImageBound,
        PulseComplete,
        GameplayChecked,
        FullCoreReady,
    };

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    constexpr std::uint32_t Unavailable = 0xffffffffu;
    // A failed constructor destroys its members before InitializeCore returns.
    // Keep only values here, so the owner can inspect that attempt without a
    // dangling Core pointer or confusing a pinned DLL with a live adapter.
    inline std::atomic<std::uint32_t> fields[8]{{1}, {Unavailable}, {0}, {0}, {0}, {0}, {0}, {1}};
    inline std::atomic<DWORD>         ownerThread{0};

    inline void BeginAttempt() noexcept
    {
        ownerThread.store(GetCurrentThreadId());
        for (auto& field : fields)
            field.store(0);
        fields[0].store(1);
        fields[1].store(Unavailable);
        fields[2].store(static_cast<std::uint32_t>(Phase::Entry));
        fields[7].store(1);  // No adapter has been started by this attempt.
    }

    inline void SetPhase(Phase phase) noexcept
    {
        fields[2].store(static_cast<std::uint32_t>(phase));
    }
    inline void SetCapabilities(std::uint64_t capabilities) noexcept
    {
        fields[3].store(static_cast<std::uint32_t>(capabilities));
        fields[4].store(static_cast<std::uint32_t>(capabilities >> 32));
    }
    inline void SetGameplayReady(bool ready) noexcept
    {
        fields[5].store(ready ? 1u : 0u);
    }
    inline void SetCleanupComplete(bool complete) noexcept
    {
        fields[7].store(complete ? 1u : 0u);
    }
    inline int CompleteResult(int result) noexcept
    {
        fields[1].store(static_cast<std::uint32_t>(result));
        if (result == 0)
        {
            fields[6].store(1);
            SetPhase(Phase::FullCoreReady);
        }
        return result;
    }
    inline std::uint32_t Read(std::uint32_t field) noexcept
    {
        // The export is a diagnostic readback, not a cross-thread lifecycle API.
        // Atomics also make a rejected foreign read race-free during an attempt.
        if (field >= 8 || ownerThread.load() != GetCurrentThreadId())
            return Unavailable;
        return fields[field].load();
    }
#else
    // Classic startup retains the same behavior and exports; these calls are
    // optimized away instead of creating a second legacy initialization policy.
    inline void BeginAttempt() noexcept
    {
    }
    inline void SetPhase(Phase) noexcept
    {
    }
    inline void SetCapabilities(std::uint64_t) noexcept
    {
    }
    inline void SetGameplayReady(bool) noexcept
    {
    }
    inline void SetCleanupComplete(bool) noexcept
    {
    }
    inline int CompleteResult(int result) noexcept
    {
        return result;
    }
#endif
}
