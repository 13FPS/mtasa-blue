/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CTaskManagementSystemSA.cpp
 *  PURPOSE:     Task management system
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CGameSA.h"
#include "CTaskManagementSystemSA.h"
#include <game/CTaskWrapperPolicy.h>
#include "TaskAttackSA.h"
#include "TaskBasicSA.h"
#include "TaskCarAccessoriesSA.h"
#include "TaskCarSA.h"
#include "TaskGoToSA.h"
#include "TaskIKSA.h"
#include "TaskJumpFallSA.h"
#include "TaskPhysicalResponseSA.h"
#include "TaskSA.h"
#include "TaskSecondarySA.h"
#include "TaskSimpleSwimSA.h"

extern CGameSA* pGame;

using namespace std;

void HOOK_CTask_Operator_Delete();

CTaskManagementSystemSA::CTaskManagementSystemSA()
{
    // Install our hook used to delete our tasks when GTA does
    HookInstall(FUNC_CTask_Operator_Delete, (DWORD)HOOK_CTask_Operator_Delete, 6);
}

CTaskManagementSystemSA::~CTaskManagementSystemSA()
{
    // Delete all the entries
    list<STaskListItem*>::const_iterator iter = m_TaskList.begin();
    for (; iter != m_TaskList.end(); iter++)
    {
        delete *iter;
    }

    // Clear the list
    m_TaskList.clear();
}

CTaskSA* CTaskManagementSystemSA::AddTask(CTaskSA* pTask)
{
    if (!pTask)
        return NULL;

    assert(pTask->IsValid());
    STaskListItem* pItem = new STaskListItem;
    pItem->pTaskSA = pTask;
    pItem->taskInterface = pTask->GetInterface();
    m_TaskList.push_back(pItem);

    return pTask;
}

// Only called from HOOK_CTask_Operator_Delete
void CTaskManagementSystemSA::RemoveTask(CTaskSAInterface* pTaskInterface)
{
    // Stops crash on exit
    if (m_TaskList.size() == 0)
        return;

    assert(pTaskInterface);

    MtaGame::CTaskWrapperPolicy::RetireFirst(
        m_TaskList, pTaskInterface, [](const STaskListItem* item) { return item->taskInterface; }, [](const STaskListItem* item) { return item->pTaskSA; },
        [](STaskListItem* item) { delete item; }, [](CTaskSA* task) { task->DestroyJustThis(); });
}

CTaskSA* CTaskManagementSystemSA::GetTask(CTaskSAInterface* pTaskInterface)
{
    // Return NULL if we got passed NULL
    if (!pTaskInterface)
        return nullptr;

    // Check vtable pointer to prevent crash from corrupted task objects
    TaskVTBL* pVTBL = pTaskInterface->VTBL;
    if (!pVTBL)
        return nullptr;

    // Vtable should be in executable memory range (.text/.rdata sections)
    // GTA SA base is around 0x400000-0x900000 range
    constexpr DWORD GTA_BASE_MIN = 0x400000;
    constexpr DWORD GTA_BASE_MAX = 0x900000;
    DWORD           dwVTableAddr = reinterpret_cast<DWORD>(pVTBL);
    if (dwVTableAddr < GTA_BASE_MIN || dwVTableAddr > GTA_BASE_MAX)
        return nullptr;

    if (auto* task = MtaGame::CTaskWrapperPolicy::FindCached(
            m_TaskList, pTaskInterface, [](const STaskListItem* item) { return item->taskInterface; }, [](const STaskListItem* item) { return item->pTaskSA; }))
        return task;

    // its not existed before, lets create the task
    // First, we create a temp task
    int   iTaskType = 9999;
    DWORD dwFunc = pVTBL->GetTaskType;
    if (dwFunc && dwFunc != 0x82263A)
    {
        // clang-format off
        __asm
        {
            mov     ecx, pTaskInterface
            call    dwFunc
            mov     iTaskType, eax
        }
        // clang-format on
    }

    // Create it and add it to our list
    CTaskSA* pTask = dynamic_cast<CTaskSA*>(CreateAppropriateTask(pTaskInterface, iTaskType));
    if (pTask)
    {
        auto* pListItem = new STaskListItem;
        pListItem->pTaskSA = pTask;
        pListItem->taskInterface = pTaskInterface;
        m_TaskList.push_back(pListItem);
    }

    return pTask;
}

CTaskSA* CTaskManagementSystemSA::CreateAppropriateTask(CTaskSAInterface* pTaskInterface, int iTaskType)
{
    CTaskSA* pTaskSA = NULL;

    using Factory = MtaGame::CTaskWrapperPolicy::Factory;
    switch (MtaGame::CTaskWrapperPolicy::SelectFactory(iTaskType))
    {
        // Attack
        case Factory::GangDriveBy:
            pTaskSA = new CTaskSimpleGangDriveBySA;
            break;
        case Factory::UseGun:
            pTaskSA = new CTaskSimpleUseGunSA;
            break;
        case Factory::Fight:
            pTaskSA = new CTaskSimpleFightSA;
            break;

        // Basic
        case Factory::UseMobilePhone:
            pTaskSA = new CTaskComplexUseMobilePhoneSA;
            break;
        case Factory::RunAnim:
            pTaskSA = new CTaskSimpleRunAnimSA;
            break;
        case Factory::RunNamedAnim:
            pTaskSA = new CTaskSimpleRunNamedAnimSA;
            break;
        case Factory::Die:
            pTaskSA = new CTaskComplexDieSA;
            break;
        case Factory::StealthKill:
            pTaskSA = new CTaskSimpleStealthKillSA;
            break;
        case Factory::Sunbathe:
            pTaskSA = new CTaskComplexSunbatheSA;
            break;
        case Factory::Swim:
            pTaskSA = new CTaskSimpleSwimSA;
            break;

        // Car accessories
        case Factory::CarSetPassenger:
            pTaskSA = new CTaskSimpleCarSetPedInAsPassengerSA;
            break;
        case Factory::CarSetDriver:
            pTaskSA = new CTaskSimpleCarSetPedInAsDriverSA;
            break;
        case Factory::CarSetOut:
            pTaskSA = new CTaskSimpleCarSetPedOutSA;
            break;

        // Car
        case Factory::EnterCarDriver:
            pTaskSA = new CTaskComplexEnterCarAsDriverSA;
            break;
        case Factory::EnterCarPassenger:
            pTaskSA = new CTaskComplexEnterCarAsPassengerSA;
            break;
        case Factory::EnterBoatDriver:
            pTaskSA = new CTaskComplexEnterBoatAsDriverSA;
            break;
        case Factory::LeaveCar:
            pTaskSA = new CTaskComplexLeaveCarSA;
            break;

        // GoTo
        case Factory::WanderStandard:
            pTaskSA = new CTaskComplexWanderStandardSA;
            break;

        // IK
        case Factory::IkChain:
            pTaskSA = new CTaskSimpleIKChainSA;
            break;
        case Factory::IkLookAt:
            pTaskSA = new CTaskSimpleIKLookAtSA;
            break;
        case Factory::IkManager:
            pTaskSA = new CTaskSimpleIKManagerSA;
            break;

        // JumpFall
        case Factory::Climb:
            pTaskSA = new CTaskSimpleClimbSA;
            break;
        case Factory::JetPack:
            pTaskSA = new CTaskSimpleJetPackSA;
            break;

        // Physical response
        case Factory::Choking:
            pTaskSA = new CTaskSimpleChokingSA;
            break;

        // Secondary
        case Factory::Duck:
            pTaskSA = new CTaskSimpleDuckSA;
            break;

        // Unknown logical types retain the existing real base wrapper.
        case Factory::Base:
        default:
            pTaskSA = new CTaskSA;
            break;
    }

    assert(pTaskSA && !pTaskSA->GetInterface());

    // Set the internal interface
    pTaskSA->SetInterface(pTaskInterface);
    return pTaskSA;
}

// HOOKS
static void OnMY_Task_Operator_Delete(CTaskSAInterface* pTaskInterface)
{
    pGame->GetTaskManagementSystem()->RemoveTask(pTaskInterface);
}

static void __declspec(naked) HOOK_CTask_Operator_Delete()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        pushad
        push    [esp + 32 + 4]
        call    OnMY_Task_Operator_Delete
        add     esp, 4
        popad

        mov     eax, 0xB744A8
        mov     ecx, dword ptr [eax]
        mov     eax, FUNC_CTask_Operator_Delete
        add     eax, 6
        jmp     eax
    }
    // clang-format on
}
