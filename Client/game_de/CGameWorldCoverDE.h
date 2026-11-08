/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Supply MTA's real Cover effect in the existing world lifecycle
 ****************************************************************************/
#pragma once
#include "CGameWorldIplDE.h"

namespace mta::game_de
{
    class CGameWorldCoverDE : public CGameWorldIplDE
    {
    protected:
        using CGameWorldIplDE::CGameWorldIplDE;
        void RemoveWorldCovers() override;
    };
    // One actual effect joins the same linear CGame object. Plant, stencil,
    // pool and complete readiness hooks stay mandatory rather than no-ops.
    static_assert(std::is_abstract_v<CGameWorldCoverDE> && std::is_convertible_v<CGameWorldCoverDE*, CGame*>);
}
