/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked native file-ID partitions for actual MTA CGame getters
 *****************************************************************************/
#pragma once

#include <game/CGameAdapter.h>
#include <atomic>

namespace mta::game_de
{
    // This contains SDK scalar values only. DE's native streaming rows have a
    // different stride from CStreamingInfo and must never escape through it.
    struct NativeStreamingFileIds
    {
        std::uint32_t dff = 0, txd = 0, col = 0, ipl = 0, dat = 0, ifp = 0, rrr = 0, scm = 0, count = 0;
    };
    static_assert(sizeof(NativeStreamingFileIds) == 36);

    class NativeStreamingFileIdsReader final
    {
    public:
        Status Bind() noexcept;
        Status Read(NativeStreamingFileIds& output) const noexcept;
        void   Reset() noexcept;
        bool   IsBound() const noexcept;

    private:
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
