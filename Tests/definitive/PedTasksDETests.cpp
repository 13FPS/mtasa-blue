/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK gang-driveby task composition without native factories
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedTasksDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <game/CPedTaskPolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
namespace ped_tasks_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified task-policy fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedTasksFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned taskChecks = 0;
    void     CheckTask(bool condition, const char* reason)
    {
        ++taskChecks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    struct TaskAccesses : NativePedVitalsAccess,
                          NativePedMetadataAccess,
                          NativePedMutationAccess,
                          NativePedObservationAccess,
                          NativePedRotationAccess,
                          NativePedOxygenAccess,
                          NativePedDuckingAccess,
                          NativePedBikeFlagsAccess,
                          NativePedShotFlagsAccess,
                          NativePedBleedingAccess,
                          NativePedStayAccess,
                          NativePedFightingStyleAccess,
                          NativePedCurrentWeaponSlotAccess,
                          NativePedPlacementAccess,
                          NativePedWaterFlagsAccess,
                          NativePedWaterTaskAccess,
                          NativePedOrientationAccess,
                          NativePedAreaAccess,
                          NativePedFireAccess,
                          NativePedCollisionAccess
    {
    };
    struct ObservedTask final : CTask
    {
        explicit ObservedTask(ped_tasks_fixture::TaskScenario& scenario) : taskScenarioValue(scenario) {}
        int      value = TASK_SIMPLE_GANG_DRIVEBY;
        unsigned reads = 0;
        bool     throwType = false;
        int      GetTaskType() override
        {
            taskScenarioValue.Record(5);
            ++reads;
            if (throwType)
                throw std::runtime_error("task type refused");
            return value;
        }
        CTask*                  Clone() override { Unqualified(); }
        void                    SetParent(CTask*) override { Unqualified(); }
        CTask*                  GetParent() override { Unqualified(); }
        CTask*                  GetSubTask() override { Unqualified(); }
        bool                    IsSimpleTask() override { Unqualified(); }
        void                    StopTimer(const CEvent*) override { Unqualified(); }
        bool                    MakeAbortable(CPed*, const int, const CEvent*) override { Unqualified(); }
        const char*             GetTaskName() override { Unqualified(); }
        CTaskSAInterface*       GetInterface() noexcept override { Unqualified(); }
        const CTaskSAInterface* GetInterface() const noexcept override { Unqualified(); }
        bool                    IsValid() override { Unqualified(); }
        void                    SetAsPedTask(CPed*, const int, const bool) override { Unqualified(); }
        void                    SetAsSecondaryPedTask(CPed*, const int) override { Unqualified(); }
        void                    Destroy() override { Unqualified(); }
        void                    DestroyJustThis() override { Unqualified(); }

    private:
        [[noreturn]] static void         Unqualified() noexcept { ped_tasks_fixture::Unqualified("Task unrelated method"); }
        ped_tasks_fixture::TaskScenario& taskScenarioValue;
    };
    struct ObservedManager final : CTaskManager
    {
        explicit ObservedManager(ped_tasks_fixture::TaskScenario& scenario) : taskScenarioValue(scenario) {}
        CTask*   selected = nullptr;
        unsigned reads = 0;
        int      priority = -1;
        bool     throwTask = false;
        CTask*   GetTask(const int selectedPriority) override
        {
            taskScenarioValue.Record(4);
            ++reads;
            priority = selectedPriority;
            if (throwTask)
                throw std::runtime_error("task lookup refused");
            return selected;
        }
        void   RemoveTask(const int) override { Unqualified(); }
        CTask* GetActiveTask() override { Unqualified(); }
        CTask* GetSimplestActiveTask() override { Unqualified(); }
        CTask* GetSimplestTask(const int) override { Unqualified(); }
        CTask* FindActiveTaskByType(const int) override { Unqualified(); }
        CTask* FindTaskByType(const int, const int) override { Unqualified(); }
        void   RemoveTaskSecondary(const int) override { Unqualified(); }
        bool   RemoveTaskSecondary(const int, const int) override { Unqualified(); }
        CTask* GetTaskSecondary(const int) override { Unqualified(); }
        bool   HasTaskSecondary(const CTask*) override { Unqualified(); }
        void   ClearTaskEventResponse() override { Unqualified(); }
        void   Flush(const int) override { Unqualified(); }

    private:
        [[noreturn]] static void         Unqualified() noexcept { ped_tasks_fixture::Unqualified("TaskManager unrelated method"); }
        ped_tasks_fixture::TaskScenario& taskScenarioValue;
    };
    struct ObservedIntelligence final : CPedIntelligence
    {
        ObservedIntelligence(ped_tasks_fixture::TaskScenario& scenario, ObservedManager& selectedManager)
            : taskScenarioValue(scenario), taskManagerValue(selectedManager)
        {
        }
        unsigned      reads = 0;
        bool          throwManager = false;
        CTaskManager* GetTaskManager() override
        {
            taskScenarioValue.Record(3);
            ++reads;
            if (throwManager)
                throw std::runtime_error("manager refused");
            return &taskManagerValue;
        }
        bool               TestForStealthKill(CPed*, bool) override { Unqualified(); }
        CTaskSAInterface*  SetTaskDuckSecondary(unsigned short) override { Unqualified(); }
        CTaskSimpleUseGun* GetTaskUseGun() override { Unqualified(); }
        CTaskSimpleFight*  GetFightTask() override { Unqualified(); }

    private:
        [[noreturn]] static void         Unqualified() noexcept { ped_tasks_fixture::Unqualified("Intelligence unrelated method"); }
        ped_tasks_fixture::TaskScenario& taskScenarioValue;
        ObservedManager&                 taskManagerValue;
    };
    bool Events(const ped_tasks_fixture::TaskScenario& scenario, std::initializer_list<int> expected)
    {
        if (scenario.eventCount != expected.size())
            return false;
        std::size_t index = 0;
        for (const int event : expected)
            if (scenario.events[index++] != event)
                return false;
        return true;
    }
}
int main()
{
    try
    {
        static_assert(TASK_PRIORITY_PRIMARY == 3);
        static_assert(std::is_abstract_v<CPedTasksDE> && std::is_base_of_v<CPedCollisionDE, CPedTasksDE>);
        unsigned refusedGetterCalls = 0;
        CheckTask(!MtaGame::CPedTaskPolicy::IsDoingGangDriveby(false,
                                                               [&]() -> CPedIntelligence*
                                                               {
                                                                   ++refusedGetterCalls;
                                                                   return nullptr;
                                                               }) &&
                      !refusedGetterCalls,
                  "shared policy checks stored availability before invoking a getter with no intelligence");
        bool          getterRefusal = false;
        volatile bool refuseGetterNow = true;
        try
        {
            MtaGame::CPedTaskPolicy::IsDoingGangDriveby(true,
                                                        [&]() -> CPedIntelligence*
                                                        {
                                                            if (refuseGetterNow)
                                                                throw std::runtime_error("getter refused");
                                                            return nullptr;
                                                        });
        }
        catch (const std::runtime_error&)
        {
            getterRefusal = true;
        }
        CheckTask(getterRefusal, "generic callback refusal propagates rather than becoming successful task absence");
        ped_tasks_fixture::Game            game;
        TaskAccesses                       access;
        ped_tasks_fixture::TaskScenario    scenario;
        ObservedTask                       task(scenario);
        ObservedManager                    manager(scenario);
        ObservedIntelligence               intelligence(scenario, manager);
        const NativePedReferenceToken      token{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        ped_tasks_fixture::PedTasksFixture ped(game, access, access, access, access, access, access, access, access, access, access, access, access, access,
                                               access, access, access, access, access, access, access, token, 0x12345678, scenario);
        const CPed&                        sdk = ped;
        CheckTask(scenario.eventCount == 0 && ped.GetType() == 0x12345678 && ped.GetCustomMoveAnim() == 0,
                  "inherited construction neither checks intelligence nor touches game/native services, preserving actual CPU bookkeeping");
        CheckTask(!sdk.IsDoingGangDriveby() && Events(scenario, {1}) && !intelligence.reads && !manager.reads && !task.reads,
                  "actual SDK checks distinct stored availability before virtual GetPedIntelligence and never queries missing services");
        scenario.available = true;
        scenario.intelligence = &intelligence;
        scenario.ResetEvents();
        CheckTask(!sdk.IsDoingGangDriveby() && Events(scenario, {1, 2, 3, 4}) && manager.priority == 3 && !task.reads,
                  "actual nullable primary task returns false with exactly one getter/manager/task lookup and no type read");
        manager.selected = &task;
        for (const int rawTaskType :
             {-1, 0, static_cast<int>(TASK_SIMPLE_GANG_DRIVEBY) - 1, static_cast<int>(TASK_SIMPLE_GANG_DRIVEBY), static_cast<int>(TASK_SIMPLE_GANG_DRIVEBY) + 1,
              NO_TASK_TYPE, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
        {
            task.value = rawTaskType;
            scenario.ResetEvents();
            const auto taskReads = task.reads;
            const auto managerReads = manager.reads;
            const auto intelligenceReads = intelligence.reads;
            CheckTask(sdk.IsDoingGangDriveby() == (rawTaskType == TASK_SIMPLE_GANG_DRIVEBY) && Events(scenario, {1, 2, 3, 4, 5}) &&
                          task.reads == taskReads + 1 && manager.reads == managerReads + 1 && intelligence.reads == intelligenceReads + 1 &&
                          manager.priority == 3,
                      "fresh actual SDK query compares one full task type against the actual gang-driveby enum at primary priority3");
        }
        for (const unsigned refusedStage : {1u, 2u, 3u})
        {
            intelligence.throwManager = refusedStage == 1;
            manager.throwTask = refusedStage == 2;
            task.throwType = refusedStage == 3;
            scenario.ResetEvents();
            bool propagated = false;
            try
            {
                (void)sdk.IsDoingGangDriveby();
            }
            catch (const std::runtime_error&)
            {
                propagated = true;
            }
            CheckTask(propagated && scenario.eventCount == refusedStage + 2 &&
                          Events(scenario, refusedStage == 1   ? std::initializer_list<int>{1, 2, 3}
                                           : refusedStage == 2 ? std::initializer_list<int>{1, 2, 3, 4}
                                                               : std::initializer_list<int>{1, 2, 3, 4, 5}),
                      "manager/task/type refusal propagates without a null fallback, second query, swallowed exception or false success");
        }
        intelligence.throwManager = manager.throwTask = task.throwType = false;
        task.value = TASK_SIMPLE_GANG_DRIVEBY;
        scenario.ResetEvents();
        CheckTask(sdk.IsDoingGangDriveby() && Events(scenario, {1, 2, 3, 4, 5}), "an earlier refused CPU query does not cache unavailable state");
        scenario.available = false;
        scenario.intelligence = nullptr;
        scenario.ResetEvents();
        CheckTask(!sdk.IsDoingGangDriveby() && Events(scenario, {1}),
                  "later stored-wrapper retirement again avoids the virtual getter and all task interfaces");
        std::puts("Actual shared ped task policy fixture completed");
        std::puts("Actual ped task SDK fixture completed");
        std::printf("Passed %u ped task policy SDK checks (64-bit; no native engine code executed)\n", taskChecks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", taskChecks, error.what());
        return 1;
    }
}
