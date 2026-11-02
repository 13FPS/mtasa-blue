/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Assemble actual Initialize/Reset/World policies through one CGame
 ****************************************************************************/
#pragma once
#include "CGameStartupInitializationDE.h"
#include "CGameWorldDE.h"
#include "CGamePresentationPolicyDE.h"
namespace mta::game_de
{
    // All constructor routes and qualified Initialize services are inherited.
    // Fourteen unqualified Reset/World hooks remain pure; this composes their
    // real SDK policies without making native effects or factories available.
    using CGameStartupLifecycleDE = CGameWorldMixinDE<CGameResetMixinDE<CGameStartupInitializationDE>>;
    // The existing optional Core presentation borrow wraps the same game,
    // clock, prepared-service bundle, handler store and world state.
    using CGameStartupLifecyclePresentationDE = CGamePresentationPolicyMixinDE<CGameStartupLifecycleDE>;
    static_assert(std::is_abstract_v<CGameStartupLifecycleDE> && std::is_base_of_v<CGameStartupInitializationDE, CGameStartupLifecycleDE> &&
                  std::is_convertible_v<CGameStartupLifecycleDE*, CGame*>);
    static_assert(std::is_abstract_v<CGameStartupLifecyclePresentationDE> && std::is_convertible_v<CGameStartupLifecyclePresentationDE*, CGame*>);
}
