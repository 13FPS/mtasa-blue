/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Map the actual main-camera target through existing controlled peds
 ****************************************************************************/
#pragma once
#include "CCameraWidescreenDE.h"
namespace mta::game_de
{
    // Main-camera target observation is independent of the active Cam row.
    // The supported result is qualified null or an existing same-owner ped;
    // unknown nonzero object/vehicle/native addresses explicitly refuse.
    // Existing product drain and borrowed entity lifetimes remain unchanged.
    class CCameraTargetDE : public CCameraWidescreenDE
    {
    public:
        CEntity* GetTargetEntity() override;
        ~CCameraTargetDE() noexcept = default;

    protected:
        CCameraTargetDE(CPoolsDE& pools, NativeGameTimingAccess& owner);

    private:
        static void             Require(ClockResult);
        CPoolsDE&               targetPools;
        NativeGameTimingAccess& targetOwner;
    };
    // Twenty-seven camera methods stay pure. This controlled-ped observation
    // cannot stand in for general GetEntity or a complete camera/game factory.
    static_assert(std::is_abstract_v<CCameraTargetDE> && std::is_base_of_v<CCameraWidescreenDE, CCameraTargetDE> &&
                  !std::is_copy_constructible_v<CCameraTargetDE> && !std::is_move_constructible_v<CCameraTargetDE> &&
                  !std::has_virtual_destructor_v<CCameraTargetDE>);
}
