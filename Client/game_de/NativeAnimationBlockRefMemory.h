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
        // Integer manager operations use all sixteen name bytes as current
        // row affinity, not as a C string. Native observations never issue a
        // CPU owner generation; only the enclosing runtime may stamp one.
        ClockResult ReadCounterIdentity(std::uint32_t, NativeAnimationBlockIdentity&) const noexcept;
        ClockResult ReadCounter(const NativeAnimationBlockIdentity&, std::uint16_t&) const noexcept;
        ClockResult AddCounter(const NativeAnimationBlockIdentity&, Receipt* = nullptr, Exchange = nullptr) const noexcept;
        // Addressed classic4D3FF0 is unconditional WORD decrement, including
        // zero toFFFF. It does not unload/delete or saturate the counter.
        ClockResult RemoveWithoutDelete(const NativeAnimationBlockIdentity&, Receipt* = nullptr, Exchange = nullptr) const noexcept;
        // Actual MTA AddRef is ushort ++, including its modulo-65536 result.
        // Loading is not a prerequisite: Request acquires its ref before load.
        // Exactly one conditional write is attempted. An entered failure must
        // stop its enclosing owner without replaying or undoing this operation.
        ClockResult Add(const NativeAnimationBlockIdentity&, Receipt* = nullptr, Exchange = nullptr) const noexcept;

    private:
        enum class IdentityPolicy
        {
            Named,
            Counter
        };
        ClockResult Adjust(const NativeAnimationBlockIdentity&, IdentityPolicy, int direction, Receipt*, Exchange) const noexcept;
        Binding     binding;
        CheckOwner  owner = nullptr;
    };
}
