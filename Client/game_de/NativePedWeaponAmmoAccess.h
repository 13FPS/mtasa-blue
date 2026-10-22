/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Selected registered-ped weapon ammo with no standalone lifetime claim
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedWeaponAmmoAccess
    {
    public:
        virtual ClockResult ReadPedWeaponAmmoInClip(const NativePedReferenceToken&, std::uint32_t, std::uint32_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPedWeaponAmmoInClip(const NativePedReferenceToken&, std::uint32_t, std::uint32_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ReadPedWeaponAmmoTotal(const NativePedReferenceToken&, std::uint32_t, std::uint32_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPedWeaponAmmoTotal(const NativePedReferenceToken&, std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedWeaponAmmoAccess() = default;
    };
}
