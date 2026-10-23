/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Extend the existing MTA world SDK with a qualified clear-line query
 *****************************************************************************/
#pragma once
#include "CWorldGroundDE.h"
#include "NativeWorldLineAccess.h"
namespace mta::game_de
{
    class CWorldLineDE : public CWorldGroundDE
    {
    public:
        bool IsLineOfSightClear(const CVector* start, const CVector* end, const SLineOfSightFlags flags) override;

    protected:
        CWorldLineDE(NativeWorldGroundAccess& ground, NativeWorldLineAccess& line) noexcept : CWorldGroundDE(ground), lineOwner(line) {}

    private:
        NativeWorldLineAccess& lineOwner;
    };
    // Preserve the actual inherited ground operation. Neither scalar query
    // supplies native entity ownership, IgnoreEntity or a complete world factory.
    static_assert(std::is_abstract_v<CWorldLineDE> && std::is_base_of_v<CWorldGroundDE, CWorldLineDE>);
}
