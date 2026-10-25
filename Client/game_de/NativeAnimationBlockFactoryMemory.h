/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh native animation-block identities and opaque hierarchy borrows
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockMemory.h"
#include <cstddef>
namespace mta::game_de
{
    class NativeAnimationBlockFactoryMemory final
    {
    public:
        using Binding = NativeAnimationBlockMemory::Binding;
        using CheckOwner = NativeAnimationBlockMemory::CheckOwner;
        NativeAnimationBlockFactoryMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        // Native observations never issue CPU affinity. The runtime may stamp
        // its current generation only after its own before/after owner checks.
        ClockResult ReadIdentity(std::uint32_t, NativeAnimationBlockIdentity&) const noexcept;
        ClockResult ResolveInterface(std::uintptr_t, NativeAnimationBlockIdentity&) const noexcept;
        // The address is a borrowed opaque native tag, not a classic layout or
        // a hierarchy/sequence lease. Unloaded returns null before index checks.
        ClockResult ReadHierarchy(const NativeAnimationBlockIdentity&, std::size_t, std::uintptr_t&) const noexcept;
        // Actual4D39F0 searches hashes in the block's current hierarchy range.
        // It has no loaded/ordinal shortcut; a complete qualified miss is null.
        ClockResult FindHierarchyByHash(const NativeAnimationBlockIdentity&, std::uint32_t, std::uintptr_t&) const noexcept;

    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
