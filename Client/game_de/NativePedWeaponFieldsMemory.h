/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA raw CWeapon scalar storage without initializing weapons
 ****************************************************************************/
#pragma once
#include "NativePedWeaponFieldsAccess.h"
#include "NativePedWeaponContext.h"
namespace mta::game_de
{
    class NativePedWeaponFieldsMemory final
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
        NativePedWeaponFieldsMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding& output) noexcept { return NativePedWeaponContext::Qualify(output); }
        static bool Revalidate(const Binding& binding) noexcept { return NativePedWeaponContext::Revalidate(binding); }
        ClockResult ReadType(const Ped&, std::uint32_t slot, std::uint32_t&) const noexcept;
        ClockResult SetType(const Ped&, std::uint32_t slot, std::uint32_t, Receipt* = nullptr) const noexcept;
        ClockResult ReadState(const Ped&, std::uint32_t slot, std::uint32_t&) const noexcept;
        ClockResult SetState(const Ped&, std::uint32_t slot, std::uint32_t, Receipt* = nullptr) const noexcept;
        // Classic setters assign the complete enum storage word. They neither
        // call GiveWeapon nor reset ammo, models, FX or timers. Preserve that
        // narrow intent and every other byte, without inventing a value clamp.
        // One conditional DWORD write may have a persistent effect even when
        // acknowledgement/owner validation fails. The owner must Stop on an
        // entered failure; no replay, rollback or native lifetime is provided.
    private:
        ClockResult Read(const Ped&, std::uint32_t, std::size_t, std::uint32_t&) const noexcept;
        ClockResult Set(const Ped&, std::uint32_t, std::size_t, std::uint32_t, Receipt*) const noexcept;
        Binding     binding;
        CheckOwner  owner = nullptr;
    };
}
