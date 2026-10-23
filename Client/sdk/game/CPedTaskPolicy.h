/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA ped task-query orchestration across backends
 ****************************************************************************/
#pragma once
#include "CPedIntelligence.h"
#include "CTaskManager.h"
#include "Task.h"
namespace MtaGame
{
    struct CPedTaskPolicy final
    {
        template <class GetIntelligence>
        static bool IsDoingGangDriveby(bool storedIntelligenceAvailable, GetIntelligence&& getIntelligence)
        {
            // Classic tests its owned SDK wrapper before the virtual getter.
            // A missing native service or refused call is not successful task
            // absence; the real getter/manager contract and exceptions remain.
            if (!storedIntelligenceAvailable)
                return false;
            auto* task = getIntelligence()->GetTaskManager()->GetTask(TASK_PRIORITY_PRIMARY);
            return task && task->GetTaskType() == TASK_SIMPLE_GANG_DRIVEBY;
        }
    };
}
