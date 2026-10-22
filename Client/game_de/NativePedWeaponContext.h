/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate identity for a selected ped-owned inline weapon slot
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include <cstddef>
namespace mta::game_de
{
    class NativePedWeaponContext final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Ped = NativePedPoolReader::Value;
        struct Value
        {
            Ped            ped{};
            std::uint32_t  slot = 0, rawType = 0;
            std::uintptr_t nativeWeapon = 0;
        };
        // One callback mutation scope is shared by type/state/ammo. Recursive
        // setters cannot treat a partially accepted slot change as new input.
        class Mutation final
        {
        public:
            Mutation() noexcept;
            ~Mutation();
            explicit operator bool() const noexcept { return acquired; }
            Mutation(const Mutation&) = delete;
            Mutation& operator=(const Mutation&) = delete;

        private:
            bool acquired = false;
        };
        NativePedWeaponContext(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        static bool Writable(std::uintptr_t, std::size_t) noexcept;
        ClockResult Capture(const Ped&, std::uint32_t slot, Value&) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;
        ClockResult Recheck(const Value&, std::uint32_t expectedType) const noexcept;
        // A snapshot never outlives the owner's borrowed callback. Neither a
        // matching raw type nor a selected slot is a native allocation lease.
        // An explicit expected type is used only after the caller's accepted
        // raw type CAS; it does not authorize initialization or model changes.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
