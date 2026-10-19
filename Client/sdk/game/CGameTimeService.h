/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional read-only native time needed by DE startup consumers
 *****************************************************************************/
#pragma once

#include "CGameAdapter.h"
#include <type_traits>

namespace mta::game_de
{
    inline constexpr std::uint32_t GameTimeServiceVersionV1 = 1;

    // These are directly sampled native values, not a wall clock, an emulated
    // CGame or evidence that every gameplay subsystem is available. The reader
    // accepts two matching complete samples; it does not claim an engine-thread
    // atomic snapshot or retain an earlier successful sample after a refusal.
    struct GameTimeStateV1
    {
        std::uint32_t size = sizeof(GameTimeStateV1), version = GameTimeServiceVersionV1;
        std::uint32_t producerThread = 0;
        std::uint32_t hour = 0, minute = 0, seconds = 0;
        std::uint32_t nativeTimer = 0, lastMinuteChange = 0, millisecondsPerGameMinute = 0;
        std::uint32_t reserved[3]{};
    };
    static_assert(std::is_standard_layout_v<GameTimeStateV1> && std::is_trivially_copyable_v<GameTimeStateV1>);
    static_assert(sizeof(GameTimeStateV1) == 48);

    // Kept outside IGameAdapter so ABI6 and the incomplete CGame capability do
    // not change. Reads install no hook and need no short-lived write lease.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_GAME_TIME_EXPORT __declspec(dllexport)
#else
    #define MTA_GAME_TIME_EXPORT
#endif
    extern "C" MTA_GAME_TIME_EXPORT Status ReadGameTimeStateV1(std::uint32_t version, IGameAdapter* adapter, GameTimeStateV1* output) noexcept;
#undef MTA_GAME_TIME_EXPORT
}
