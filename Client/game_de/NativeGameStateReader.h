/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked native startup-state access for the actual CGame contract
 *****************************************************************************/
#pragma once

#include <game/CGameAdapter.h>
#include <enums/SystemState.h>
#include <atomic>

namespace mta::game_de
{
    class NativeGameStateReader final
    {
    public:
        Status Bind() noexcept;
        Status Read(SystemState::Enum& output) const noexcept;
        Status ReadMenu(bool& output) const noexcept;
        void   Reset() noexcept;
        bool   IsBound() const noexcept;

    private:
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
