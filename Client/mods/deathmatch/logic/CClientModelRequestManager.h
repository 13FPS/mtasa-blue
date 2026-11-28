/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientModelRequestManager.h
 *  PURPOSE:     Entity model streaming manager class
 *
 *****************************************************************************/

class CClientModelRequestManager;

#pragma once

#include "CClientCommon.h"
#include "CClientEntity.h"
#include <list>
#include <memory>

class CClientModel;
class CClientStreamElement;

struct SClientModelRequest
{
    std::shared_ptr<CClientModel> registration;
    CModelInfo*                   pModel;
    CClientEntity*                pEntity;
    CClientStreamElement*         rangeOwner = nullptr;
    CElapsedTime                  requestTimer;
    bool                          canceled = false;
};

class CClientModelRequestManager
{
    friend class CClientManager;

public:
    CClientModelRequestManager();
    ~CClientModelRequestManager();

    bool        IsLoaded(unsigned short usModelID);
    bool        IsRequested(CModelInfo* pModelInfo);
    bool        HasRequested(CClientEntity* pRequester);
    CModelInfo* GetRequestedModelInfo(CClientEntity* pRequester);

    bool RequestBlocking(unsigned short usModelID, const char* szTag);

    bool Request(unsigned short usModelID, CClientEntity* pRequester);
    bool RequestForStreamIn(unsigned short usModelID, CClientStreamElement* pRequester);
    void Cancel(CClientEntity* pRequester, bool bAllowQueue);

private:
    bool Request(unsigned short usModelID, CClientEntity* pRequester, CClientStreamElement* rangeOwner);
    void DoPulse();
    using Requests = std::list<SClientModelRequest>;
    Requests::iterator  FindRequest(CClientEntity* pRequester);
    SClientModelRequest Detach(Requests::iterator iter);

    bool     m_bDoingPulse = false;
    bool     m_restartPulse = false;
    Requests m_Requests;
};
