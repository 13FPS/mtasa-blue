/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route the native IPL effects through MTA's one world-lifecycle game
 ****************************************************************************/
#pragma once
#include "CGameModelWheelResetDE.h"
namespace mta::game_de
{
    class CGameWorldIplDE : public CGameModelWheelResetDE
    {
    protected:
        using CGameModelWheelResetDE::CGameModelWheelResetDE;
        void DisableWorldIplStreaming() override;
        void EnableWorldIplStreaming(MtaGame::CGameWorldPolicy::IplFilter) override;
    };
    // IPL support supplies two effects only. Covers, plants, shadows, pools,
    // caches and the full readiness gate are still mandatory native owners.
    static_assert(std::is_abstract_v<CGameWorldIplDE> && std::is_convertible_v<CGameWorldIplDE*, CGame*>);
}
