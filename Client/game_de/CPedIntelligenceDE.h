/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the actual same-ped SDK task manager without adopting tasks
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CPedIntelligence.h>
#include <game/CGameClockService.h>
#include <type_traits>
namespace mta::game_de
{
    class NativeGameTimingAccess;
    class CTaskManagerDE;
    // The enclosing owner supplies an already complete typed manager and
    // retains it, this intelligence and the ped reference while SDK users run.
    // Close/quiesce those users before typed retirement; no native pointer or
    // observed task word becomes a persistent SDK object or lifetime lease.
    class CPedIntelligenceDE : public CPedIntelligence
    {
    public:
        CTaskManager*      GetTaskManager() override;
        CTaskSimpleUseGun* GetTaskUseGun() override;
        CTaskSimpleFight*  GetFightTask() override;
        CPedIntelligenceDE(const CPedIntelligenceDE&) = delete;
        CPedIntelligenceDE& operator=(const CPedIntelligenceDE&) = delete;
        CPedIntelligenceDE(CPedIntelligenceDE&&) = delete;
        CPedIntelligenceDE& operator=(CPedIntelligenceDE&&) = delete;

    protected:
        CPedIntelligenceDE(NativeGameTimingAccess&, const NativePedReferenceToken&, CTaskManagerDE&);

    private:
        bool                          HasManagerBinding() const noexcept;
        static void                   Require(ClockResult, const char*);
        NativeGameTimingAccess&       native;
        const NativePedReferenceToken reference;
        CTaskManagerDE&               manager;
    };
    // Duck-task creation and the stealth-kill query still require a backend.
    static_assert(std::is_abstract_v<CPedIntelligenceDE> && std::is_base_of_v<CPedIntelligence, CPedIntelligenceDE> &&
                  !std::has_virtual_destructor_v<CPedIntelligenceDE>);
}
