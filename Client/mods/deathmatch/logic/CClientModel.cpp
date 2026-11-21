/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CClientModel.h
 *  PURPOSE:     Model handling class
 *
 *****************************************************************************/

#include "StdInc.h"
#include "game/CStreaming.h"

CClientModel::CClientModel(CClientManager* pManager, int iModelID, eClientModelType eModelType)
{
    m_pManager = pManager;
    m_iModelID = iModelID;
    m_eModelType = eModelType;
}

CClientModel::~CClientModel(void)
{
    Deallocate();
}

bool CClientModel::Allocate(ushort usParentID)
{
    if (m_bAllocatedByUs || m_iModelID < 0 || m_iModelID >= MAX_MODEL_DFF_ID)
        return false;
    CModelInfo* pModelInfo = g_pGame->GetModelInfo(m_iModelID, true);

    // Allocate only on free IDs
    if (!pModelInfo || pModelInfo->IsValid())
        return false;

    // Avoid hierarchy
    CModelInfo* pParentModelInfo = g_pGame->GetModelInfo(usParentID, true);

    if (!pParentModelInfo || pParentModelInfo->GetParentID())
        return false;

    switch (m_eModelType)
    {
        case eClientModelType::PED:
            pModelInfo->MakePedModel("PSYCHO");
            break;
        case eClientModelType::OBJECT:
            if (!g_pClientGame->GetObjectManager()->IsValidModel(usParentID))
                return false;
            pModelInfo->MakeObjectModel(usParentID);
            break;
        case eClientModelType::OBJECT_DAMAGEABLE:
        {
            if (!g_pClientGame->GetObjectManager()->IsValidModel(usParentID) || !pParentModelInfo->IsDamageableAtomic())
                return false;
            pModelInfo->MakeObjectDamageableModel(usParentID);
            break;
        }
        case eClientModelType::CLUMP:
            if (!g_pClientGame->GetObjectManager()->IsValidModel(usParentID))
                return false;
            pModelInfo->MakeClumpModel(usParentID);
            break;
        case eClientModelType::TIMED_OBJECT:
            if (!g_pClientGame->GetObjectManager()->IsValidModel(usParentID))
                return false;
            pModelInfo->MakeTimedObjectModel(usParentID);
            break;
        case eClientModelType::VEHICLE:
            if (!g_pClientGame->GetVehicleManager()->IsValidModel(usParentID))
                return false;
            pModelInfo->MakeVehicleAutomobile(usParentID);
            break;
        default:
            return false;
    }
    // MakePedModel can publish before a later streaming request throws. Only a
    // completed make hands this client the identity it can subsequently release.
    m_pAllocatedModelInterface = pModelInfo->GetInterface();
    m_bAllocatedByUs = m_pAllocatedModelInterface != nullptr;
    return m_bAllocatedByUs;
}

// You can call it only in destructor for DFF.
bool CClientModel::Deallocate()
{
    if (!m_bAllocatedByUs)
        return false;

    m_bAllocatedByUs = false;
    const auto* allocatedInterface = m_pAllocatedModelInterface;
    m_pAllocatedModelInterface = nullptr;
    SetParentResource(nullptr);

    CModelInfo* pModelInfo = g_pGame->GetModelInfo(m_iModelID, true);
    if (!pModelInfo || !pModelInfo->IsValid())
        return false;

    if (m_eModelType == eClientModelType::TXD)
        RestoreTXD(pModelInfo);
    else
    {
        if (pModelInfo->GetInterface() != allocatedInterface)
            return false;
        pModelInfo->DeallocateModel();
    }

    return true;
}

void CClientModel::RestoreEntitiesUsingThisModel()
{
    if (!m_bAllocatedByUs)
        return;
    if (m_eModelType == eClientModelType::TXD)
    {
        Deallocate();
        return;
    }

    CModelInfo* pModelInfo = g_pGame->GetModelInfo(m_iModelID, true);
    if (!pModelInfo || !pModelInfo->IsValid() || pModelInfo->GetInterface() != m_pAllocatedModelInterface)
        return;

    switch (m_eModelType)
    {
        case eClientModelType::PED:
        case eClientModelType::OBJECT:
        case eClientModelType::OBJECT_DAMAGEABLE:
        case eClientModelType::CLUMP:
        case eClientModelType::TIMED_OBJECT:
        case eClientModelType::VEHICLE:
            RestoreDFF(pModelInfo);
            return;
        default:
            return;
    }
}

void CClientModel::RestoreDFF(CModelInfo* pModelInfo)
{
    auto callElementChangeEvent = [](auto& element, unsigned short usParentID, auto modelId)
    {
        CLuaArguments Arguments;
        Arguments.PushNumber(modelId);
        Arguments.PushNumber(usParentID);
        element.CallEvent("onClientElementModelChange", Arguments, true);
    };

    auto unloadModelsAndCallEvents = [&](auto iterBegin, auto iterEnd, unsigned short usParentID, auto setElementModelLambda)
    {
        for (auto iter = iterBegin; iter != iterEnd; iter++)
        {
            auto& element = **iter;

            if (element.GetModel() != m_iModelID)
                continue;

            if (element.IsStreamedIn())
                element.StreamOutForABit();

            setElementModelLambda(element);

            callElementChangeEvent(element, usParentID, m_iModelID);
        }
    };

    auto unloadModelsAndCallEventsNonStreamed = [&](auto iterBegin, auto iterEnd, unsigned short usParentID, auto setElementModelLambda)
    {
        for (auto iter = iterBegin; iter != iterEnd; iter++)
        {
            auto& element = **iter;

            if (element.GetModel() != m_iModelID)
                continue;

            setElementModelLambda(element);
            callElementChangeEvent(element, usParentID, m_iModelID);
        }
    };

    switch (m_eModelType)
    {
        case eClientModelType::PED:
        {
            // If some ped is using this ID, change him to CJ
            CClientPedManager* pPedManager = g_pClientGame->GetManager()->GetPedManager();

            unloadModelsAndCallEvents(pPedManager->IterBegin(), pPedManager->IterEnd(), 0, [](auto& element) { element.SetModel(0); });
            break;
        }
        case eClientModelType::CLUMP:
        case eClientModelType::OBJECT:
        case eClientModelType::OBJECT_DAMAGEABLE:
        case eClientModelType::TIMED_OBJECT:
        {
            const auto& objects = &g_pClientGame->GetManager()->GetObjectManager()->GetObjects();
            const auto  usParentID = static_cast<unsigned short>(g_pGame->GetModelInfo(m_iModelID)->GetParentID());

            unloadModelsAndCallEvents(objects->begin(), objects->end(), usParentID, [=](auto& element) { element.SetModel(usParentID); });

            // Restore pickups with custom model
            CClientPickupManager* pPickupManager = g_pClientGame->GetManager()->GetPickupManager();

            unloadModelsAndCallEvents(pPickupManager->IterBegin(), pPickupManager->IterEnd(), usParentID, [=](auto& element) { element.SetModel(usParentID); });

            // Restore buildings
            CClientBuildingManager* pBuildingsManager = g_pClientGame->GetManager()->GetBuildingManager();
            auto&                   buildingsList = pBuildingsManager->GetBuildings();

            unloadModelsAndCallEventsNonStreamed(buildingsList.begin(), buildingsList.end(), usParentID, [=](auto& element) { element.SetModel(usParentID); });

            // Restore COL
            g_pClientGame->GetManager()->GetColModelManager()->RestoreModel(static_cast<unsigned short>(m_iModelID));
            break;
        }
        case eClientModelType::VEHICLE:
        {
            CClientVehicleManager* pVehicleManager = g_pClientGame->GetManager()->GetVehicleManager();
            const auto             usParentID = static_cast<unsigned short>(g_pGame->GetModelInfo(m_iModelID)->GetParentID());

            unloadModelsAndCallEvents(pVehicleManager->IterBegin(), pVehicleManager->IterEnd(), usParentID,
                                      [=](auto& element) { element.SetModelBlocking(usParentID, 255, 255); });
            break;
        }
    }

    // Restore DFF/TXD
    g_pClientGame->GetManager()->GetDFFManager()->RestoreModel(static_cast<unsigned short>(m_iModelID));
}

bool CClientModel::AllocateTXD(std::string& strTxdName)
{
    if (m_bAllocatedByUs || m_eModelType != eClientModelType::TXD || m_iModelID < MAX_MODEL_DFF_ID ||
        static_cast<unsigned>(m_iModelID) >= g_pGame->GetBaseIDforCOL())
        return false;
    auto&      pool = g_pGame->GetPools()->GetTxdPool();
    const auto slot = static_cast<std::uint32_t>(m_iModelID - MAX_MODEL_DFF_ID);
    if (!pool.IsFreeTextureDictonarySlot(slot))
        return false;
    m_bAllocatedByUs = pool.AllocateTextureDictonarySlot(slot, strTxdName) == slot;
    return m_bAllocatedByUs;
}

void CClientModel::RestoreTXD(CModelInfo* pModelInfo)
{
    uint uiTextureDictonarySlotID = pModelInfo->GetModel() - MAX_MODEL_DFF_ID;

    for (uint uiModelID = 0; uiModelID < MAX_MODEL_DFF_ID; uiModelID++)
    {
        CModelInfo* pModelInfo = g_pGame->GetModelInfo(uiModelID, true);

        if (pModelInfo && pModelInfo->GetTextureDictionaryID() == uiTextureDictonarySlotID)
            pModelInfo->SetTextureDictionaryID(0);
    }

    g_pGame->GetPools()->GetTxdPool().RemoveTextureDictonarySlot(uiTextureDictonarySlotID);
    g_pGame->GetStreaming()->SetStreamingInfo(pModelInfo->GetModel(), 0, 0, 0, -1);
}
