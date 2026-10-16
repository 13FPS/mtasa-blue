/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's per-player storage with backend-owned pad layouts
 *****************************************************************************/

#pragma once

#include "CRemoteDataStorage.h"
#include <cstring>
#include <type_traits>

// The accessors and member order are shared with the existing classic storage.
// Its context switch copies the entire CPadSAInterface, so the pad type remains
// a backend choice rather than imposing that x86 layout on another engine.
// CRemoteDataStorage has no virtual destructor: owners must destroy their
// concrete specialization, never delete through the borrowed SDK interface.
template <class PadStorage>
class CRemoteDataStorageImpl : public CRemoteDataStorage
{
public:
    CRemoteDataStorageImpl()
    {
        static_assert(std::is_trivially_copyable_v<PadStorage>);
        std::memset(&m_pad, 0, sizeof(PadStorage));
        m_fCameraRotation = 0.0f;
        std::memset(&m_stats, 0, sizeof(CStatsData));
        m_fGravity = 0.008f;
        m_bAkimboTargetUp = false;
        m_bProcessPlayerWeapon = false;
        m_shotSyncData.m_bRemoteBulletSyncVectorsValid = false;
    }

    CControllerState* CurrentControllerState() override { return &m_pad.NewState; }
    CControllerState* LastControllerState() override { return &m_pad.OldState; }
    CShotSyncData*    ShotSyncData() override { return &m_shotSyncData; }
    CStatsData*       Stats() override { return &m_stats; }
    void              SetCameraRotation(float fCameraRotation) override { m_fCameraRotation = fCameraRotation; }
    float             GetCameraRotation() override { return m_fCameraRotation; }
    void              SetGravity(float fGravity) override { m_fGravity = fGravity; }
    bool              ProcessPlayerWeapon() { return m_bProcessPlayerWeapon; }
    void              SetProcessPlayerWeapon(bool bProcess) override { m_bProcessPlayerWeapon = bProcess; }

    CVector& GetAkimboTarget() override { return m_vecAkimboTarget; }
    bool     GetAkimboTargetUp() override { return m_bAkimboTargetUp; }

    void SetAkimboTarget(const CVector& vecTarget) override { m_vecAkimboTarget = vecTarget; }
    void SetAkimboTargetUp(bool bUp) override { m_bAkimboTargetUp = bUp; }

    // Preserve classic public member order; hooks access these members and
    // transfer the complete backend pad independently of the SDK accessors.
    PadStorage    m_pad;
    float         m_fCameraRotation;
    CShotSyncData m_shotSyncData;
    CVector       m_vecAkimboTarget;
    bool          m_bAkimboTargetUp;
    CStatsData    m_stats;
    float         m_fGravity;
    bool          m_bProcessPlayerWeapon;
};
