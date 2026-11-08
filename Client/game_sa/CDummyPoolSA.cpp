/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CDummyPoolSA.cpp
 *  PURPOSE:     Dummy pool class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include "StdInc.h"
#include "CDummyPoolSA.h"
#include <game/CWorldPoolBackupPolicy.h>
#include "CBuildingRemovalSA.h"
#include "CGameSA.h"
#include <game/CWorld.h>

extern CGameSA* pGame;

CDummyPoolSA::CDummyPoolSA()
{
    m_ppDummyPoolInterface = (CPoolSAInterface<CEntitySAInterface>**)0xB744A0;
}

void CDummyPoolSA::RemoveAllWithBackup()
{
    if (m_pOriginalElementsBackup)
        return;

    m_pOriginalElementsBackup = std::make_unique<pool_backup_t>();

    auto                pDummyPool = (*m_ppDummyPoolInterface);
    CEntitySAInterface* building = nullptr;
    MtaGame::CWorldPoolBackupPolicy::BackupSlots(
        MAX_DUMMIES_DEFAULT,
        [&](std::size_t index)
        {
            if (!pDummyPool->IsContains(static_cast<std::int32_t>(index)))
                return false;
            building = pDummyPool->GetObject(static_cast<std::int32_t>(index));
            return true;
        },
        [&](std::size_t)
        {
            pGame->GetWorld()->Remove(building, CDummyPool_Destructor);
            building->RemoveRWObjectWithReferencesCleanup();
            return true;
        },
        // Classic dummy backup does not separately detach/save matrices.
        // This explicit callback is a policy difference, not DE authority.
        [](std::size_t) { return true; },
        [&](std::size_t index)
        {
            pDummyPool->Release(static_cast<uint>(index));
            return true;
        },
        [&](std::size_t index)
        {
            (*m_pOriginalElementsBackup)[index].first = true;
            std::memcpy((*m_pOriginalElementsBackup)[index].second, building, sizeof(CEntitySAInterface));
            return true;
        },
        [&](std::size_t index)
        {
            (*m_pOriginalElementsBackup)[index].first = false;
            return true;
        });
}

void CDummyPoolSA::RestoreBackup()
{
    if (!m_pOriginalElementsBackup)
        return;

    auto* pBuildingRemoval = static_cast<CBuildingRemovalSA*>(pGame->GetBuildingRemoval());

    auto&               originalData = *m_pOriginalElementsBackup;
    auto                pDummyPool = (*m_ppDummyPoolInterface);
    CEntitySAInterface* pDummy = nullptr;
    // The real classic dummy pool uses this fixed bound for both loops. DE
    // must qualify its actual allocation instead of inheriting this constant.
    MtaGame::CWorldPoolBackupPolicy::RestoreSlots(
        MAX_DUMMIES_DEFAULT, MAX_DUMMIES_DEFAULT, [&](std::size_t index) { return originalData[index].first; },
        [&](std::size_t index)
        {
            pDummyPool->AllocateAtNoInit(static_cast<std::uint32_t>(index));
            pDummy = pDummyPool->GetObject(static_cast<std::int32_t>(index));
            std::memcpy(pDummy, &originalData[index].second, sizeof(CEntitySAInterface));
            return true;
        },
        [](std::size_t) { return true; },
        [&](std::size_t)
        {
            pGame->GetWorld()->Add(pDummy, CDummyPool_Constructor);
            return true;
        },
        [&](std::size_t)
        {
            pBuildingRemoval->AddDataBuildingAndReapplyRemoval(pDummy);
            return true;
        });

    m_pOriginalElementsBackup = nullptr;
}

void CDummyPoolSA::UpdateBuildingLods(const std::uint32_t offset, const std::uintptr_t oldPoolStart, const std::uintptr_t oldPoolEnd)
{
    if (m_pOriginalElementsBackup)
        UpdateBackupLodOffset(offset, oldPoolStart, oldPoolEnd);
    else
        UpdateLodsOffestInPool(offset, oldPoolStart, oldPoolEnd);
}

// A dummy's LOD can point at another dummy just as easily as at a building, so only shift it
// when it actually falls inside the buildings pool range that just moved; otherwise the pointer
// is left as is, since it was never invalidated by this resize.
void CDummyPoolSA::UpdateBackupLodOffset(const std::uint32_t offset, const std::uintptr_t oldPoolStart, const std::uintptr_t oldPoolEnd)
{
    for (auto& it : *m_pOriginalElementsBackup)
    {
        if (it.first)
        {
            CEntitySAInterface* object = reinterpret_cast<CEntitySAInterface*>(&it.second);
            CEntitySAInterface* lod = object->GetLod();
            auto                lodAddress = reinterpret_cast<std::uintptr_t>(lod);
            if (MtaGame::CWorldPoolBackupPolicy::InMovedRange(lodAddress, oldPoolStart, oldPoolEnd))
                object->SetLod(reinterpret_cast<CEntitySAInterface*>(lodAddress + offset));
        }
    }
}

void CDummyPoolSA::UpdateLodsOffestInPool(const std::uint32_t offset, const std::uintptr_t oldPoolStart, const std::uintptr_t oldPoolEnd)
{
    for (auto i = 0; i < (*m_ppDummyPoolInterface)->Size(); i++)
    {
        CEntitySAInterface* object = (*m_ppDummyPoolInterface)->GetObject(i);
        CEntitySAInterface* lod = object->GetLod();
        auto                lodAddress = reinterpret_cast<std::uintptr_t>(lod);
        if (MtaGame::CWorldPoolBackupPolicy::InMovedRange(lodAddress, oldPoolStart, oldPoolEnd))
            object->SetLod(reinterpret_cast<CEntitySAInterface*>(lodAddress + offset));
    }
}
