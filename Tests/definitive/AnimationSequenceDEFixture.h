/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Constructor-only actual sequence receiver; no operation overrides
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/CAnimBlendSequenceDE.h"

namespace animation_sequence_fixture
{
    class AnimationSequenceReceiver : public mta::game_de::CAnimBlendSequenceDE
    {
    public:
        AnimationSequenceReceiver(mta::game_de::NativeAnimationSequenceAccess& owner, const mta::game_de::NativeAnimationSequenceReferenceToken& token) noexcept
            : CAnimBlendSequenceDE(owner, token)
        {
        }
    };
    static_assert(!std::is_abstract_v<AnimationSequenceReceiver>);
}
