/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CClientModelManager.cpp
 *  PURPOSE:     Model manager class
 *
 *****************************************************************************/

#include "StdInc.h"
#include <algorithm>
#include <limits>

CClientModelManager::CClientModelManager() : m_Models(g_pGame->GetBaseIDforCOL())
{
}

CClientModelManager::~CClientModelManager(void)
{
    RemoveAll();
}

void CClientModelManager::RemoveAll(void)
{
    for (auto& entry : m_Models)
    {
        auto model = std::move(entry);
        if (model)
        {
            --m_modelCount;
            model->SetParentResource(nullptr);
            if (model->GetModelType() == eClientModelType::TXD)
                model->Deallocate();
        }
    }
}

void CClientModelManager::Add(const std::shared_ptr<CClientModel>& pModel)
{
    if (!pModel || !pModel->m_bAllocatedByUs || pModel->GetModelID() < 0 || static_cast<unsigned>(pModel->GetModelID()) >= m_Models.size())
        return;
    if (m_Models[pModel->GetModelID()] != nullptr)
    {
        dassert(m_Models[pModel->GetModelID()].get() == pModel.get());
        return;
    }
    m_Models[pModel->GetModelID()] = pModel;
    m_modelCount++;
}

bool CClientModelManager::Remove(const std::shared_ptr<CClientModel>& pModel)
{
    if (!pModel || FindModelByID(pModel->GetModelID()) != pModel)
        return false;
    const int modelId = pModel->GetModelID();
    // Restoration dispatches Lua events. Detach this registration before callbacks
    // and keep the model alive while its entities release their shared references.
    auto model = std::move(m_Models[modelId]);
    --m_modelCount;
    if (auto* resource = model->GetParentResource())
        resource->GetResourceModelStreamer()->FullyReleaseModel(static_cast<std::uint16_t>(modelId));
    model->SetParentResource(nullptr);
    model->RestoreEntitiesUsingThisModel();
    return true;
}

int CClientModelManager::GetFirstFreeModelID(void)
{
    const auto uiMaxModelID = std::min<std::size_t>(MAX_MODEL_DFF_ID, m_Models.size());
    for (unsigned int i = 0; i < uiMaxModelID; i++)
    {
        if (m_Models[i])
            continue;
        CModelInfo* pModelInfo = g_pGame->GetModelInfo(i, true);
        if (pModelInfo && !pModelInfo->IsValid())
            return i;
    }
    return INVALID_MODEL_ID;
}

int CClientModelManager::GetFreeTxdModelID()
{
    if (m_Models.size() <= MAX_MODEL_DFF_ID)
        return INVALID_MODEL_ID;

    const std::uint32_t txdId = g_pGame->GetPools()->GetTxdPool().GetFreeTextureDictonarySlot();
    const std::uint64_t modelId = static_cast<std::uint64_t>(txdId) + MAX_MODEL_DFF_ID;
    if (modelId >= m_Models.size() || modelId > std::numeric_limits<int>::max())
        return INVALID_MODEL_ID;
    return static_cast<int>(modelId);
}

std::shared_ptr<CClientModel> CClientModelManager::FindModelByID(int iModelID)
{
    if (iModelID >= 0 && static_cast<unsigned>(iModelID) < m_Models.size())
        return m_Models[iModelID];

    return nullptr;
}

std::shared_ptr<CClientModel> CClientModelManager::Request(CClientManager* pManager, int iModelID, eClientModelType eType)
{
    if (iModelID < 0 || static_cast<unsigned>(iModelID) >= m_Models.size() || m_Models[iModelID])
        return nullptr;
    return std::make_shared<CClientModel>(pManager, iModelID, eType);
}

std::vector<std::shared_ptr<CClientModel>> CClientModelManager::GetModelsByType(const eClientModelType type, const unsigned int minModelID)
{
    std::vector<std::shared_ptr<CClientModel>> found;
    found.reserve(m_modelCount);

    for (std::size_t i = minModelID; i < m_Models.size(); ++i)
    {
        const std::shared_ptr<CClientModel>& model = m_Models[i];
        if (model && model->GetModelType() == type)
        {
            found.push_back(model);
        }
    }
    return found;
}

void CClientModelManager::DeallocateModelsAllocatedByResource(CResource* pResource)
{
    for (const auto& model : m_Models)
    {
        if (model && model->GetParentResource() == pResource)
            Remove(model);
    }
}
