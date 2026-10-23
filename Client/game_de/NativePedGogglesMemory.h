/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read only the genuine goggles association presence, never its target
 ****************************************************************************/
#pragma once
#include "NativePedGogglesAccess.h"
namespace mta::game_de
{
    class NativePedGogglesMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        NativePedGogglesMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadWearingGoggles(const Value&, bool&) const noexcept;
        // Actual MTA asks only whether its native goggles association is
        // nonnull. Copy the inline pointer bits without dereferencing or
        // exposing the actor. No object lease, setter, task, vision-state
        // substitution or goggles allocation/retirement is implied.
        // A refused observation always preserves the caller's prior bool.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
