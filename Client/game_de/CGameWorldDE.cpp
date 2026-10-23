/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual MTA world ordering without inventing native pool effects
 *****************************************************************************/
#include "CGameWorldDE.h"
#include <game/CPools.h>
#include <game/CDummyPool.h>
#include <stdexcept>
namespace mta::game_de
{
    namespace
    {
        CPools& RequireWorldPools(CPools* pools)
        {
            if (!pools)
                throw std::runtime_error("DE world lifecycle requires the qualified pools service");
            return *pools;
        }
    }
    void CGameWorldDE::RemoveGameWorld()
    {
        RequireWorldLifecycleReady();
        MtaGame::CGameWorldPolicy::Remove([this] { DisableWorldIplStreaming(); }, [this] { RemoveWorldCovers(); }, [this] { RemoveWorldPlants(); }, [this]
                                          { DestroyWorldStencilShadows(); }, [this] { RequireWorldPools(GetPools()).GetDummyPool().RemoveAllWithBackup(); },
                                          [this] { RequireWorldPools(GetPools()).GetBuildingsPool().RemoveAllWithBackup(); },
                                          [this] { DropWorldBuildingCaches(); }, [this](bool removed) { m_worldRemoved = removed; });
    }
    void CGameWorldDE::RestoreGameWorld()
    {
        RequireWorldLifecycleReady();
        MtaGame::CGameWorldPolicy::Restore([this] { RequireWorldPools(GetPools()).GetBuildingsPool().RestoreBackup(); },
                                           [this] { RequireWorldPools(GetPools()).GetDummyPool().RestoreBackup(); },
                                           [this](MtaGame::CGameWorldPolicy::IplFilter filter) { EnableWorldIplStreaming(filter); },
                                           [this](bool removed) { m_worldRemoved = removed; });
    }
    bool CGameWorldDE::SetBuildingPoolSize(std::size_t size)
    {
        // Oversize refusal precedes owner or SDK access, just like classic.
        if (!MtaGame::CGameWorldPolicy::IsBuildingPoolSizeRepresentable(size))
            return false;
        RequireWorldLifecycleReady();
        return MtaGame::CGameWorldPolicy::SetBuildingPoolSize(
            size, m_worldRemoved, [this] { return RequireWorldPools(GetPools()).GetBuildingsPool().GetSize(); }, [this] { RemoveGameWorld(); },
            [this] { DropWorldBuildingCaches(); }, [this](int capacity) { return RequireWorldPools(GetPools()).GetBuildingsPool().Resize(capacity); },
            [this] { RestoreGameWorld(); });
    }
}
