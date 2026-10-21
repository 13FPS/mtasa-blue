/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified DE native stat observations without native engine calls
 *****************************************************************************/
#pragma once

// The actual weapon SDK declares Windows DWORDs; native backend units do not
// inherit the classic PCH that normally supplies those types.
#include <windows.h>
#include <game/CGameAdapter.h>
#include <game/CWeaponInfo.h>
#include <atomic>

namespace mta::game_de
{
    class NativeStatsReader final
    {
    public:
        Status Bind() noexcept;
        Status Read(unsigned short index, float& output) const noexcept;
        Status ReadSkillIndex(eWeaponType weapon, unsigned short& output) const noexcept;
        void   Reset() noexcept;
        bool   IsBound() const noexcept;

    private:
        Status                      CurrentImage(std::uintptr_t& output) const noexcept;
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
