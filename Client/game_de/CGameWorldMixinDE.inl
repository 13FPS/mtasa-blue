/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep the existing world policy and state on one selected game base
 ****************************************************************************/
#pragma once
#include <game/CPools.h>
#include <game/CDummyPool.h>
#include <stdexcept>
namespace mta::game_de
{
    namespace GameWorldDetail
    {
        inline CPools& RequirePools(CPools* pools)
        {
            if (!pools)
                throw std::runtime_error("DE world lifecycle requires the qualified pools service");
            return *pools;
        }
    }
    template <class GameBase>
    void CGameWorldMixinDE<GameBase>::RemoveGameWorld()
    {
        RequireWorldLifecycleReady();
        MtaGame::CGameWorldPolicy::Remove([this] { DisableWorldIplStreaming(); }, [this] { RemoveWorldCovers(); }, [this] { RemoveWorldPlants(); },
                                          [this] { DestroyWorldStencilShadows(); },
                                          [this] { GameWorldDetail::RequirePools(this->GetPools()).GetDummyPool().RemoveAllWithBackup(); },
                                          [this] { GameWorldDetail::RequirePools(this->GetPools()).GetBuildingsPool().RemoveAllWithBackup(); },
                                          [this] { DropWorldBuildingCaches(); }, [this](bool removed) { m_worldRemoved = removed; });
    }
    template <class GameBase>
    void CGameWorldMixinDE<GameBase>::RestoreGameWorld()
    {
        RequireWorldLifecycleReady();
        MtaGame::CGameWorldPolicy::Restore([this] { GameWorldDetail::RequirePools(this->GetPools()).GetBuildingsPool().RestoreBackup(); },
                                           [this] { GameWorldDetail::RequirePools(this->GetPools()).GetDummyPool().RestoreBackup(); },
                                           [this](MtaGame::CGameWorldPolicy::IplFilter filter) { EnableWorldIplStreaming(filter); },
                                           [this](bool removed) { m_worldRemoved = removed; });
    }
    template <class GameBase>
    bool CGameWorldMixinDE<GameBase>::SetBuildingPoolSize(std::size_t size)
    {
        // Oversize refusal precedes owner or SDK access, just like classic.
        if (!MtaGame::CGameWorldPolicy::IsBuildingPoolSizeRepresentable(size))
            return false;
        RequireWorldLifecycleReady();
        return MtaGame::CGameWorldPolicy::SetBuildingPoolSize(
            size, m_worldRemoved, [this] { return GameWorldDetail::RequirePools(this->GetPools()).GetBuildingsPool().GetSize(); },
            [this] { RemoveGameWorld(); }, [this] { DropWorldBuildingCaches(); }, [this](int capacity)
            { return GameWorldDetail::RequirePools(this->GetPools()).GetBuildingsPool().Resize(capacity); }, [this] { RestoreGameWorld(); });
    }
}
