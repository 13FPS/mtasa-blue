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
    // One linear base shares the real game identity and exactly one world
    // state with either the legacy or startup-qualified reset composition.
    template <class GameBase>
    class CGameWorldMixinDE : public GameBase
    {
    public:
        void RemoveGameWorld() override;
        void RestoreGameWorld() override;
        bool SetBuildingPoolSize(std::size_t size) override;

    protected:
        using GameBase::GameBase;
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
    // Preserve the legacy injected name and protected constructor routes
    // without fields or forwarding overrides; the mixin owns the one policy.
    class CGameWorldDE : public CGameWorldMixinDE<CGameResetDE>
    {
    protected:
        using CGameWorldMixinDE<CGameResetDE>::CGameWorldMixinDE;
    };
    static_assert(std::is_abstract_v<CGameWorldDE> && std::is_base_of_v<CGameResetDE, CGameWorldDE>);
}

#include "CGameWorldMixinDE.inl"
