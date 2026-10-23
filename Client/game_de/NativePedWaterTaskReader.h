/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate primary2/type268 observation without task publication
 ****************************************************************************/
#pragma once
#include "NativePedWaterTaskAccess.h"
namespace mta::game_de
{
    class NativePedWaterTaskReader final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Ped = NativePedPoolReader::Value;
        NativePedWaterTaskReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const Ped&, bool&) const noexcept;
        // Only qualified null primary2 or a supported occupied task class can
        // produce a bool. Unknown task vtables refuse and preserve output;
        // physical flags, active child tasks and type254 are not substitutes.
        // All task values remain immediate internal observations, never leases.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
