/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Supply Reset's genuine original building-capacity dependency only
 ****************************************************************************/
#pragma once
#include "CGameStartupLifecycleDE.h"
namespace mta::game_de
{
    class CGameBuildingPoolDefaultDE : public CGameStartupLifecycleDE
    {
    protected:
        // Name the actual selected base's constructors through the lifecycle
        // alias, preserving every existing protected preparation route.
        using CGameWorldMixinDE<CGameResetMixinDE<CGameStartupInitializationDE>>::CGameWorldMixinDE;
        int GetResetBuildingPoolCapacity() const override;
    };
    // This closes one dependency, not Reset readiness: thirteen native hooks,
    // unfinished services and the complete CGame factory remain unavailable.
    static_assert(std::is_abstract_v<CGameBuildingPoolDefaultDE> && std::is_base_of_v<CGameStartupLifecycleDE, CGameBuildingPoolDefaultDE> &&
                  std::is_convertible_v<CGameBuildingPoolDefaultDE*, CGame*>);
}
