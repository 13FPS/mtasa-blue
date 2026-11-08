/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Restore actual dummy defaults before releasing their private model holds
 ****************************************************************************/
#pragma once
#include "CGameObjectGroupResetDE.h"
namespace mta::game_de
{
    class CGameModelDummiesResetDE : public CGameObjectGroupResetDE
    {
    protected:
        using CGameObjectGroupResetDE::CGameObjectGroupResetDE;
        void ResetVehicleModelDummies() override;
    };
    // A real reset dependency does not qualify the remaining services, world
    // lifecycle or complete CGame factory. Preserve that mandatory boundary.
    static_assert(std::is_abstract_v<CGameModelDummiesResetDE> && std::is_convertible_v<CGameModelDummiesResetDE*, CGame*>);
}
