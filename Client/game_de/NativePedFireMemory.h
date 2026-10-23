/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe only current ped's associated-fire pointer presence
 ****************************************************************************/
#pragma once
#include "NativePedFireAccess.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedFireMemory final
    {
    public:
        static_assert(sizeof(std::uintptr_t) == 8, "The qualified native ped association is a QWORD in the x64 executable");
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        NativePedFireMemory(Binding value, CheckOwner callback) noexcept : binding(value), owner(callback) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadOnFire(const Value&, bool&) const noexcept;
        // The const CPed SDK predicate is pointer presence, not an actor/task
        // flag. Never expose or dereference the fire pointer, create/extinguish
        // a fire, or substitute the current player's association for this ped.
    private:
        Binding    binding;
        CheckOwner owner;
    };
}
