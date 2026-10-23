/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual world lifecycle orchestration over mandatory native services
 *****************************************************************************/
#pragma once
#include "CGameResetDE.h"
#include <game/CGameWorldPolicy.h>
namespace mta::game_de
{
    class CGameWorldDE : public CGameResetDE
    {
    public:
        void RemoveGameWorld() override;
        void RestoreGameWorld() override;
        bool SetBuildingPoolSize(std::size_t size) override;

    protected:
        using CGameResetDE::CGameResetDE;
        // The enclosing backend must freshly admit all actual receivers and
        // effects. This CPU phase is neither a native lease nor pool readiness.
        virtual void RequireWorldLifecycleReady() = 0;
        virtual void DisableWorldIplStreaming() = 0;
        virtual void RemoveWorldCovers() = 0;
        virtual void RemoveWorldPlants() = 0;
        virtual void DestroyWorldStencilShadows() = 0;
        virtual void DropWorldBuildingCaches() = 0;
        // Apply the supplied predicate to exact copied eight-byte IPL names;
        // no classic IPL interface/layout or guessed DE registration is exposed.
        virtual void EnableWorldIplStreaming(MtaGame::CGameWorldPolicy::IplFilter filter) = 0;
        bool         IsGameWorldRemoved() const noexcept { return m_worldRemoved; }

    private:
        bool m_worldRemoved = false;
    };
    static_assert(std::is_abstract_v<CGameWorldDE> && std::is_base_of_v<CGameResetDE, CGameWorldDE>);
}
