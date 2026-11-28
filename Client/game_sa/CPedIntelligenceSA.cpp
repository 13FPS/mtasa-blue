/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CPedIntelligenceSA.cpp
 *  PURPOSE:     Ped entity AI logic
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CPedIntelligenceSA.h"
#include "CPedSA.h"
#include "CTaskManagementSystemSA.h"
#include "CTaskManagerSA.h"
#include <game/CPedTaskPolicy.h>
#include <game/TaskAttack.h>

CPedIntelligenceSA::CPedIntelligenceSA(CPedIntelligenceSAInterface* pedIntelligenceSAInterface, CPed* ped)
{
    this->ped = ped;
    internalInterface = pedIntelligenceSAInterface;
    CTaskManagerSAInterface* pTaskManagerInterface = (CTaskManagerSAInterface*)&(pedIntelligenceSAInterface->taskManager);
    TaskManager = new CTaskManagerSA(pTaskManagerInterface, ped);
}

CPedIntelligenceSA::~CPedIntelligenceSA()
{
    delete TaskManager;
}

CTaskManager* CPedIntelligenceSA::GetTaskManager()
{
    return TaskManager;
}

bool CPedIntelligenceSA::TestForStealthKill(CPed* pPed, bool bUnk)
{
    bool  bReturn;
    DWORD dwThis = (DWORD)internalInterface;
    DWORD dwPed = (DWORD)pPed->GetInterface();
    DWORD dwFunc = FUNC_CPedIntelligence_TestForStealthKill;
    // clang-format off
    __asm
    {
        mov     ecx, dwThis
        push    bUnk
        push    dwPed
        call    dwFunc
        mov     bReturn, al
    }
    // clang-format on
    return bReturn;
}

CTaskSAInterface* CPedIntelligenceSA::SetTaskDuckSecondary(unsigned short nLengthOfDuck)
{
    auto SetTaskDuckSecondary = (CTaskSAInterface * (__thiscall*)(CPedIntelligenceSAInterface*, unsigned short))0x601230;
    return SetTaskDuckSecondary(internalInterface, nLengthOfDuck);
}

CTaskSimpleUseGun* CPedIntelligenceSA::GetTaskUseGun()
{
    return MtaGame::CPedTaskPolicy::GetAttackTask<CTaskSimpleUseGun>(GetTaskManager(), TASK_SIMPLE_USE_GUN);
}

CTaskSimpleFight* CPedIntelligenceSA::GetFightTask()
{
    return MtaGame::CPedTaskPolicy::GetAttackTask<CTaskSimpleFight>(GetTaskManager(), TASK_SIMPLE_FIGHT);
}
