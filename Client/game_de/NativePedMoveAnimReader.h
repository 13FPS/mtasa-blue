/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate raw movement-group observation over the occupied ped identity
 ****************************************************************************/
#pragma once
#include "NativePedMoveAnimAccess.h"
namespace mta::game_de
{
    class NativePedMoveAnimReader final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        NativePedMoveAnimReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const Value&, std::int32_t&) const noexcept;

        // The runtime resolves its copied registered token in the current
        // callback. This reader independently bookends that same occupied
        // ped, native GT/GC/game state and raw DWORD; it creates no lifetime
        // lease, block reference, native animation setter or player factory.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
