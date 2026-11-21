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
CClientModelManager::CClientModelManager() : m_Models(std::make_unique<std::shared_ptr<CClientModel>[]>(g_pGame->GetBaseIDforCOL()))
{
    const unsigned int uiMaxModelID = g_pGame->GetBaseIDforCOL();
    for (unsigned int i = 0; i < uiMaxModelID; i++)
    {
        m_Models[i] = nullptr;
    }
}

CClientModelManager::~CClientModelManager(void)
{
    RemoveAll();
}

void CClientModelManager::RemoveAll(void)
{
    const unsigned int uiMaxModelID = g_pGame->GetBaseIDforCOL();
    for (unsigned int i = 0; i < uiMaxModelID; i++)
    {
        auto model = std::move(m_Models[i]);
        if (model)
        {
            model->SetParentResource(nullptr);
            if (model->GetModelType() == eClientModelType::TXD)
                model->Deallocate();
        }
    }
    m_modelCount = 0;
}

void CClientModelManager::Add(const std::shared_ptr<CClientModel>& pModel)
{
    if (!pModel || !pModel->m_bAllocatedByUs || pModel->GetModelID() < 0 || static_cast<unsigned>(pModel->GetModelID()) >= g_pGame->GetBaseIDforCOL())
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
    const unsigned int uiMaxModelID = MAX_MODEL_DFF_ID;
    for (unsigned int i = 0; i < uiMaxModelID; i++)
    {
        CModelInfo* pModelInfo = g_pGame->GetModelInfo(i, true);
        if (!m_Models[i] && pModelInfo && !pModelInfo->IsValid())
        {
            return i;
        }
    }
    return INVALID_MODEL_ID;
}

int CClientModelManager::GetFreeTxdModelID()
{
    std::uint32_t usTxdId = g_pGame->GetPools()->GetTxdPool().GetFreeTextureDictonarySlot();

    if (usTxdId == -1)
        return INVALID_MODEL_ID;

    return MAX_MODEL_DFF_ID + usTxdId;
}

std::shared_ptr<CClientModel> CClientModelManager::FindModelByID(int iModelID)
{
    int32_t iMaxModelId = g_pGame->GetBaseIDforCOL();

    // Lua-facing callers can supply invalid IDs, so check both bounds before indexing the model array.
    if (iModelID >= 0 && iModelID < iMaxModelId)
        return m_Models[iModelID];

    return nullptr;
}

std::shared_ptr<CClientModel> CClientModelManager::Request(CClientManager* pManager, int iModelID, eClientModelType eType)
{
    if (iModelID < 0 || static_cast<unsigned>(iModelID) >= g_pGame->GetBaseIDforCOL() || m_Models[iModelID])
        return nullptr;
    return std::make_shared<CClientModel>(pManager, iModelID, eType);
}

std::vector<std::shared_ptr<CClientModel>> CClientModelManager::GetModelsByType(const eClientModelType type, const unsigned int minModelID)
{
    std::vector<std::shared_ptr<CClientModel>> found;
    found.reserve(m_modelCount);

    const unsigned int uiMaxModelID = g_pGame->GetBaseIDforCOL();
    for (unsigned int i = minModelID; i < uiMaxModelID; i++)
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
    const unsigned int uiMaxModelID = g_pGame->GetBaseIDforCOL();
    for (unsigned int i = 0; i < uiMaxModelID; i++)
    {
        if (m_Models[i] != nullptr && m_Models[i]->GetParentResource() == pResource)
            Remove(m_Models[i]);
    }
}
