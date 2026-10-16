/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CSetCursorPosHook.cpp
 *  PURPOSE:     Header file for cursor position hook class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <windows.h>
#include "CSingleton.h"
#include <core/CSetCursorPosHookInterface.h>
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include "FrontendCursorLeasePolicy.h"
    #include <memory>
#endif

typedef BOOL(WINAPI* pSetCursorPos)(int X, int Y);

class CSetCursorPosHook : public CSetCursorPosHookInterface, public CSingleton<CSetCursorPosHook>
{
public:
    CSetCursorPosHook();
    ~CSetCursorPosHook();

    void ApplyHook();
    void RemoveHook();

    void DisableSetCursorPos();
    void EnableSetCursorPos();
    bool IsSetCursorPosEnabled();

    BOOL CallSetCursorPos(int X, int Y);

    static BOOL WINAPI API_SetCursorPos(int X, int Y);

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    struct FrontendLeaseState : FrontendCursorLease::State
    {
        std::uint32_t hooked = 0, lastFailure = 0, attachStatus = 0, detachStatus = 0, clipObserved = 0;
        RECT          clip{};
    };
    // These nonvirtual operations leave the classic hook interface intact.
    // The frontend must retain this owner after a failed detach and retry it.
    bool               BeginFrontendLease(HWND rootWindow) noexcept;
    bool               RenewFrontendLease() noexcept;
    void               DisarmFrontendLease() noexcept;
    bool               EndFrontendLease() noexcept;
    FrontendLeaseState ReadFrontendLease() const noexcept;
#endif

private:
    bool          m_bCanCall;
    pSetCursorPos m_pfnSetCursorPos;
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    struct FrontendImpl;
    std::unique_ptr<FrontendImpl> m_frontend;
    bool                          ChangeFrontendHook(bool install) noexcept;
#endif
};
