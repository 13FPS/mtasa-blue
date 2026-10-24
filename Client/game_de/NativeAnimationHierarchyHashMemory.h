/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current native hierarchy hash observation for MTA's gateway check
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockMemory.h"
namespace mta::game_de
{
    class NativeAnimationHierarchyHashMemory final
    {
    public:
        using Binding = NativeAnimationBlockMemory::Binding;
        using CheckOwner = NativeAnimationBlockMemory::CheckOwner;
        NativeAnimationHierarchyHashMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        // A raw tag supplies only a fresh current bank locator. Each read
        // proves row40/highwater/current block range/owner before output;
        // it never adopts private/custom storage or grants a hierarchy lease.
        ClockResult Read(std::uintptr_t locator, std::uint32_t& output) const noexcept;

    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
