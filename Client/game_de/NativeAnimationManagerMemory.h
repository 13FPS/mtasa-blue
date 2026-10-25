/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate current native counts and bounded first-match name lookup
 ****************************************************************************/
#pragma once
#include "NativeAnimationManagerAccess.h"
namespace mta::game_de
{
    class NativeAnimationManagerMemory
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        NativeAnimationManagerMemory(Binding value, CheckOwner callback) noexcept : binding(value), owner(callback) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadCounts(NativeAnimationManagerCounts&) const noexcept;
        // Separate source observations preserve the old count contract and do
        // not couple ordinary block/animation reads to definition metadata.
        ClockResult ReadDefinitionCount(std::int32_t&) const noexcept;
        ClockResult ReadGroupName(std::uint32_t, std::uintptr_t&) const noexcept;
        ClockResult ReadGroupBlockName(std::uint32_t, std::uintptr_t&) const noexcept;
        ClockResult FindFirstGroupByBlockName(const std::string&, std::uint32_t&) const noexcept;
        // Read only the current native array's source DWORD Created value;
        // a copied bool grants no group, child-array or release ownership.
        ClockResult ReadGroupCreated(std::uint32_t, bool&) const noexcept;
        ClockResult ReadStaticAssociationAnimID(std::uint32_t, std::uint32_t, std::int16_t&) const noexcept;
        ClockResult FindBlockIndex(const NativeAnimationBlockName&, std::int32_t&) const noexcept;

    private:
        Binding    binding;
        CheckOwner owner;
    };
}
