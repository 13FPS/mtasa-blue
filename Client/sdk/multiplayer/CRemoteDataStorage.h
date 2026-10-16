/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Engine-independent per-player multiplayer state interface
 *****************************************************************************/

#pragma once

// CVector's legacy inline helpers use SharedUtil's numeric definitions. Supply
// them before that SDK header instead of depending on a caller's Core PCH.
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CControllerState.h>
#include <CVector.h>
#include <game/CStats.h>

enum class eVehicleAimDirection : unsigned char;

/**
 * This class contains information used for shot syncing, one exists per player.
 */
class CShotSyncData
{
public:
    CVector m_vecShotTarget;
    CVector m_vecShotOrigin;
    // so we can make the arm move vertically (mainly while on foot) and horizontally (mainly while in vehicles)
    float m_fArmDirectionX;
    float m_fArmDirectionY;
    // only for in-vehicle shooting
    eVehicleAimDirection m_cInVehicleAimDirection;
    // use origin
    bool m_bUseOrigin;

    bool    m_bRemoteBulletSyncVectorsValid;
    CVector m_vecRemoteBulletSyncStart;
    CVector m_vecRemoteBulletSyncEnd;
};

class CStatsData
{
public:
    float StatTypesFloat[MAX_FLOAT_STATS];
    int   StatTypesInt[MAX_INT_STATS];
    float StatReactionValue[MAX_REACTION_STATS];
};

// Keep these SDK declarations independent of the complete multiplayer facade.
// Another backend can reuse MTA's actual per-player storage without importing
// classic engine layouts or claiming that its native context switch is ready.
class CRemoteDataStorage
{
public:
    virtual class CControllerState* CurrentControllerState() = 0;
    virtual class CControllerState* LastControllerState() = 0;
    virtual class CShotSyncData*    ShotSyncData() = 0;
    virtual class CStatsData*       Stats() = 0;
    virtual float                   GetCameraRotation() = 0;
    virtual void                    SetCameraRotation(float fCameraRotation) = 0;
    virtual void                    SetGravity(float fGravity) = 0;
    virtual void                    SetProcessPlayerWeapon(bool bProcess) = 0;
    virtual CVector&                GetAkimboTarget() = 0;
    virtual bool                    GetAkimboTargetUp() = 0;
    virtual void                    SetAkimboTarget(const CVector& vecTarget) = 0;
    virtual void                    SetAkimboTargetUp(bool bUp) = 0;
};
