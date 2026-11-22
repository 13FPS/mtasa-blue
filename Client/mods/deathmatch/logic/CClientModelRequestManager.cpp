/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientModelRequestManager.cpp
 *  PURPOSE:     Entity model streaming manager class
 *
 *****************************************************************************/

#include "StdInc.h"
#include <algorithm>
#include <exception>

namespace
{
    // A failed SDK release may already have decremented the reference. Keep
    // its first error visible and never replay it during later cleanup.
    void ReleaseModel(CModelInfo* model, std::exception_ptr& failure)
    {
        try
        {
            model->RemoveRef();
        }
        catch (...)
        {
            if (!failure)
                failure = std::current_exception();
            else
                OutputDebugLine("[Models] Another model reference failed to release during cleanup");
        }
    }
}

CClientModelRequestManager::CClientModelRequestManager() = default;

CClientModelRequestManager::~CClientModelRequestManager()
{
    std::exception_ptr failure;
    while (!m_Requests.empty())
        ReleaseModel(Detach(m_Requests.begin()).pModel, failure);
    if (failure)
        OutputDebugLine("[Models] A pending model reference failed to release during shutdown");
}

CClientModelRequestManager::Requests::iterator CClientModelRequestManager::FindRequest(CClientEntity* requester)
{
    return std::find_if(m_Requests.begin(), m_Requests.end(), [requester](const auto& entry) { return entry.pEntity == requester; });
}

SClientModelRequest CClientModelRequestManager::Detach(Requests::iterator iter)
{
    auto entry = std::move(*iter);
    m_Requests.erase(iter);
    return entry;
}

bool CClientModelRequestManager::IsLoaded(unsigned short usModelID)
{
    CModelInfo* info = g_pGame->GetModelInfo(usModelID);
    return info && info->IsLoaded();
}

bool CClientModelRequestManager::IsRequested(CModelInfo* pModelInfo)
{
    return std::any_of(m_Requests.begin(), m_Requests.end(), [pModelInfo](const auto& entry) { return entry.pModel == pModelInfo; });
}

bool CClientModelRequestManager::HasRequested(CClientEntity* pRequester)
{
    assert(pRequester);
    return FindRequest(pRequester) != m_Requests.end();
}

CModelInfo* CClientModelRequestManager::GetRequestedModelInfo(CClientEntity* pRequester)
{
    assert(pRequester);
    const auto entry = FindRequest(pRequester);
    return entry != m_Requests.end() ? entry->pModel : nullptr;
}

bool CClientModelRequestManager::RequestBlocking(unsigned short usModelID, const char* szTag)
{
    // Grab the model info
    CModelInfo* pInfo = g_pGame->GetModelInfo(usModelID);
    if (pInfo)
    {
        pInfo->Request(BLOCKING, szTag);
        if (pInfo->IsLoaded())
        {
            pInfo->MakeCustomModel();
            return true;
        }
        OutputDebugLine(SString("[Models] RequestBlocking failed for id %d", usModelID));
    }

    // Bad model ID probably.
    return false;
}

bool CClientModelRequestManager::Request(unsigned short usModelID, CClientEntity* pRequester)
{
    return Request(usModelID, pRequester, nullptr);
}

bool CClientModelRequestManager::RequestForStreamIn(unsigned short usModelID, CClientStreamElement* pRequester)
{
    // A vehicle can also ask its passengers to stream in directly, outside their own stream radius.
    return Request(usModelID, pRequester, pRequester->IsAttemptingToStreamIn() ? pRequester : nullptr);
}

bool CClientModelRequestManager::Request(unsigned short usModelID, CClientEntity* pRequester, CClientStreamElement* rangeOwner)
{
    assert(pRequester);
    CModelInfo* info = g_pGame->GetModelInfo(usModelID);
    if (!info)
        return false;

    auto       registration = g_pClientGame->GetManager()->GetModelManager()->FindModelByID(usModelID);
    auto       previous = FindRequest(pRequester);
    const bool replacing = previous != m_Requests.end();
    auto       position = m_Requests.end();
    if (replacing)
    {
        if (previous->pModel == info)
        {
            // Any explicit demand keeps this request alive. A canceled entry starts afresh.
            if (previous->canceled || !rangeOwner)
                previous->rangeOwner = rangeOwner;
            previous->canceled = false;
            return false;
        }
        // Retire the old entry before any SDK call can fail. Its registration
        // stays alive through RemoveRef even if the model was unregistered.
        position = std::next(previous);
        Detach(previous).pModel->RemoveRef();
    }

    if (info->IsLoaded())
    {
        info->MakeCustomModel();
        return true;
    }

    // Allocate the node before acquiring the reference. Splicing it into the
    // live queue cannot fail after ModelAddRef returns.
    Requests pending;
    auto&    entry = pending.emplace_back();
    entry.pEntity = pRequester;
    entry.rangeOwner = rangeOwner;
    entry.pModel = info;
    entry.registration = std::move(registration);
    entry.requestTimer.SetMaxIncrement(500);
    entry.requestTimer.Reset();
    info->ModelAddRef(NON_BLOCKING, replacing ? "CClientModelRequestManager::Request" : "CClientModelRequestManager::Request #2");
    m_Requests.splice(position, pending);
    return false;
}

void CClientModelRequestManager::Cancel(CClientEntity* pEntity, bool bAllowQueue)
{
    assert(pEntity);
    auto entry = FindRequest(pEntity);
    if (entry == m_Requests.end())
        return;

    if (m_bDoingPulse)
    {
        assert(bAllowQueue);
        entry->canceled = true;
    }
    else
        Detach(entry).pModel->RemoveRef();
}

void CClientModelRequestManager::DoPulse()
{
    if (m_Requests.empty() || m_bDoingPulse)
        return;
    m_bDoingPulse = true;
    std::exception_ptr failure;
    try
    {
        for (auto iter = m_Requests.begin(); iter != m_Requests.end();)
        {
            if (!iter->canceled && iter->rangeOwner && !iter->rangeOwner->IsModelStreamInWanted())
                iter->canceled = true;
            if (iter->canceled)
            {
                ++iter;
                continue;
            }
            if (iter->pModel->IsLoaded())
            {
                // Entity callbacks can cancel, replace or create other requests.
                // Detach this request before calling them, keeping its registration.
                auto entry = Detach(iter);
                try
                {
                    entry.pModel->MakeCustomModel();
                    entry.pEntity->ModelRequestCallback(entry.pModel);
                }
                catch (...)
                {
                    failure = std::current_exception();
                }
                ReleaseModel(entry.pModel, failure);
                if (failure)
                    break;
                iter = m_Requests.begin();
            }
            else
            {
                if (iter->requestTimer.Get() > 2000)
                {
                    if (g_pGame->IsASyncLoadingEnabled())
                        iter->pModel->Request(NON_BLOCKING, "CClientModelRequestManager::DoPulse #1");
                    else
                        iter->pModel->Request(BLOCKING, "CClientModelRequestManager::DoPulse #2");
                    iter->requestTimer.Reset();
                }
                ++iter;
            }
        }
    }
    catch (...)
    {
        failure = std::current_exception();
    }

    m_bDoingPulse = false;
    for (auto iter = m_Requests.begin(); iter != m_Requests.end();)
    {
        auto current = iter++;
        if (current->canceled)
            ReleaseModel(Detach(current).pModel, failure);
    }
    if (failure)
        std::rethrow_exception(failure);
}
