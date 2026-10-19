/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked native timing used by real CGame timing overrides
 *****************************************************************************/
#pragma once

#include "NativeGameTimeReader.h"
#include <type_traits>

namespace mta::game_de
{
    // Internal backend storage only: this is neither a new DLL ABI nor a
    // replacement CGame. Matching samples are observational coherence, not an
    // assertion that the producer owns an atomic engine update.
    struct NativeGameTimingValues
    {
        GameTimeStateV1 clock;
        std::uint32_t   frameCounter = 0;
        float           timeStep = 0, oldTimeStep = 0, timeScale = 0;
    };
    static_assert(sizeof(NativeGameTimingValues) == 64 && std::is_standard_layout_v<NativeGameTimingValues> &&
                  std::is_trivially_copyable_v<NativeGameTimingValues>);

    class NativeGameTimingReader final
    {
    public:
        Status Bind() noexcept;
        Status Read(NativeGameTimingValues& output) const noexcept;
        Status ReadFPS(float& output) const noexcept;
        void   Reset() noexcept;
        bool   IsBound() const noexcept;

    private:
        NativeGameTimeReader        clock;
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
