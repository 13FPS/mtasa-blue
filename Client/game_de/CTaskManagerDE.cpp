/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Query borrowed SDK task hierarchies and forward secondary-task removal
 ****************************************************************************/
#include "CTaskManagerDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    namespace
    {
        CTask* GetLastSubTask(CTask* task)
        {
            if (task)
            {
                while (auto* child = task->GetSubTask())
                    task = child;
            }
            return task;
        }
    }

    CTask* CTaskManagerDE::GetSimplestTask(int priority)
    {
        return GetLastSubTask(GetTask(priority));
    }

    CTask* CTaskManagerDE::GetSimplestActiveTask()
    {
        return GetLastSubTask(GetActiveTask());
    }

    CTask* CTaskManagerDE::FindTaskByType(int priority, int taskType)
    {
        for (auto* task = GetTask(priority); task; task = task->GetSubTask())
        {
            if (task->GetTaskType() == taskType)
                return task;
        }
        return nullptr;
    }

    void CTaskManagerDE::RemoveTaskSecondary(const int priority)
    {
        // Classic forwards the index directly to SetTaskSecondary(nullptr,
        // index). Preserve that intent without a local clamp or silent no-op;
        // the qualified native owner reports unsupported/invalid operations.
        const auto result = access.RemoveReferencedPedSecondaryTask(token, priority);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE task-manager owner refused RemoveTaskSecondary: " + std::to_string(static_cast<unsigned>(result)));
    }
}
