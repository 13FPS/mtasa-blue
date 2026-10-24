/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reject only the four remaining abstract custom sequence operations
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include <cstdlib>

namespace animation_sequence_fixture
{
    class AnimationSequenceReceiver : public mta::game_de::CAnimBlendSequenceDE
    {
    public:
        AnimationSequenceReceiver(mta::game_de::NativeAnimationSequenceAccess& owner, const mta::game_de::NativeAnimationSequenceReferenceToken& token) noexcept
            : CAnimBlendSequenceDE(owner, token)
        {
        }

        // These methods are genuine unqualified leaves. Executing one would
        // invalidate this fixture rather than manufacture a usable factory.
        void  SetName(const char*) override { std::abort(); }
        void  SetKeyFrames(std::size_t, bool, bool, void*) override { std::abort(); }
        void* GetKeyFrame(std::size_t, std::uint32_t) override { std::abort(); }
        void  CopySequenceProperties(CAnimBlendSequenceSAInterface*) override { std::abort(); }
    };
    static_assert(!std::is_abstract_v<AnimationSequenceReceiver>);
}
