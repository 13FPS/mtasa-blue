/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Restore actual model-group defaults through the enclosing MTA game
 ****************************************************************************/
#pragma once
#include "CGameWorldCoverDE.h"

namespace mta::game_de
{
    class CGameObjectGroupResetDE : public CGameWorldCoverDE
    {
    protected:
        using CGameWorldCoverDE::CGameWorldCoverDE;
        void RestoreObjectModelGroups() override;
    };
    // One more real Reset effect does not qualify the remaining game services,
    // world restoration prerequisites or the complete CGame factory.
    static_assert(std::is_abstract_v<CGameObjectGroupResetDE> && std::is_convertible_v<CGameObjectGroupResetDE*, CGame*>);
}
