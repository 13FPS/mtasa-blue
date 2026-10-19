/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read-only, exact-image time service independent of write trials
 *****************************************************************************/
#pragma once

#include <game/CGameTimeService.h>
#include <atomic>

namespace mta::game_de
{
    class NativeGameTimeReader final
    {
    public:
        Status Bind() noexcept;
        Status Read(GameTimeStateV1& output) const noexcept;
        void   Reset() noexcept;
        bool   IsBound() const noexcept;

    private:
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
