/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CObjectGroupPhysicalPropertiesSA.cpp
 *  PURPOSE:     Objects dynamic physical properties handler class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CFxSystemBPSA.h"
#include "CGameSA.h"
#include "CObjectGroupPhysicalPropertiesSA.h"
#include <game/CObjectGroupPropertiesPolicy.h>

extern CGameSA* pGame;

CObjectGroupPhysicalPropertiesSAInterface* pObjectInfo = *(CObjectGroupPhysicalPropertiesSAInterface**)(0x59F857 + 6);
std::unordered_map<unsigned char, std::unique_ptr<CObjectGroupPhysicalPropertiesSAInterface>> CObjectGroupPhysicalPropertiesSA::ms_OriginalGroupProperties;

CObjectGroupPhysicalPropertiesSA::CObjectGroupPhysicalPropertiesSA() : m_pInterface(nullptr)
{
}

CObjectGroupPhysicalPropertiesSA::CObjectGroupPhysicalPropertiesSA(unsigned char ucObjectGroup) : m_ucObjectGroup(ucObjectGroup)
{
    m_pInterface = &pObjectInfo[ucObjectGroup];
    m_bModified = MapFind(ms_OriginalGroupProperties, ucObjectGroup);
}

CObjectGroupPhysicalPropertiesSAInterface* CObjectGroupPhysicalPropertiesSA::GetInterface() const
{
    return m_pInterface;
}

void CObjectGroupPhysicalPropertiesSA::SetGroup(unsigned char ucObjectGroup)
{
    m_pInterface = &pObjectInfo[ucObjectGroup];
    m_ucObjectGroup = ucObjectGroup;
    m_bModified = MapFind(ms_OriginalGroupProperties, ucObjectGroup);
}

unsigned char CObjectGroupPhysicalPropertiesSA::GetGroup() const
{
    return m_ucObjectGroup;
}

bool CObjectGroupPhysicalPropertiesSA::IsValid() const
{
    return m_pInterface != nullptr;
}

void CObjectGroupPhysicalPropertiesSA::ChangeSafeguard()
{
    MtaGame::CObjectGroupPropertiesPolicy::ChangeSafeguard(m_bModified,
                                                           [&]
                                                           {
                                                               // The modified flag belongs to this wrapper; the first original remains
                                                               // shared even after another wrapper restores the same native group.
                                                               if (!MapFind(ms_OriginalGroupProperties, m_ucObjectGroup))
                                                               {
                                                                   auto pOriginalCopy = std::make_unique<CObjectGroupPhysicalPropertiesSAInterface>();
                                                                   memcpy(pOriginalCopy.get(), m_pInterface, sizeof(CObjectGroupPhysicalPropertiesSAInterface));
                                                                   ms_OriginalGroupProperties[m_ucObjectGroup] = std::move(pOriginalCopy);
                                                               }
                                                           });
}

void CObjectGroupPhysicalPropertiesSA::RestoreDefault()
{
    MtaGame::CObjectGroupPropertiesPolicy::RestoreDefault(m_bModified,
                                                          [&]
                                                          {
                                                              auto ppOriginalCopy = MapFind(ms_OriginalGroupProperties, m_ucObjectGroup);
                                                              dassert(ppOriginalCopy);
                                                              if (!ppOriginalCopy)
                                                                  return false;
                                                              dassert(ppOriginalCopy->get());
                                                              if (!ppOriginalCopy->get())
                                                                  return false;
                                                              memcpy(m_pInterface, ppOriginalCopy->get(), sizeof(CObjectGroupPhysicalPropertiesSAInterface));
                                                              return true;
                                                          });
}

void CObjectGroupPhysicalPropertiesSA::RestoreDefaultValues()
{
    MtaGame::CObjectGroupPropertiesPolicy::RestoreAll(ms_OriginalGroupProperties,
                                                      [](unsigned char group, auto& original)
                                                      {
                                                          pGame->GetObjectGroupPhysicalProperties(group)->RestoreDefault();
                                                          original.reset();
                                                          return true;
                                                      });
}

void CObjectGroupPhysicalPropertiesSA::SetMass(float fMass)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fMass, fMass))
        return;

    ChangeSafeguard();
    m_pInterface->fMass = fMass;
}

float CObjectGroupPhysicalPropertiesSA::GetMass() const
{
    return m_pInterface->fMass;
}

void CObjectGroupPhysicalPropertiesSA::SetTurnMass(float fTurnMass)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fTurnMass, fTurnMass))
        return;

    ChangeSafeguard();
    m_pInterface->fTurnMass = fTurnMass;
}

float CObjectGroupPhysicalPropertiesSA::GetTurnMass() const
{
    return m_pInterface->fTurnMass;
}

void CObjectGroupPhysicalPropertiesSA::SetAirResistance(float fAirResistance)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fAirResistance, fAirResistance))
        return;

    ChangeSafeguard();
    m_pInterface->fAirResistance = fAirResistance;
}

float CObjectGroupPhysicalPropertiesSA::GetAirResistance() const
{
    return m_pInterface->fAirResistance;
}

void CObjectGroupPhysicalPropertiesSA::SetElasticity(float fElasticity)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fElasticity, fElasticity))
        return;

    ChangeSafeguard();
    m_pInterface->fElasticity = fElasticity;
}

float CObjectGroupPhysicalPropertiesSA::GetElasticity() const
{
    return m_pInterface->fElasticity;
}

void CObjectGroupPhysicalPropertiesSA::SetBuoyancy(float fBuoyancy)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fBuoyancy, fBuoyancy))
        return;

    ChangeSafeguard();
    m_pInterface->fBuoyancy = fBuoyancy;
}

float CObjectGroupPhysicalPropertiesSA::GetBuoyancy() const
{
    return m_pInterface->fBuoyancy;
}

void CObjectGroupPhysicalPropertiesSA::SetUprootLimit(float fUprootLimit)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fUprootLimit, fUprootLimit))
        return;

    ChangeSafeguard();
    m_pInterface->fUprootLimit = fUprootLimit;
}

float CObjectGroupPhysicalPropertiesSA::GetUprootLimit() const
{
    return m_pInterface->fUprootLimit;
}

void CObjectGroupPhysicalPropertiesSA::SetCollisionDamageMultiplier(float fColMult)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fColDamageMultiplier, fColMult))
        return;

    ChangeSafeguard();
    m_pInterface->fColDamageMultiplier = fColMult;
}

float CObjectGroupPhysicalPropertiesSA::GetCollisionDamageMultiplier() const
{
    return m_pInterface->fColDamageMultiplier;
}

void CObjectGroupPhysicalPropertiesSA::SetCollisionDamageEffect(ObjectGroupPhysicalProperties::DamageEffect eDamageEffect)
{
    // Compare the full SDK enum before its byte store so an out-of-range
    // request keeps the actual first-change/default-capture behavior.
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(static_cast<ObjectGroupPhysicalProperties::DamageEffect>(m_pInterface->eColDamageEffect),
                                                              eDamageEffect))
        return;

    ChangeSafeguard();
    m_pInterface->eColDamageEffect = (uchar)eDamageEffect;
}

ObjectGroupPhysicalProperties::DamageEffect CObjectGroupPhysicalPropertiesSA::GetCollisionDamageEffect() const
{
    return static_cast<ObjectGroupPhysicalProperties::DamageEffect>(m_pInterface->eColDamageEffect);
}

void CObjectGroupPhysicalPropertiesSA::SetCollisionSpecialResponseCase(ObjectGroupPhysicalProperties::CollisionResponse eResponseCase)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(static_cast<ObjectGroupPhysicalProperties::CollisionResponse>(m_pInterface->eSpecialColResponse),
                                                              eResponseCase))
        return;

    ChangeSafeguard();
    m_pInterface->eSpecialColResponse = (uchar)eResponseCase;
}

ObjectGroupPhysicalProperties::CollisionResponse CObjectGroupPhysicalPropertiesSA::GetCollisionSpecialResponseCase() const
{
    return static_cast<ObjectGroupPhysicalProperties::CollisionResponse>(m_pInterface->eSpecialColResponse);
}

void CObjectGroupPhysicalPropertiesSA::SetCameraAvoidObject(bool bAvoid)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(m_pInterface->bCameraAvoidObject, bAvoid))
        return;

    ChangeSafeguard();
    m_pInterface->bCameraAvoidObject = bAvoid;
}

bool CObjectGroupPhysicalPropertiesSA::GetCameraAvoidObject() const
{
    return m_pInterface->bCameraAvoidObject;
}

void CObjectGroupPhysicalPropertiesSA::SetCausesExplosion(bool bExplodes)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(m_pInterface->bCausesExplosion, bExplodes))
        return;

    ChangeSafeguard();
    m_pInterface->bCausesExplosion = bExplodes;
}

bool CObjectGroupPhysicalPropertiesSA::GetCausesExplosion() const
{
    return m_pInterface->bCausesExplosion;
}

void CObjectGroupPhysicalPropertiesSA::SetFxType(ObjectGroupPhysicalProperties::FxType eFxType)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(static_cast<ObjectGroupPhysicalProperties::FxType>(m_pInterface->eFxType), eFxType))
        return;

    ChangeSafeguard();
    m_pInterface->eFxType = (uchar)eFxType;
}

ObjectGroupPhysicalProperties::FxType CObjectGroupPhysicalPropertiesSA::GetFxType() const
{
    return static_cast<ObjectGroupPhysicalProperties::FxType>(m_pInterface->eFxType);
}

void CObjectGroupPhysicalPropertiesSA::SetFxOffset(CVector vecOffset)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(m_pInterface->vecFxOffset, vecOffset))
        return;

    ChangeSafeguard();
    m_pInterface->vecFxOffset = vecOffset;
}

CVector CObjectGroupPhysicalPropertiesSA::GetFxOffset() const
{
    return m_pInterface->vecFxOffset;
}

bool CObjectGroupPhysicalPropertiesSA::SetFxParticleSystem(CFxSystemBPSAInterface* pBlueprint)
{
    if (!pBlueprint)
        return false;

    if (pBlueprint->cPlayMode != 0)
        return false;

    if (m_pInterface->pFxSystemBlueprintPtr == pBlueprint)
        return true;

    ChangeSafeguard();
    m_pInterface->pFxSystemBlueprintPtr = pBlueprint;
    return true;
}

void CObjectGroupPhysicalPropertiesSA::RemoveFxParticleSystem()
{
    if (!m_pInterface->pFxSystemBlueprintPtr)
        return;

    ChangeSafeguard();
    m_pInterface->pFxSystemBlueprintPtr = nullptr;
}

void CObjectGroupPhysicalPropertiesSA::SetSmashMultiplier(float fMult)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fSmashMultiplier, fMult))
        return;

    ChangeSafeguard();
    m_pInterface->fSmashMultiplier = fMult;
}

float CObjectGroupPhysicalPropertiesSA::GetSmashMultiplier() const
{
    return m_pInterface->fSmashMultiplier;
}

void CObjectGroupPhysicalPropertiesSA::SetBreakVelocity(CVector vecVelocity)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(m_pInterface->vecBreakVelocity, vecVelocity))
        return;

    ChangeSafeguard();
    m_pInterface->vecBreakVelocity = vecVelocity;
}

CVector CObjectGroupPhysicalPropertiesSA::GetBreakVelocity() const
{
    return m_pInterface->vecBreakVelocity;
}

void CObjectGroupPhysicalPropertiesSA::SetBreakVelocityRandomness(float fRand)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(m_pInterface->fBreakVelocityRand, fRand))
        return;

    ChangeSafeguard();
    m_pInterface->fBreakVelocityRand = fRand;
}

float CObjectGroupPhysicalPropertiesSA::GetBreakVelocityRandomness() const
{
    return m_pInterface->fBreakVelocityRand;
}

void CObjectGroupPhysicalPropertiesSA::SetBreakMode(ObjectGroupPhysicalProperties::BreakMode eBreakMode)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(static_cast<ObjectGroupPhysicalProperties::BreakMode>(m_pInterface->eBreakMode), eBreakMode))
        return;

    ChangeSafeguard();
    m_pInterface->eBreakMode = (DWORD)eBreakMode;
}

ObjectGroupPhysicalProperties::BreakMode CObjectGroupPhysicalPropertiesSA::GetBreakMode() const
{
    return static_cast<ObjectGroupPhysicalProperties::BreakMode>(m_pInterface->eBreakMode);
}

void CObjectGroupPhysicalPropertiesSA::SetSparksOnImpact(bool bSparks)
{
    if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(static_cast<bool>(m_pInterface->dwSparksOnImpact), bSparks))
        return;

    ChangeSafeguard();
    m_pInterface->dwSparksOnImpact = static_cast<bool>(bSparks);
}

bool CObjectGroupPhysicalPropertiesSA::GetSparksOnImpact() const
{
    return static_cast<bool>(m_pInterface->dwSparksOnImpact);
}
