/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: MTA's immediate ushort animation-block reference operations
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockMemory.h"
namespace mta::game_de
{
    class NativeAnimationBlockRefMemory final
    {
    public:
        using Binding = NativeAnimationBlockMemory::Binding;
        using CheckOwner = NativeAnimationBlockMemory::CheckOwner;
        using Exchange = ClockResult (*)(std::uintptr_t, std::uint16_t, std::uint16_t, std::uint16_t&, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t address = 0;
            std::uint16_t  before = 0, target = 0, observed = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, accepted = false, verified = false;
        };
        NativeAnimationBlockRefMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const NativeAnimationBlockIdentity&, std::uint16_t&) const noexcept;
        // Actual MTA AddRef is ushort ++, including its modulo-65536 result.
        // Loading is not a prerequisite: Request acquires its ref before load.
        // Exactly one conditional write is attempted. An entered failure must
        // stop its enclosing owner without replaying or undoing this operation.
        ClockResult Add(const NativeAnimationBlockIdentity&, Receipt* = nullptr, Exchange = nullptr) const noexcept;

    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
