/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Constructor-only actual hierarchy test receiver; no method overrides
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"

namespace animation_hierarchy_fixture
{
    class AnimationHierarchyReceiver : public mta::game_de::CAnimBlendHierarchyDE
    {
    public:
        AnimationHierarchyReceiver(mta::game_de::NativeAnimationHierarchyAccess&               owner,
                                   const mta::game_de::NativeAnimationHierarchyReferenceToken& token) noexcept
            : CAnimBlendHierarchyDE(owner, token)
        {
        }
    };
    static_assert(!std::is_abstract_v<AnimationHierarchyReceiver>);
}
