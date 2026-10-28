/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Supply later Initialize's actual native menu-close stage
 ****************************************************************************/
#pragma once
#include "CGameRenderPreparationDE.h"
#include <type_traits>
namespace mta::game_de
{
    // Only the existing borrowed owner may invoke the actual engine close.
    // RequireInitializationReady and all unfinished CGame factory services
    // remain mandatory; completing this stage grants no complete interface.
    class CGameNativeMenuDE : public CGameRenderPreparationDE
    {
    protected:
        using CGameRenderPreparationDE::CGameRenderPreparationDE;
        void HideNativeMenu() override { RequireNativeWrite(NativeWrites().HideStartupNativeMenu(), "hide native menu"); }
    };
    static_assert(std::is_abstract_v<CGameNativeMenuDE> && std::is_base_of_v<CGame, CGameNativeMenuDE>);
}
