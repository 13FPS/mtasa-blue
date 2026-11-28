/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Query SDK task hierarchies and forward secondary-task removal
 ****************************************************************************/
#pragma once
#include "NativeTaskRemovalAccess.h"
#include <game/CTaskManager.h>
#include <type_traits>

namespace mta::game_de
{
    class CPedIntelligenceDE;
    // The task provider supplies current borrowed roots for each query.
    class CTaskManagerDE : public CTaskManager
    {
    public:
        using CTaskManager::RemoveTaskSecondary;
        CTask* GetSimplestTask(int priority) override;
        CTask* GetSimplestActiveTask() override;
        CTask* FindTaskByType(int priority, int taskType) override;
        void   RemoveTaskSecondary(const int priority) override;

    protected:
        CTaskManagerDE(NativeTaskRemovalAccess& access, const NativePedReferenceToken& token) noexcept : access(access), token(token) {}

    private:
        // The intelligence borrow may verify this exact typed owner/token
        // without publishing a raw manager address or a new task factory.
        friend class CPedIntelligenceDE;
        NativeTaskRemovalAccess& access;
        // Borrow the enclosing ped's existing reference. Constructing or
        // destroying this CPU slice never allocates or deletes native tasks.
        const NativePedReferenceToken token;
    };
    static_assert(std::is_abstract_v<CTaskManagerDE> && std::is_base_of_v<CTaskManager, CTaskManagerDE>);
}
