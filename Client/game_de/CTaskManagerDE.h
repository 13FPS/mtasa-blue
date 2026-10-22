/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the real SDK secondary-task removal without a fake task graph
 ****************************************************************************/
#pragma once
#include "NativeTaskRemovalAccess.h"
#include <game/CTaskManager.h>
#include <type_traits>

namespace mta::game_de
{
    // Task lookup, assignment, and every operation requiring a persistent CTask
    // remain pure. This slice cannot be published as a complete task manager.
    class CTaskManagerDE : public CTaskManager
    {
    public:
        using CTaskManager::RemoveTaskSecondary;
        void RemoveTaskSecondary(const int priority) override;

    protected:
        CTaskManagerDE(NativeTaskRemovalAccess& access, const NativePedReferenceToken& token) noexcept : access(access), token(token) {}

    private:
        NativeTaskRemovalAccess& access;
        // Borrow the enclosing ped's existing reference. Constructing or
        // destroying this CPU slice never allocates or deletes native tasks.
        const NativePedReferenceToken token;
    };
    static_assert(std::is_abstract_v<CTaskManagerDE> && std::is_base_of_v<CTaskManager, CTaskManagerDE>);
}
