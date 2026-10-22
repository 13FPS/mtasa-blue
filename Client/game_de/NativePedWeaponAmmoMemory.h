/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw MTA ammo intent over a current qualified inline native weapon
 ****************************************************************************/
#pragma once
#include "NativePedWeaponAmmoAccess.h"
#include "NativePedWeaponContext.h"
namespace mta::game_de
{
    class NativePedWeaponAmmoMemory final
    {
    public:
        using Binding = NativePedWeaponContext::Binding;
        using CheckOwner = NativePedWeaponContext::CheckOwner;
        using Ped = NativePedWeaponContext::Ped;
        struct Receipt
        {
            std::uint32_t  handle = 0, slot = 0, rawType = 0, original = 0, target = 0, observed = 0, exceptionCode = 0;
            std::uintptr_t nativePed = 0, nativeWeapon = 0, address = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false;
        };
        NativePedWeaponAmmoMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadAmmoInClip(const Ped&, std::uint32_t slot, std::uint32_t&) const noexcept;
        ClockResult SetAmmoInClip(const Ped&, std::uint32_t slot, std::uint32_t, Receipt* = nullptr) const noexcept;
        ClockResult ReadAmmoTotal(const Ped&, std::uint32_t slot, std::uint32_t&) const noexcept;
        ClockResult SetAmmoTotal(const Ped&, std::uint32_t slot, std::uint32_t, Receipt* = nullptr) const noexcept;
        // Immediate inline-slot operations never publish a native CWeapon
        // lease. A setter owns one complete DWORD only; clip/total/state and
        // native script clamping remain separate. Entered failure Stop/no replay.
    private:
        ClockResult Read(const Ped&, std::uint32_t, std::size_t, std::uint32_t&) const noexcept;
        ClockResult Set(const Ped&, std::uint32_t, std::size_t, std::uint32_t, Receipt*) const noexcept;
        Binding     binding;
        CheckOwner  owner = nullptr;
    };
}
