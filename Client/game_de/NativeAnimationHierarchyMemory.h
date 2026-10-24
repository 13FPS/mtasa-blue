/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow current native-bank hierarchy fields and sequence addresses
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockMemory.h"
#include "NativeAnimationHierarchyAccess.h"
namespace mta::game_de
{
    class NativeAnimationHierarchyMemory final
    {
    public:
        using Binding = NativeAnimationBlockMemory::Binding;
        using CheckOwner = NativeAnimationBlockMemory::CheckOwner;
        NativeAnimationHierarchyMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        // A locator only requests a fresh current native-bank observation.
        // Each operation reacquires row/block ownership; none issues a custom
        // allocation, a sequence lease or permission to retire native children.
        ClockResult Read(std::uintptr_t locator, NativeAnimationHierarchyValues& output) const noexcept;
        ClockResult ReadSequences(std::uintptr_t locator, std::uintptr_t& output) const noexcept;
        ClockResult ReadSequence(std::uintptr_t locator, std::uint32_t index, std::uintptr_t& output) const noexcept;

    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
