/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CRemoteDataSA.h
 *  PURPOSE:     Remote data storage class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <multiplayer/CRemoteDataStorageImpl.h>
#include <map>

// Storage needs the concrete classic pad, but not the module's hook setup or
// complete game facade. Keep the registry's engine identities opaque here.
#include "../game_sa/CPadSA.h"

class CPools;
class CPlayerPed;
class CPedSAInterface;

// Classic context switching still owns and copies the complete CPadSAInterface.
// Sharing its storage implementation must not replace that layout with the
// smaller canonical DE controller pair or change the SDK's ownership contract.
class CRemoteDataStorageSA : public CRemoteDataStorageImpl<CPadSAInterface>
{
};

class CRemoteDataSA
{
public:
    static void Init();

    // Static accessors
    static CRemoteDataStorageSA* GetRemoteDataStorage(CPlayerPed* pPed);
    static CRemoteDataStorageSA* GetRemoteDataStorage(CPedSAInterface* pPed);
    static void                  AddRemoteDataStorage(CPlayerPed* pPed, CRemoteDataStorage* pData);
    static void                  RemoveRemoteDataStorage(CPlayerPed* pPed);

private:
    static CPools*                                      m_pPools;
    static std::map<CPlayerPed*, CRemoteDataStorageSA*> m_RemoteData;
};
