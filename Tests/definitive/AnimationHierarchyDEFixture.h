/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reject the ten remaining hierarchy mutation operations in tests
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include <cstdlib>

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
        // These are unavailable contracts, not successful no-op mutations.
        void Initialize() override { std::abort(); }
        void SetName(const char*) override { std::abort(); }
        void SetSequences(CAnimBlendSequenceSAInterface*) override { std::abort(); }
        void SetNumSequences(unsigned short) override { std::abort(); }
        void SetRunningCompressed(bool) override { std::abort(); }
        void SetAnimationBlockID(int) override { std::abort(); }
        void RemoveAnimSequences() override { std::abort(); }
        void RemoveFromUncompressedCache() override { std::abort(); }
        void RemoveQuaternionFlips() override { std::abort(); }
        void CalculateTotalTime() override { std::abort(); }
    };
    static_assert(!std::is_abstract_v<AnimationHierarchyReceiver>);
}
