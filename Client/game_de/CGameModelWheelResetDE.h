/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route MTA Reset's wheel restoration through the same native model owner
 ****************************************************************************/
#pragma once
#include "CGameBuildingPoolDefaultDE.h"
namespace mta::game_de
{
    class CGameModelWheelResetDE : public CGameBuildingPoolDefaultDE
    {
    protected:
        using CGameBuildingPoolDefaultDE::CGameBuildingPoolDefaultDE;
        void ResetVehicleModelWheelSizes() override;
    };
    // This supplies one real restoration effect. The remaining Reset/world
    // prerequisites and complete CGame factory must still be supplied before
    // this object can be constructed or the full lifecycle can be admitted.
    static_assert(std::is_abstract_v<CGameModelWheelResetDE> && std::is_convertible_v<CGameModelWheelResetDE*, CGame*>);
}
