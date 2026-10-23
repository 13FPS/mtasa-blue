/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/CPools.h
 *  PURPOSE:     Game pool interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include "Common.h"
#include "CPoolClientEntity.h"
#include "CBuildingsPool.h"
#include "CDummyPool.h"
#include "CTxdPool.h"
#include "CPtrNodeSingleLinkPool.h"

class CClientEntity;
class CEntity;
class CEntitySAInterface;
class CObject;
class CPed;
class CBuilding;
class CBuildingSA;
class CVector;
class CVehicle;
struct RpClump;

enum ePools
{
    BUILDING_POOL = 0,
    PED_POOL,
    OBJECT_POOL,
    DUMMY_POOL,
    VEHICLE_POOL,
    COL_MODEL_POOL,
    TASK_POOL,
    EVENT_POOL,
    TASK_ALLOCATOR_POOL,
    PED_INTELLIGENCE_POOL,
    PED_ATTRACTOR_POOL,
    ENTRY_INFO_NODE_POOL,
    NODE_ROUTE_POOL,
    PATROL_ROUTE_POOL,
    POINT_ROUTE_POOL,
    POINTER_DOUBLE_LINK_POOL,
    POINTER_SINGLE_LINK_POOL,
    ENV_MAP_MATERIAL_POOL,
    ENV_MAP_ATOMIC_POOL,
    SPEC_MAP_MATERIAL_POOL,
    MAX_POOLS
};

// SDK links retain the real backend wrapper through its common interface.
// Every backend owns one registry; a native row or a cast of a classic
// SClientEntity specialization is never a substitute for these pointers.
class CPools
{
public:
    // Vehicles pool
    virtual CVehicle* AddVehicle(class CClientVehicle* pClientVehicle, std::uint16_t model, std::uint8_t variation, std::uint8_t variation2) noexcept = 0;
    virtual void      RemoveVehicle(CVehicle* pVehicle, bool bDelete = true) = 0;

    virtual SClientEntity<CVehicle>* GetVehicle(DWORD* pGameInterface) = 0;
    virtual SClientEntity<CVehicle>* GetVehicle(size_t pos) = 0;
    virtual unsigned long            GetVehicleCount() = 0;

    // Objects pool
    virtual CObject* AddObject(class CClientObject* pClientObject, DWORD dwModelID, bool bLowLod, bool bBreakingDisabled) = 0;
    virtual void     RemoveObject(CObject* pObject, bool bDelete = true) = 0;

    virtual SClientEntity<CObject>* GetObject(DWORD* pGameInterface) = 0;
    virtual CObject*                GetObjectFromIndex(std::uint32_t elementIndexInPool) = 0;
    virtual unsigned long           GetObjectCount() = 0;

    // Peds pool
    virtual CPed* AddPed(class CClientPed* pClientPed, unsigned int nModelIndex) = 0;
    virtual CPed* AddPed(class CClientPed* pClientPed, DWORD* pGameInterface) = 0;
    virtual void  RemovePed(CPed* pPed, bool bDelete = true) = 0;

    virtual SClientEntity<CPed>* GetPed(DWORD* pGameInterface) = 0;  // not sure we really want this here
    virtual SClientEntity<CPed>* GetPed(size_t pos) = 0;
    virtual CPed*                GetPedFromRef(DWORD dwGameRef) = 0;
    virtual unsigned long        GetPedCount() = 0;

    // Others
    virtual CVehicle* AddTrain(class CClientVehicle* pClientVehicle, const CVector& vecPosition, std::vector<DWORD> models, bool iDirection,
                               std::uint8_t ucTrackId = 255) noexcept = 0;

    virtual CEntity*       GetEntity(DWORD* pGameInterface) = 0;
    virtual CClientEntity* GetClientEntity(DWORD* pGameInterface) = 0;
    virtual uint           GetModelIdFromClump(RpClump* pRpClump) = 0;

    virtual int  GetNumberOfUsedSpaces(ePools pool) = 0;
    virtual int  GetPoolDefaultCapacity(ePools pool) = 0;
    virtual int  GetPoolDefaultModdedCapacity(ePools pool) = 0;
    virtual int  GetPoolMaxCapacity(ePools pool) const noexcept = 0;
    virtual int  GetPoolCapacity(ePools pool) = 0;
    virtual void SetPoolCapacity(ePools pool, int iValue) = 0;

    virtual void ResetPedPoolCount() = 0;
    virtual void InvalidateLocalPlayerClientEntity() = 0;

    virtual CBuildingsPool&         GetBuildingsPool() noexcept = 0;
    virtual CDummyPool&             GetDummyPool() noexcept = 0;
    virtual CTxdPool&               GetTxdPool() noexcept = 0;
    virtual CPtrNodeSingleLinkPool& GetPtrNodeSingleLinkPool() noexcept = 0;

    // Resolve the current client association of an already registered common
    // ped wrapper, without exposing a backend's native interface. This reads
    // the existing pool row, not GetStoredPointer (which has separate lifetime
    // and reassociation semantics). Appended to retain existing method slots.
    // An unregistered/null wrapper has no CPU association; native admission
    // failures remain explicit backend refusals rather than qualified absence.
    virtual CClientEntity* GetPedClientEntity(CPed* pPed) = 0;
};
